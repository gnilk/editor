//
// Created by gnilk on 31.01.23.
//

#include "Buffer.h"
#include "Line.h"

//
// Reparse all lines and create attributes
//
void Buffer::Reparse() {
    // No language, don't do this...
    if (language == nullptr) {
        return;
    }
    auto tokenizer = language->Tokenizer();
    tokenizer.ParseLines(lines);
}
