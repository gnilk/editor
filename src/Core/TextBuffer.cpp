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

TextBuffer::~TextBuffer() {
    // Close the debugging thread if it is running...
    if (reparseThread != nullptr) {
        bQuitReparse = true;
        // Timeout????
        while(GetParseState() != kState_None) {
            std::this_thread::yield();
        }
    }
}


void TextBuffer::Reparse() {
    // No language, don't do this...
    if (language == nullptr) {
        return;
    }
    // When a workspace is opened, a lot of text-buffers are created 'passively' and are not loaded until activiated
    if(IsEmpty()) {
        return;
    }

    auto useThreads = Config::Instance()["main"].GetBool("threaded_syntaxparser", false);

    if (!useThreads) {
        ExecuteFullParse();
        return;
    }

    if (reparseThread == nullptr) {
        StartParseThread();
    }

    // Don't queue up full-parse job's unless we are idle
    // On large files this will literally never end and the queue will just fill up...
    if (GetParseState() == kState_Idle) {
        StartParseJob(ParseJobType::kParseFull);
    }
}

void TextBuffer::ReparseRegion(size_t idxStartLine, size_t idxEndLine) {
    // No language, don't do this...
    if (language == nullptr) {
        return;
    }
    // When a workspace is opened, a lot of text-buffers are created 'passively' and are not loaded until activated
    if(IsEmpty()) {
        return;
    }

    auto useThreads = Config::Instance()["main"].GetBool("threaded_syntaxparser", false);

    if (!useThreads) {
        ExecuteRegionParse(idxStartLine, idxEndLine);
        return;
    }
    if (reparseThread == nullptr) {
        StartParseThread();
    }
    // Cut this at the end...
    if (idxEndLine > (lines.size()-1)) {
        idxEndLine = lines.size();
    }
    StartParseJob(ParseJobType::kParseRegion, idxStartLine, idxEndLine);
}

void TextBuffer::Close() {
    if (reparseThread != nullptr) {
        bQuitReparse = true;
        reparseThread->join();
        reparseThread = nullptr;
    }
}

bool TextBuffer::CanEdit() {
    // No reparse thread => we can always edit - single threaded mode...
    if (reparseThread == nullptr) return true;
    if (GetParseState() == kState_Idle) return true;
    return false;
}

void TextBuffer::StartParseJob(TextBuffer::ParseJobType jobType, size_t idxLineStart, size_t idxLineEnd) {
    parseQueueLock.lock();
    ParseJob job = {
            .jobType = jobType,
            .idxLineStart = idxLineStart,
            .idxLineEnd = idxLineEnd
    };
    parseQueue.push_front(job);
    parseQueueLock.unlock();
    // Kick of the parse-job
    ChangeParseState(kState_Start);
    // Wait until we are actually parsing...
    while (GetParseState() != kState_Parsing) {
        std::this_thread::yield();
    }
}

void TextBuffer::ChangeParseState(ParseState newState) {
    parseState = newState;
}


void TextBuffer::StartParseThread() {
    reparseThread = new std::thread([this]() {
        ParseThread();
    });
    // Wait until the thread has become idle...
    while(GetParseState() != kState_Idle) {
        std::this_thread::yield();
    }
}

void TextBuffer::ParseThread() {
    ChangeParseState(kState_Idle);
    while(!bQuitReparse) {
        if (GetParseState() == kState_Idle) {
            std::this_thread::yield();
            continue;
        }
        ChangeParseState(kState_Parsing);
        // Fetch job..
        parseQueueLock.lock();
        auto &job = parseQueue.front();
        parseQueue.pop_front();
        parseQueueLock.unlock();

        ExecuteParseJob(job);
        Editor::Instance().TriggerUIRedraw();

        ChangeParseState(kState_Idle);
    }
    ChangeParseState(kState_None);
}

void TextBuffer::ExecuteParseJob(const ParseJob &job) {
    if (job.jobType == ParseJobType::kParseFull) {
        ExecuteFullParse();
    } else if (job.jobType == ParseJobType::kParseRegion) {
        ExecuteRegionParse(job.idxLineStart, job.idxLineEnd);
    }
}

void TextBuffer::ExecuteFullParse() {
    auto tokenizer = language->Tokenizer();
    tokenizer.ParseLines(lines);
    parseMetrics.total += 1;
    parseMetrics.full += 1;
}

void TextBuffer::ExecuteRegionParse(size_t idxLineStart, size_t idxLineEnd) {
    auto tokenizer = language->Tokenizer();
    tokenizer.ParseRegion(lines, idxLineStart, idxLineEnd);
    parseMetrics.total += 1;
    parseMetrics.region += 1;
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
    ChangeBufferState(kBuffer_Loaded);
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
    ChangeBufferState(kBuffer_Loaded);
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

void TextBuffer::ChangeBufferState(BufferState newState) {
    bufferState = newState;
}

void TextBuffer::OnLineChanged(const Line &line) {
    ChangeBufferState(kBuffer_Changed);
}

