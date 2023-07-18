
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
    dukglue_register_method(ctx, &TextBufferAPIWrapper::SaveBuffer, "SaveBuffer");
    dukglue_register_method(ctx, &TextBufferAPIWrapper::HasFileName, "HasFileName");
    dukglue_register_method(ctx, &TextBufferAPIWrapper::SetFileName, "SetFileName");
    dukglue_register_method(ctx, &TextBufferAPIWrapper::GetFileName, "GetFileName");

}

//
// Impl. API
//
void TextBufferAPIWrapper::SetLanguage(const char *param) {
    if (textBuffer == nullptr) {
        return;
    }
    textBuffer->SetLanguage(param);
}

const std::string TextBufferAPIWrapper::GetFileName() {
    if (textBuffer == nullptr) {
        static std::string dummy = "";
        return dummy.c_str();
    }
    return textBuffer->GetPathName();
}

const std::string TextBufferAPIWrapper::GetName() {
    if (textBuffer == nullptr) {
        static std::string dummy = "";
        return dummy;
    }
    return textBuffer->GetName();
}

bool TextBufferAPIWrapper::HasFileName() {
    return textBuffer->HasFileName();
}

void TextBufferAPIWrapper::SetFileName(const char *newFileName) {
    textBuffer->SetFileName(newFileName);
}

bool TextBufferAPIWrapper::SaveBuffer() {
    return textBuffer->SaveBuffer();
}
