//
// Created by gnilk on 15.02.23.
//

#include "TextBuffer.h"

using namespace gedit;

void TextBuffer::Reparse() {
    // No language, don't do this...
    if (language == nullptr) {
        return;
    }
    auto tokenizer = language->Tokenizer();
    tokenizer.ParseLines(lines);
}
