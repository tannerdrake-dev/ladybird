/*
 * Copyright (c) 2018-2022, Andreas Kling <andreas@ladybird.org>
 * Copyright (c) 2020-2023, the SerenityOS developers.
 * Copyright (c) 2021-2024, Sam Atkins <atkinssj@serenityos.org>
 * Copyright (c) 2021, Tobias Christiansen <tobyase@serenityos.org>
 * Copyright (c) 2022, MacDue <macdue@dueutil.tech>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/CSS/CSSMediaRule.h>
#include <LibWeb/CSS/CSSRuleList.h>
#include <LibWeb/CSS/CSSStyleSheet.h>
#include <LibWeb/CSS/Parser/Parser.h>

namespace Web {

CSS::CSSStyleSheet* parse_css_stylesheet(CSS::Parser::ParsingParams const& context, StringView css, Optional<URL::URL> location)
{
    if (css.is_empty()) {
        auto rule_list = CSS::CSSRuleList::create_empty(*context.realm);
        auto media_list = CSS::MediaList::create(*context.realm, {});
        auto style_sheet = CSS::CSSStyleSheet::create(*context.realm, rule_list, media_list, location);
        style_sheet->set_source_text({});
        return style_sheet;
    }
    auto* style_sheet = CSS::Parser::Parser::create(context, css).parse_as_css_stylesheet(location);
    // FIXME: Avoid this copy
    style_sheet->set_source_text(MUST(String::from_utf8(css)));
    return style_sheet;
}

CSS::Parser::Parser::PropertiesAndCustomProperties parse_css_style_attribute(CSS::Parser::ParsingParams const& context, StringView css)
{
    if (css.is_empty())
        return {};
    return CSS::Parser::Parser::create(context, css).parse_as_style_attribute();
}

RefPtr<CSS::CSSStyleValue> parse_css_value(CSS::Parser::ParsingParams const& context, StringView string, CSS::PropertyID property_id)
{
    if (string.is_empty())
        return nullptr;
    return CSS::Parser::Parser::create(context, string).parse_as_css_value(property_id);
}

CSS::CSSRule* parse_css_rule(CSS::Parser::ParsingParams const& context, StringView css_text)
{
    return CSS::Parser::Parser::create(context, css_text).parse_as_css_rule();
}

Optional<CSS::SelectorList> parse_selector(CSS::Parser::ParsingParams const& context, StringView selector_text)
{
    return CSS::Parser::Parser::create(context, selector_text).parse_as_selector();
}

Optional<CSS::SelectorList> parse_selector_for_nested_style_rule(CSS::Parser::ParsingParams const& context, StringView selector_text)
{
    auto parser = CSS::Parser::Parser::create(context, selector_text);

    auto maybe_selectors = parser.parse_as_relative_selector(CSS::Parser::Parser::SelectorParsingMode::Standard);
    if (!maybe_selectors.has_value())
        return {};

    return adapt_nested_relative_selector_list(*maybe_selectors);
}

Optional<CSS::Selector::PseudoElementSelector> parse_pseudo_element_selector(CSS::Parser::ParsingParams const& context, StringView selector_text)
{
    return CSS::Parser::Parser::create(context, selector_text).parse_as_pseudo_element_selector();
}

RefPtr<CSS::MediaQuery> parse_media_query(CSS::Parser::ParsingParams const& context, StringView string)
{
    return CSS::Parser::Parser::create(context, string).parse_as_media_query();
}

Vector<NonnullRefPtr<CSS::MediaQuery>> parse_media_query_list(CSS::Parser::ParsingParams const& context, StringView string)
{
    return CSS::Parser::Parser::create(context, string).parse_as_media_query_list();
}

RefPtr<CSS::Supports> parse_css_supports(CSS::Parser::ParsingParams const& context, StringView string)
{
    if (string.is_empty())
        return {};
    return CSS::Parser::Parser::create(context, string).parse_as_supports();
}

}
