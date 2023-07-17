//
// Created by gnilk on 04.05.23.
//

#include "dukglue/dukglue.h"

#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"

#include "ConsoleAPIWrapper.h"

using namespace gedit;

void ConsoleAPIWrapper::RegisterModule(duk_context *ctx) {
    static ConsoleAPIWrapper consoleApiWrapper;

    dukglue_push(ctx, &consoleApiWrapper);
    duk_put_global_string(ctx, "Console");

    dukglue_register_method_varargs(ctx,&ConsoleAPIWrapper::WriteLine, "WriteLine");
    dukglue_register_method_varargs(ctx,&ConsoleAPIWrapper::WriteLine, "log");
}


// Do variadic argument printing - this is pretty much lifted directly from duktape/extras/console
// I do understand (to some extent) what it is doing, but I wouldn't have been able to write it myself...
duk_ret_t ConsoleAPIWrapper::WriteLine(duk_context *ctx) {
    //duk_uint_t flags = (duk_uint_t) duk_get_current_magic(ctx);

    duk_idx_t n = duk_get_top(ctx);
    duk_idx_t i;

    for (i = 0; i < n; i++) {
        if (duk_check_type_mask(ctx, i, DUK_TYPE_MASK_OBJECT)) {
            /* Slow path formatting. */
            duk_dup(ctx, -1);  /* console.format */
            duk_dup(ctx, i);
            duk_call(ctx, 1);
            duk_replace(ctx, i);  /* arg[i] = console.format(arg[i]); */
        }
    }

    duk_push_string(ctx, " ");
    duk_insert(ctx, 0);
    duk_join(ctx, n);

    // NOTE: This should NOT go here!
    auto cstr = duk_to_string(ctx, -1);
    SendToConsole(cstr);
//    fprintf(stdout, "%s\n", duk_to_string(ctx, -1));
//    fflush(stdout);
    return 0;
}

void ConsoleAPIWrapper::SendToConsole(const char *str) {
    auto console = RuntimeConfig::Instance().OutputConsole();
    console->WriteLine(str);
}

