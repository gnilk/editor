//
// Created by gnilk on 03.05.23.
//

#include "Core/Config/Config.h"
#include "TextBufferAPI.h"

using namespace gedit;

void TextBufferAPI::SetLanguage(const char *param) {
    if (textBuffer == nullptr) {
        return;
    }
    printf("TextBufferAPI::SetLanguage, param=%s\n", param);
    auto lang = Config::Instance().GetLanguageForExtension(param);
    textBuffer->SetLanguage(lang);
    textBuffer->Reparse();
}