/*
 * Copyright (c) 2023, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "WebContentService.h"
#include "LadybirdServiceBase.h"
#include <AK/LexicalPath.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/EventLoop.h>
#include <LibCore/LocalServer.h>
#include <LibCore/System.h>
#include <LibIPC/ConnectionFromClient.h>
#include <LibImageDecoderClient/Client.h>
#include <LibJS/Bytecode/Interpreter.h>
#include <LibMedia/Audio/Loader.h>
#include <LibRequests/RequestClient.h>
#include <LibWeb/Bindings/MainThreadVM.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/Loader/ContentFilter.h>
#include <LibWeb/Loader/GeneratedPagesLoader.h>
#include <LibWeb/Loader/ResourceLoader.h>
#include <LibWeb/PermissionsPolicy/AutoplayAllowlist.h>
#include <LibWeb/Platform/AudioCodecPluginAgnostic.h>
#include <LibWeb/Platform/EventLoopPluginSerenity.h>
#include <LibWebView/HelperProcess.h>
#include <LibWebView/Plugins/FontPlugin.h>
#include <LibWebView/Plugins/ImageCodecPlugin.h>
#include <LibWebView/SiteIsolation.h>
#include <LibWebView/Utilities.h>
#include <WebContent/ConnectionFromClient.h>
#include <WebContent/PageHost.h>

static ErrorOr<NonnullRefPtr<Requests::RequestClient>> bind_request_server_service()
{
    return bind_service<Requests::RequestClient>(&bind_request_server_java);
}

static ErrorOr<NonnullRefPtr<ImageDecoderClient::Client>> bind_image_decoder_service()
{
    return bind_service<ImageDecoderClient::Client>(&bind_image_decoder_java);
}

static ErrorOr<void> load_content_filters();

static ErrorOr<void> load_autoplay_allowlist();

ErrorOr<int> service_main(int ipc_socket)
{
    Core::EventLoop event_loop;

    Web::Platform::EventLoopPlugin::install(*new Web::Platform::EventLoopPluginSerenity);

    auto image_decoder_client = TRY(bind_image_decoder_service());
    Web::Platform::ImageCodecPlugin::install(*new WebView::ImageCodecPlugin(move(image_decoder_client)));

    Web::Platform::AudioCodecPlugin::install_creation_hook([](auto loader) {
        return Web::Platform::AudioCodecPluginAgnostic::create(move(loader));
    });

    Web::Bindings::initialize_main_thread_vm(Web::Bindings::AgentType::SimilarOriginWindow);

    auto request_server_client = TRY(bind_request_server_service());
    Web::ResourceLoader::initialize(Web::Bindings::main_thread_vm().heap(), move(request_server_client));

    bool is_layout_test_mode = false;

    Web::HTML::Window::set_internals_object_exposed(is_layout_test_mode);
    Web::Platform::FontPlugin::install(*new WebView::FontPlugin(is_layout_test_mode));

    // Currently site isolation doesn't work on Android since everything is running
    // in the same process. It would require an entire redesign of this port
    // in order to make it work. For now, it's better to just disable it.
    WebView::disable_site_isolation();

    auto maybe_content_filter_error = load_content_filters();
    if (maybe_content_filter_error.is_error())
        dbgln("Failed to load content filters: {}", maybe_content_filter_error.error());

    auto maybe_autoplay_allowlist_error = load_autoplay_allowlist();
    if (maybe_autoplay_allowlist_error.is_error())
        dbgln("Failed to load autoplay allowlist: {}", maybe_autoplay_allowlist_error.error());

    auto webcontent_socket = TRY(Core::LocalSocket::adopt_fd(ipc_socket));
    auto webcontent_client = TRY(WebContent::ConnectionFromClient::try_create(make<IPC::Transport>(move(webcontent_socket))));

    return event_loop.exec();
}

template<typename Client>
ErrorOr<NonnullRefPtr<Client>> bind_service(void (*bind_method)(int))
{
    int socket_fds[2] {};
    TRY(Core::System::socketpair(AF_LOCAL, SOCK_STREAM, 0, socket_fds));

    int ui_fd = socket_fds[0];
    int server_fd = socket_fds[1];

    // NOTE: The java object takes ownership of the socket fds
    (*bind_method)(server_fd);

    auto socket = TRY(Core::LocalSocket::adopt_fd(ui_fd));
    TRY(socket->set_blocking(true));

    auto new_client = TRY(try_make_ref_counted<Client>(make<IPC::Transport>(move(socket))));

    return new_client;
}

static ErrorOr<void> load_content_filters()
{
    auto file_or_error = Core::File::open(ByteString::formatted("{}/res/ladybird/default-config/BrowserContentFilters.txt", WebView::s_ladybird_resource_root), Core::File::OpenMode::Read);
    if (file_or_error.is_error())
        return file_or_error.release_error();

    auto file = file_or_error.release_value();
    auto ad_filter_list = TRY(Core::InputBufferedFile::create(move(file)));
    auto buffer = TRY(ByteBuffer::create_uninitialized(4096));

    Vector<String> patterns;

    while (TRY(ad_filter_list->can_read_line())) {
        auto line = TRY(ad_filter_list->read_line(buffer));
        if (line.is_empty())
            continue;

        auto pattern = TRY(String::from_utf8(line));
        TRY(patterns.try_append(move(pattern)));
    }

    auto& content_filter = Web::ContentFilter::the();
    TRY(content_filter.set_patterns(patterns));

    return {};
}

static ErrorOr<void> load_autoplay_allowlist()
{
    auto file_or_error = Core::File::open(TRY(String::formatted("{}/res/ladybird/default-config/BrowserAutoplayAllowlist.txt", WebView::s_ladybird_resource_root)), Core::File::OpenMode::Read);
    if (file_or_error.is_error())
        return file_or_error.release_error();

    auto file = file_or_error.release_value();
    auto allowlist = TRY(Core::InputBufferedFile::create(move(file)));
    auto buffer = TRY(ByteBuffer::create_uninitialized(4096));

    Vector<String> origins;

    while (TRY(allowlist->can_read_line())) {
        auto line = TRY(allowlist->read_line(buffer));
        if (line.is_empty())
            continue;

        auto domain = TRY(String::from_utf8(line));
        TRY(origins.try_append(move(domain)));
    }

    auto& autoplay_allowlist = Web::PermissionsPolicy::AutoplayAllowlist::the();
    autoplay_allowlist.enable_for_origins(origins);

    return {};
}
