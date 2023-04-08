//
// Created by gnilk on 15.02.23.
//

#include "logger.h"
#include "TextBuffer.h"

using namespace gedit;

void TextBuffer::Reparse() {
    // No language, don't do this...
    if (language == nullptr) {
        return;
    }
    auto logger = gnilk::Logger::GetLogger("TextBuffer");
    logger->Debug("Begin syntax parsing");
    auto tokenizer = language->Tokenizer();
    tokenizer.ParseLines(lines);
    logger->Debug("End syntax parsing");
}

void TextBuffer::CopyRegionToString(std::string &outText, const Point &start, const Point &end) {
    for (int idxLine=start.y;idxLine<end.y;idxLine++) {
        outText += lines[idxLine]->Buffer();
        outText += "\n";
    }
}

