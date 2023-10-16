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
#include <sstream>


using namespace gedit;

TextBuffer::TextBuffer() {
}

TextBuffer::Ref TextBuffer::CreateEmptyBuffer() {
    auto buffer = std::make_shared<TextBuffer>();
    buffer->logger = gnilk::Logger::GetLogger("TextBuffer");
    buffer->AddLineUTF8("");
    buffer->bufferState = kBuffer_Empty;
    return buffer;
}

TextBuffer::Ref TextBuffer::CreateFileReferenceBuffer() {
    auto buffer = CreateEmptyBuffer();
    buffer->bufferState = kBuffer_FileRef;
    return buffer;
}

TextBuffer::Ref TextBuffer::CreateBufferFromFile(const std::filesystem::path &fromPath) {
    auto buffer = CreateFileReferenceBuffer();
    if (!buffer->Load(fromPath)) {
        return nullptr;
    }
    return buffer;
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
        WaitForParseCompletion();
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
void TextBuffer::WaitForParseCompletion() {
    // No language, don't do this...
    if (language == nullptr) {
        return;
    }
    // When a workspace is opened, a lot of text-buffers are created 'passively' and are not loaded until activated
    if (IsEmpty()) {
        return;
    }

    auto useThreads = Config::Instance()["main"].GetBool("threaded_syntaxparser", false);

    if (!useThreads) {
        return;
    }

    // Start job's wait until the jobs are started - so here we
    // can assume that if IDLE we are already done, no need to check if the queue has items..
    while (GetParseState() != kState_Idle) {
        std::this_thread::yield();
    }
}

void TextBuffer::Close() {
    if (reparseThread != nullptr) {
        bQuitReparse = true;
        reparseThread->join();
        reparseThread = nullptr;

        // FIXME: Not sure this is a good idea, it works for basic stuff..
        if ((bufferState == kBuffer_Changed) || (bufferState == kBuffer_Loaded)) {
            ChangeBufferState(BufferState::kBuffer_FileRef);
        }
    }
}

bool TextBuffer::CanEdit() {
    // No reparse thread => we can always edit - single threaded mode...
    if (reparseThread == nullptr) return true;
    if (GetParseState() == kState_Idle) return true;
    if (bIsReadOnly == false) return true;
    return false;
}

void TextBuffer::StartParseJob(TextBuffer::ParseJobType jobType, size_t idxLineStart, size_t idxLineEnd) {
    parseThreadLock.lock();
    ParseJob job = {
            .jobType = jobType,
            .idxLineStart = idxLineStart,
            .idxLineEnd = idxLineEnd
    };
    parseQueue.push_front(job);
    parseThreadLock.unlock();
    // Note: We don't really care about kicking off the parse-job here
}

void TextBuffer::ChangeParseState(ParseState newState) {
//    static unordered_map<ParseState, std::string> stateToString = {
//            {kState_None, "None"},
//            {kState_Idle, "Idle"},
//            {kState_Start, "Start"},
//            {kState_Parsing, "Parsing"}
//    };
//    logger->Debug("ChangeParseState %s -> %s",
//                  stateToString[GetParseState()].c_str(),
//                  stateToString[newState].c_str());

    parseThreadLock.lock();
    parseState = newState;
    parseThreadLock.unlock();
}


void TextBuffer::StartParseThread() {
    // Do not start twice...
    if (reparseThread != nullptr) {
        return;
    }

    // Ensure we are in the 'none' state
    bQuitReparse = false;
    parseState = kState_None;
    reparseThread = new std::thread([this]() {
        ParseThread();
    });
    // Wait until the thread has become idle, first thing the thread is doing...
    while(GetParseState() != kState_Idle) {
        std::this_thread::yield();
    }
}

void TextBuffer::ParseThread() {
    ChangeParseState(kState_Idle);
    while(!bQuitReparse) {
        parseThreadLock.lock();
        if (parseQueue.empty()) {
            parseThreadLock.unlock();
            std::this_thread::yield();
            continue;
        }
        parseThreadLock.unlock();

        ChangeParseState(kState_Parsing);
        // Fetch job..
        parseThreadLock.lock();
        auto &job = parseQueue.front();
        parseQueue.pop_front();
        parseThreadLock.unlock();

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

size_t TextBuffer::ExecuteRegionParse(size_t idxLineStart, size_t idxLineEnd) {
    auto tokenizer = language->Tokenizer();
    auto result = tokenizer.ParseRegion(lines, idxLineStart, idxLineEnd);
    if (result != 0) {
        logger->Error("ParseRegion, tokenizer didn't reach complete, result=%zu", result);
        // FIXME: We can issue a full/complete Reparse here..
    }
    parseMetrics.total += 1;
    parseMetrics.region += 1;
    return result;
}

void TextBuffer::CopyRegionToString(std::u32string &outText, const Point &start, const Point &end) {
    for (int idxLine=start.y;idxLine<end.y;idxLine++) {
        outText += lines[idxLine]->Buffer();
        outText += U"\n";
    }
}

bool TextBuffer::Load(const std::filesystem::path &pathName) {
    if (!std::filesystem::exists(pathName)) {
        logger->Error("Can't load, file doesn't exists");
        return false;
    }
    if (!std::filesystem::is_regular_file(pathName)) {
        logger->Error("Can't load, not regular file");
        return false;
    }

    if (bufferState != kBuffer_FileRef) {
        return false;
    }

    auto filename = pathName.string();

    // Clear out any lines before loading - the CTOR add's an empty line (so we can edit directly)
    lines.clear();

    // Now load
    FILE *f = fopen(filename.c_str(), "r");
    if (f == nullptr) {
        return false;
    }

    static uint8_t tmp[GEDIT_MAX_LINE_LENGTH];

    // Read first line and check BOM
    if (fgets((char *)tmp, GEDIT_MAX_LINE_LENGTH, f)) {
        // Check BOM on first line
        if ((tmp[0] == 0xfe) && (tmp[1] == 0xff)) {
            // remove BOM and proceed..
            AddLineUTF8((char *) &tmp[2]);
        } else {
            AddLineUTF8((char *) tmp);
        }
        // Continue reading rest of file...
        while(fgets((char *)tmp, GEDIT_MAX_LINE_LENGTH, f)) {
            // BOM detection on first line
            AddLineUTF8((char *) tmp);
        }
    }
    fclose(f);

    // Ok, I admit - it never quite occured to me that we should open EMPTY files..  but of course we do (so do I)
    // We opened an empty file - let's add a dummy here
    if (lines.size() == 0) {
        AddLineUTF8("");
    }
    // Change state, do this before UpdateLang - since lang checks if loaded before allowing parse to happen
    ChangeBufferState(kBuffer_Loaded);
//    UpdateLanguageParserFromFilename();
    return true;
}

bool TextBuffer::Save(const std::filesystem::path &pathName) {
    return DoSave(pathName, false);
}
bool TextBuffer::SaveForce(const std::filesystem::path &pathName) {
    return DoSave(pathName, true);
}

bool TextBuffer::DoSave(const std::filesystem::path &pathName, bool skipChangeCheck) {

    // Check if we even have data!
    if ((bufferState == kBuffer_Empty) || (bufferState == kBuffer_FileRef)) {
        return false;
    }
    // No need to save unless changed
    if (!skipChangeCheck && (bufferState != kBuffer_Changed)) {
        return true;
    }

    logger->Debug("Saving file: %s", pathName.c_str());
    auto f = fopen(pathName.c_str(), "w");
    if (f == nullptr) {
        logger->Error("Failed to save buffer, err: %d:%s", errno, strerror(errno));

        // Best dump this to the console as well..
        std::u32string strError = U"Unable to save file, err: ";
        strError += UnicodeHelper::utf8to32(strerror(errno));
        RuntimeConfig::Instance().OutputConsole()->WriteLine(strError);
        return false;
    }

    // Actual writing of data...
    for (auto &l: lines) {
        auto utf8 = UnicodeHelper::utf32to8(l->Buffer());
        fprintf(f,"%s\n", utf8.data());
    }
    fclose(f);

    // Go back to 'clean' - i.e. data is loaded...
    ChangeBufferState(kBuffer_Loaded);
    return true;
}

//void TextBuffer::SetPathName(const std::filesystem::path &newPathName) {
//    pathName = newPathName;
//    logger->Debug("SetPathName: %s", pathName.c_str());
//    if ((bufferState == kBuffer_Loaded) || (bufferState == kBuffer_Changed)){
//        // FIXME: Save here
//    }
//    UpdateLanguageParserFromFilename();
//}
//void TextBuffer::Rename(const std::string &newFileName) {
//    pathName = pathName.parent_path().append(newFileName);
//    logger->Debug("New name: %s", pathName.c_str());
//    // FIXME: should probably save the file here
//    if ((bufferState == kBuffer_Loaded) || (bufferState == kBuffer_Changed)){
//        // FIXME: Save here
//    }
//    UpdateLanguageParserFromFilename();
//}
//void TextBuffer::UpdateLanguageParserFromFilename() {
//    auto lang = Editor::Instance().GetLanguageForExtension(pathName.extension());
//    if (lang != nullptr) {
//        language = lang;
//        if ((bufferState == kBuffer_Loaded) || (bufferState == kBuffer_Changed)) {
//            Reparse();
//        }
//    }
//}

void TextBuffer::ChangeBufferState(BufferState newState) {
    bufferState = newState;
}

void TextBuffer::OnLineChanged(const Line &line) {
    ChangeBufferState(kBuffer_Changed);

    // auto save
    auto &tc = RuntimeConfig::Instance().GetTimerController();
    if (autoSaveTimer == nullptr) {
        auto autoSaveTimeout = Config::Instance()["main"].GetInt("autosave_timeout_ms", 2000);
        if (autoSaveTimeout > 0) {
            Timer::DurationMS duration(autoSaveTimeout);

            // Timer not created, so let's create the timer with a 2000msec timeout
            autoSaveTimer = tc.CreateAndScheduleTimer(duration, [this]() {
                // Timer kicked in - we are now in the timer-thread context!!!
                // We should NOT do anything here - instead we post ourselves to the editor message queue (which is tied to the UI)
                // this queue is emptied first on each run-loop/redraw iteration, this is the way...
                RuntimeConfig::Instance().GetRootView().PostMessage([this]() {
                    // Once here - we can retrieve the workspace node (which knows about the filename) for this TextBuffer
                    // this is a bit convoluted but not done very often...
                    auto model = Editor::Instance().GetModelFromTextBuffer(shared_from_this());
                    auto node = Editor::Instance().GetWorkspaceNodeForModel(model);
                    node->SaveData();
                });
            });
        }
    } else {
        // On every change, let's restart the timer...
        tc.RestartTimer(autoSaveTimer);
    }


}

//
// Flattens a number of lines and adds to 'out' - will insert CRLN at end of each line
//
size_t TextBuffer::Flatten(std::u32string &out, size_t idxFromLine, size_t nLines) {

    size_t linesCopied = 0;
    if (idxFromLine >= NumLines()) {
        return linesCopied;
    }
    if (nLines == 0) {
        nLines = NumLines();
    }
    size_t idxBuffer = 0;
    out.clear();

    for(size_t i=0;i<nLines;i++) {
        auto l = LineAt(i + idxFromLine);
        // Break if we hit an invalid line..
        if (l == nullptr) {
            return linesCopied;
        }
        out += l->Buffer();
        out += U"\n";
//        auto strutf8 = UnicodeHelper::utf32to8(l->Buffer());
//
//        snprintf(&outBuffer[idxBuffer], maxBytes - idxBuffer, "%s\n", l->Buffer().data());
//        idxBuffer += l->Length() + sizeof('\n');
        linesCopied++;
    }
    return linesCopied;
}
