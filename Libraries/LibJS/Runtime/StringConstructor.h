/*
 * Copyright (c) 2020, Andreas Kling <andreas@ladybird.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibJS/Runtime/NativeFunction.h>

namespace JS {

class JS_API StringConstructor final : public NativeFunction {
    JS_OBJECT(StringConstructor, NativeFunction);
    GC_DECLARE_ALLOCATOR(StringConstructor);

public:
    virtual void initialize(Realm&) override;
    virtual ~StringConstructor() override = default;

    virtual ThrowCompletionOr<Value> call() override;
    virtual ThrowCompletionOr<GC::Ref<Object>> construct(FunctionObject& new_target) override;

private:
    explicit StringConstructor(Realm&);

    virtual bool has_constructor() const override { return true; }

    JS_DECLARE_NATIVE_FUNCTION(raw);
    JS_DECLARE_NATIVE_FUNCTION(from_char_code);
    JS_DECLARE_NATIVE_FUNCTION(from_code_point);
};

}
