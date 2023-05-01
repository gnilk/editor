
#include "dukglue/dukglue.h"

#include "TextBufferAPIWrapper.h"

void TextBufferAPIWrapper::RegisterModule(duk_context *ctx) {
    dukglue_register_constructor<TextBufferAPIWrapper>(ctx, "TextBuffer");
    dukglue_register_method(ctx, &TextBufferAPIWrapper::SetLanguage, "SetLanguage");
}

//
// Impl. API
//

void TextBufferAPIWrapper::SetLanguage(int param) {
    printf("TextBufferAPIWrapper::SetLanguage, param=%d\n", param);
}
