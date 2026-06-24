//
// Created by gnilk on 24.06.24.
//

#include "dukglue/dukglue.h"

#include "Core/Editor.h"
#include "Core/API/TerminalAPI.h"
#include "Core/UnicodeHelper.h"

#include "TerminalAPIWrapper.h"

using namespace gedit;

// Push one block info as a JS object { id, command, exitCode|null, lineCount } onto the stack top.
static void PushBlockInfo(duk_context *ctx, const TerminalAPI::BlockInfo &info) {
    duk_idx_t obj = duk_push_object(ctx);

    duk_push_number(ctx, static_cast<double>(info.id));
    duk_put_prop_string(ctx, obj, "id");

    auto cmd8 = UnicodeHelper::utf32to8(info.command);
    duk_push_string(ctx, cmd8.c_str());
    duk_put_prop_string(ctx, obj, "command");

    if (info.exitCode.has_value()) {
        duk_push_int(ctx, info.exitCode.value());
    } else {
        duk_push_null(ctx);
    }
    duk_put_prop_string(ctx, obj, "exitCode");

    duk_push_number(ctx, static_cast<double>(info.lineCount));
    duk_put_prop_string(ctx, obj, "lineCount");
}

void TerminalAPIWrapper::RegisterModule(duk_context *ctx) {
    static TerminalAPIWrapper terminalApiWrapper;

    dukglue_push(ctx, &terminalApiWrapper);
    duk_put_global_string(ctx, "Terminal");

    dukglue_register_method_varargs(ctx, &TerminalAPIWrapper::GetBlocks, "GetBlocks");
    dukglue_register_method_varargs(ctx, &TerminalAPIWrapper::GetBlockOutput, "GetBlockOutput");
    dukglue_register_method_varargs(ctx, &TerminalAPIWrapper::GetLastBlock, "GetLastBlock");
    dukglue_register_method_varargs(ctx, &TerminalAPIWrapper::GetSelectedBlock, "GetSelectedBlock");
    dukglue_register_method_varargs(ctx, &TerminalAPIWrapper::OpenBlockAsDocument, "OpenBlockAsDocument");
}

duk_ret_t TerminalAPIWrapper::GetBlocks(duk_context *ctx) {
    auto terminalApi = Editor::Instance().GetGlobalAPIObject<TerminalAPI>();
    if (terminalApi == nullptr) {
        duk_push_array(ctx);
        return 1;
    }
    auto blocks = terminalApi->GetBlocks();
    duk_idx_t arr = duk_push_array(ctx);
    duk_uarridx_t idx = 0;
    for (const auto &info : blocks) {
        PushBlockInfo(ctx, info);
        duk_put_prop_index(ctx, arr, idx++);
    }
    return 1;
}

duk_ret_t TerminalAPIWrapper::GetBlockOutput(duk_context *ctx) {
    auto terminalApi = Editor::Instance().GetGlobalAPIObject<TerminalAPI>();
    if (terminalApi == nullptr) {
        duk_push_null(ctx);
        return 1;
    }
    auto blockId = static_cast<uint64_t>(duk_get_number(ctx, 0));
    auto text = terminalApi->GetBlockOutput(blockId);
    if (!text.has_value()) {
        duk_push_null(ctx);
        return 1;
    }
    auto utf8 = UnicodeHelper::utf32to8(text.value());
    duk_push_string(ctx, utf8.c_str());
    return 1;
}

duk_ret_t TerminalAPIWrapper::GetLastBlock(duk_context *ctx) {
    auto terminalApi = Editor::Instance().GetGlobalAPIObject<TerminalAPI>();
    if (terminalApi == nullptr) {
        duk_push_null(ctx);
        return 1;
    }
    auto block = terminalApi->GetLastBlock();
    if (!block.has_value()) {
        duk_push_null(ctx);
        return 1;
    }
    PushBlockInfo(ctx, block.value());
    return 1;
}

duk_ret_t TerminalAPIWrapper::GetSelectedBlock(duk_context *ctx) {
    auto terminalApi = Editor::Instance().GetGlobalAPIObject<TerminalAPI>();
    if (terminalApi == nullptr) {
        duk_push_null(ctx);
        return 1;
    }
    auto text = terminalApi->GetSelectedBlock();
    if (!text.has_value()) {
        duk_push_null(ctx);
        return 1;
    }
    auto utf8 = UnicodeHelper::utf32to8(text.value());
    duk_push_string(ctx, utf8.c_str());
    return 1;
}

duk_ret_t TerminalAPIWrapper::OpenBlockAsDocument(duk_context *ctx) {
    auto terminalApi = Editor::Instance().GetGlobalAPIObject<TerminalAPI>();
    if (terminalApi == nullptr) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    auto blockId = static_cast<uint64_t>(duk_get_number(ctx, 0));
    auto document = terminalApi->OpenBlockAsDocument(blockId);
    duk_push_boolean(ctx, document != nullptr);
    return 1;
}
