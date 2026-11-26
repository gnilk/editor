//
// Created by gnilk on 24.06.24.
//

#include "dukglue/dukglue.h"

#include "Core/Editor.h"
#include "Core/API/TerminalAPI.h"
#include "Core/UnicodeHelper.h"

#include "TerminalAPIWrapper.h"

using namespace gedit;

void TerminalAPIWrapper::RegisterModule(duk_context *ctx) {
    static TerminalAPIWrapper terminalApiWrapper;

    dukglue_push(ctx, &terminalApiWrapper);
    duk_put_global_string(ctx, "Terminal");

    dukglue_register_method_varargs(ctx, &TerminalAPIWrapper::GetLastOutput, "GetLastOutput");
    dukglue_register_method_varargs(ctx, &TerminalAPIWrapper::OpenLastOutputAsDocument, "OpenLastOutputAsDocument");
    dukglue_register_method_varargs(ctx, &TerminalAPIWrapper::CopyLastOutputToClipboard, "CopyLastOutputToClipboard");
    dukglue_register_method_varargs(ctx, &TerminalAPIWrapper::GetMessages, "GetMessages");
    dukglue_register_method_varargs(ctx, &TerminalAPIWrapper::GetLayout, "GetLayout");
    dukglue_register_method_varargs(ctx, &TerminalAPIWrapper::SetLayout, "SetLayout");
    dukglue_register_method_varargs(ctx, &TerminalAPIWrapper::MoveTab, "MoveTab");
}

duk_ret_t TerminalAPIWrapper::GetLayout(duk_context *ctx) {
    auto terminalApi = Editor::Instance().GetGlobalAPIObject<TerminalAPI>();
    auto layout = (terminalApi != nullptr) ? terminalApi->GetLayout() : std::string();
    duk_push_string(ctx, layout.c_str());
    return 1;
}

duk_ret_t TerminalAPIWrapper::SetLayout(duk_context *ctx) {
    auto terminalApi = Editor::Instance().GetGlobalAPIObject<TerminalAPI>();
    auto setting = (duk_get_top(ctx) > 0) && duk_is_string(ctx, 0) ? std::string(duk_get_string(ctx, 0)) : std::string();
    duk_push_boolean(ctx, (terminalApi != nullptr) && terminalApi->SetLayout(setting));
    return 1;
}

duk_ret_t TerminalAPIWrapper::MoveTab(duk_context *ctx) {
    auto terminalApi = Editor::Instance().GetGlobalAPIObject<TerminalAPI>();
    auto message = (terminalApi != nullptr) ? terminalApi->MoveTab() : std::string("No panels");
    duk_push_string(ctx, message.c_str());
    return 1;
}

duk_ret_t TerminalAPIWrapper::GetLastOutput(duk_context *ctx) {
    auto terminalApi = Editor::Instance().GetGlobalAPIObject<TerminalAPI>();
    auto text = (terminalApi != nullptr) ? terminalApi->GetLastOutput() : std::nullopt;
    if (!text.has_value()) {
        duk_push_null(ctx);
        return 1;
    }
    auto utf8 = UnicodeHelper::utf32to8(text.value());
    duk_push_string(ctx, utf8.c_str());
    return 1;
}

duk_ret_t TerminalAPIWrapper::OpenLastOutputAsDocument(duk_context *ctx) {
    auto terminalApi = Editor::Instance().GetGlobalAPIObject<TerminalAPI>();
    duk_push_boolean(ctx, (terminalApi != nullptr) && (terminalApi->OpenLastOutputAsDocument() != nullptr));
    return 1;
}

duk_ret_t TerminalAPIWrapper::CopyLastOutputToClipboard(duk_context *ctx) {
    auto terminalApi = Editor::Instance().GetGlobalAPIObject<TerminalAPI>();
    duk_push_boolean(ctx, (terminalApi != nullptr) && terminalApi->CopyLastOutputToClipboard());
    return 1;
}

duk_ret_t TerminalAPIWrapper::GetMessages(duk_context *ctx) {
    auto terminalApi = Editor::Instance().GetGlobalAPIObject<TerminalAPI>();
    auto messages = (terminalApi != nullptr) ? terminalApi->GetBuildMessages() : std::nullopt;
    if (!messages.has_value()) {
        duk_push_null(ctx);
        return 1;
    }
    auto idxArray = duk_push_array(ctx);
    duk_uarridx_t idxItem = 0;
    for (auto &item : messages.value()) {
        auto &message = item.message;
        duk_push_object(ctx);
        duk_push_string(ctx, message.IsError() ? "error" : "warning");
        duk_put_prop_string(ctx, -2, "severity");
        duk_push_string(ctx, message.path.c_str());
        duk_put_prop_string(ctx, -2, "file");
        duk_push_string(ctx, item.resolvedPath.c_str());
        duk_put_prop_string(ctx, -2, "path");
        duk_push_uint(ctx, message.line);
        duk_put_prop_string(ctx, -2, "line");
        duk_push_uint(ctx, message.column);
        duk_put_prop_string(ctx, -2, "column");
        duk_push_string(ctx, message.message.c_str());
        duk_put_prop_string(ctx, -2, "message");
        duk_put_prop_index(ctx, idxArray, idxItem++);
    }
    return 1;
}
