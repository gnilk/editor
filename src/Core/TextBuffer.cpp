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

void TextBuffer::StartReparseThread() {
    reparseThread = new std::thread([this]() {
        while(true) {
            state = kState_Parsing;
            auto logger = gnilk::Logger::GetLogger("TextBuffer");
            logger->Debug("Begin syntax parsing");
            auto tokenizer = language->Tokenizer();
            tokenizer.ParseLines(lines);
            logger->Debug("End syntax parsing");
            state = kState_Idle;
            while (state == kState_Idle) {
                std::this_thread::yield();
            }
        }
    });
}


void TextBuffer::CopyRegionToString(std::string &outText, const Point &start, const Point &end) {
    for (int idxLine=start.y;idxLine<end.y;idxLine++) {
        outText += lines[idxLine]->Buffer();
        outText += "\n";
    }
}

