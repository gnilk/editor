//
// Created by gnilk on 15.02.23.
//

#include "logger.h"
#include "TextBuffer.h"
#include <thread>
using namespace gedit;

void TextBuffer::Reparse() {
    // No language, don't do this...
    if (language == nullptr) {
        return;
    }

    if (reparseThread == nullptr) {
        StartReparseThread();
        while(state == kState_None) {
            std::this_thread::yield();
        }
    } else if (state == kState_Idle){
        state = kState_Start;
    }
}
void TextBuffer::Close() {
    if (reparseThread != nullptr) {
        bQuitReparse = true;
        reparseThread->join();
        reparseThread = nullptr;
    }
}

void TextBuffer::StartReparseThread() {
    reparseThread = new std::thread([this]() {
        while(!bQuitReparse) {
            state = kState_Parsing;
            auto logger = gnilk::Logger::GetLogger("TextBuffer");
            logger->Debug("Begin syntax parsing");
            auto tokenizer = language->Tokenizer();
            tokenizer.ParseLines(lines);
            logger->Debug("End syntax parsing");
            state = kState_Idle;
            while ((state == kState_Idle) && (!bQuitReparse)) {
                std::this_thread::yield();
            }
        }
    });
}

std::optional<Line *>TextBuffer::FindParseStart(size_t idxStartLine) {
    if (!HaveLanguage()) {
        return {};
    }
    if (idxStartLine >= lines.size()) {
        return {};
    }
    while (lines[idxStartLine]->GetStateStackDepth() != LangLineTokenizer::RootStateDepth) {
        if (idxStartLine == 0) {
            return lines[0];
        }
        idxStartLine -= 1;
    }
    return lines[idxStartLine];
}


void TextBuffer::CopyRegionToString(std::string &outText, const Point &start, const Point &end) {
    for (int idxLine=start.y;idxLine<end.y;idxLine++) {
        outText += lines[idxLine]->Buffer();
        outText += "\n";
    }
}

