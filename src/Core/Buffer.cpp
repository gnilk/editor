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

    std::vector<gnilk::LangToken> tokens;

    for(auto &l : lines) {
        std::vector<gnilk::LangToken> tokens;
        tokenizer.ParseLine(tokens, l->Buffer().data());
        for(auto &t : tokens) {
            Line::LineAttrib attrib;
            attrib.idxOrigString = t.idxOrigStr;
            attrib.idxColor = static_cast<int>(t.classification);
            l->Attributes().push_back(attrib);
        }
        tokens.clear();
    }
}
