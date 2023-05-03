
#include "Core/Editor.h"
#include "Core/Config/Config.h"
#include "dukglue/dukglue.h"

#include "TextBufferAPIWrapper.h"

using namespace gedit;

void TextBufferAPIWrapper::RegisterModule(duk_context *ctx) {
    dukglue_register_constructor<TextBufferAPIWrapper>(ctx, "TextBuffer");
    dukglue_register_method(ctx, &TextBufferAPIWrapper::SetLanguage, "SetLanguage");
}

//
// Impl. API
//
void TextBufferAPIWrapper::SetLanguage(const char *param) {
    printf("TextBufferAPIWrapper::SetLanguage, param=%s\n", param);
    textBuffer->SetLanguage(param);
}
