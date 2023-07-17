//
// Created by gnilk on 15.02.23.
//

#include "logger.h"
#include "TextBuffer.h"
#include "Core/Config/Config.h"
#include "Core/Editor.h"
#include <thread>
#include <filesystem>
#include <fstream>


using namespace gedit;

TextBuffer::TextBuffer(const std::string &bufferName) {
    SetPathName(bufferName);
}

void TextBuffer::Reparse() {
    // No language, don't do this...
    if (language == nullptr) {
        return;
    }
    auto useThreads = Config::Instance()["main"].GetBool("threaded_syntaxparser", false);

    if (!useThreads) {
        auto tokenizer = language->Tokenizer();
        tokenizer.ParseLines(lines);
    } else {
        if (reparseThread == nullptr) {
            StartReparseThread();
            while (state == kState_None) {
                std::this_thread::yield();
            }
        } else if (state == kState_Idle) {
            state = kState_Start;
        }
    }
}
void TextBuffer::Close() {
    if (reparseThread != nullptr) {
        bQuitReparse = true;
        reparseThread->join();
        reparseThread = nullptr;
    }
}

bool TextBuffer::CanEdit() {
    if (state == kState_Idle) return true;
    return false;
}

void TextBuffer::StartReparseThread() {
    reparseThread = new std::thread([this]() {
        auto logger = gnilk::Logger::GetLogger("TextBuffer");

        // First time we need to parse the whole buffer, on any updates we will just reparse the affected region
        logger->Debug("Begin Initial Syntax Parsing");
        state = kState_Parsing;
        auto tokenizer = language->Tokenizer();
        tokenizer.ParseLines(lines);
        logger->Debug("End Initial Syntax Parsing");
        state = kState_Idle;

        // in a better world - we should probably have a subscription for this.
        // but we are in a different thread - and this does hand over to the main thread
        Editor::Instance().TriggerUIRedraw();
        while(!bQuitReparse) {
            while ((state == kState_Idle) && (!bQuitReparse)) {
                std::this_thread::yield();
            }
            if (bQuitReparse) break;
            if (Editor::Instance().GetActiveModel() == nullptr) continue;

            state = kState_Parsing;
            logger->Debug("Begin Regional Syntax Parsing");
            tokenizer = language->Tokenizer();




            int idxLine = Editor::Instance().GetActiveModel()->GetReparseStartLineIndex();
            tokenizer.ParseRegion(lines, idxLine);
            logger->Debug("End Regional Syntax Parsing");
            state = kState_Idle;

            Editor::Instance().TriggerUIRedraw();
        }
    });
}

std::optional<Line::Ref>TextBuffer::FindParseStart(size_t idxStartLine) {
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

bool TextBuffer::Save() {
    if (!HasPathName()) {
        return false;
    }
    // Check if we even have data!
    if ((bufferState == kBuffer_Empty) || (bufferState == kBuffer_FileRef)) {
        return false;
    }
    // No need to save unless changed
    if (bufferState != kBuffer_Changed) {
        return true;
    }

    std::ofstream out(pathName, std::ios::binary);
    for(auto &l : lines) {
        out << l->Buffer() << "\n";
    }
    out.close();

    // Go back to 'clean' - i.e. data is loaded...
    ChangeState(kBuffer_Loaded);
    return true;
}

bool TextBuffer::Load() {
    if (!HasPathName()) {
        return false;
    }
    if (bufferState != kBuffer_FileRef) {
        return false;
    }

    auto filename = pathName.string();

    // Now load
    FILE *f = fopen(filename.c_str(), "r");
    if (f == nullptr) {
        return false;
    }

    char tmp[GEDIT_MAX_LINE_LENGTH];
    while(fgets(tmp, GEDIT_MAX_LINE_LENGTH, f)) {
        AddLine(tmp);
    }
    fclose(f);
    // Change state, do this before UpdateLang - since lang checks if loaded before allowing parse to happen
    ChangeState(kBuffer_Loaded);
    UpdateLanguageParserFromFilename();


    return true;
}

void TextBuffer::SetPathName(const std::filesystem::path &newPathName) {
    pathName = newPathName;
    auto logger = gnilk::Logger::GetLogger("TextBuffer");
    logger->Debug("SetPathName: %s", pathName.c_str());
    if ((bufferState == kBuffer_Loaded) || (bufferState == kBuffer_Changed)){
        // FIXME: Save here
    }
    UpdateLanguageParserFromFilename();
}

void TextBuffer::Rename(const std::string &newFileName) {
    pathName = pathName.stem().append(newFileName);
    auto logger = gnilk::Logger::GetLogger("TextBuffer");
    logger->Debug("New name: %s", pathName.c_str());
    // FIXME: should probably save the file here
    if ((bufferState == kBuffer_Loaded) || (bufferState == kBuffer_Changed)){
        // FIXME: Save here
    }
    UpdateLanguageParserFromFilename();
}

void TextBuffer::UpdateLanguageParserFromFilename() {
    auto lang = Editor::Instance().GetLanguageForExtension(pathName.extension());
    if (lang != nullptr) {
        language = lang;
        if ((bufferState == kBuffer_Loaded) || (bufferState == kBuffer_Changed)) {
            Reparse();
        }
    }
}

void TextBuffer::ChangeState(BufferState newState) {
    bufferState = newState;
}

void TextBuffer::OnLineChanged(const Line &line) {
    ChangeState(kBuffer_Changed);
}

