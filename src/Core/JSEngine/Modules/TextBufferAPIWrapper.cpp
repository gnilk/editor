
#include "Core/Editor.h"
#include "Core/Config/Config.h"
#include "dukglue/dukglue.h"

#include "TextBufferAPIWrapper.h"

using namespace gedit;

void TextBufferAPIWrapper::RegisterModule(duk_context *ctx) {
    dukglue_register_constructor_managed<TextBufferAPIWrapper>(ctx, "TextBuffer");
    dukglue_register_delete<TextBufferAPIWrapper>(ctx);
    dukglue_register_method(ctx, &TextBufferAPIWrapper::SetLanguage, "SetLanguage");
    dukglue_register_method(ctx, &TextBufferAPIWrapper::GetName, "GetName");
}

//
// Impl. API
//
void TextBufferAPIWrapper::SetLanguage(const char *param) {
    if (textBuffer == nullptr) {
        return;
    }
    printf("TextBufferAPIWrapper::SetLanguage, param=%s\n", param);
    textBuffer->SetLanguage(param);
}
const std::string &TextBufferAPIWrapper::GetName() {
    if (textBuffer == nullptr) {
        static const char *dummy = "";
        return dummy;
    }
    return textBuffer->GetName();
}
