//
// Created by gnilk on 15.02.23.
//

#ifndef EDITOR_TEXTBUFFER_H
#define EDITOR_TEXTBUFFER_H

#include <vector>
#include <thread>
#include <memory>
#include <optional>
#include <filesystem>

#include "Core/Language/LanguageBase.h"
#include "Core/Line.h"
#include "Core/Point.h"

namespace gedit {
    class TextBuffer {
    public:
        using Ref = std::shared_ptr<TextBuffer>;

    public:
        typedef enum {
            kState_None,
            kState_Idle,
            kState_Start,
            kState_Parsing,
        } ParseState;

        struct ParseMetrics {
            size_t total = 0;
            size_t full = 0;
            size_t region = 0;
        } parseMetrics;


        typedef enum {
            kBuffer_Empty,      // Buffer has been created, but nothing commited
            kBuffer_FileRef,    // Buffer references a file - but data is not loaded
            kBuffer_Loaded,     // Data from file has been loaded
            kBuffer_Changed,    // Data has been changed
        } BufferState;

    public:
        explicit TextBuffer(const std::string &bufferName);
        virtual ~TextBuffer();

        static TextBuffer::Ref CreateEmptyBuffer(const std::string &bufferName) {
            auto buffer = std::make_shared<TextBuffer>(bufferName);
            buffer->bufferState = kBuffer_Empty;
            return buffer;
        }

        static TextBuffer::Ref CreateFileReferenceBuffer(const std::filesystem::path &fromPath) {
             auto buffer = CreateEmptyBuffer(fromPath.filename().string());
             buffer->SetPathName(fromPath);
             buffer->bufferState = kBuffer_FileRef;
             return buffer;
        }

        static TextBuffer::Ref CreateBufferFromFile(const std::filesystem::path &fromPath) {
            auto buffer = CreateFileReferenceBuffer(fromPath);
            if (!buffer->Load()) {
                return nullptr;
            }
            return buffer;
        }

        bool Save();
        bool Load();

        bool HasPathName() {
            return !pathName.empty();
        }

        bool IsEmpty() {
            return ((bufferState == kBuffer_Empty) || (bufferState == kBuffer_FileRef));
        }

        void SetPathName(const std::filesystem::path &newPathName);
        void Rename(const std::string &newFileName);

        const std::string GetPathName() {
            return pathName.string();
        }
        const std::string GetName() {
            return pathName.filename();
        }

        void Close();

        void AddLine(Line::Ref line) {
            line->SetOnChangeDelegate([this](const Line &line){
                OnLineChanged(line);
            });
            lines.push_back(line);
        }

        void AddLine(const char *textString) {
            auto newLine = Line::Create(textString);
            newLine->SetOnChangeDelegate([this](const Line &line){
                OnLineChanged(line);
            });
            lines.push_back(newLine);
        }

        void Insert(size_t idxPos, Line::Ref line) {
            line->SetOnChangeDelegate([this](const Line &line){
                OnLineChanged(line);
            });
            auto it = lines.begin() + idxPos;
            lines.insert(it, line);
        }
        void Insert(size_t idxPos, const std::string &text) {
            auto newLine = Line::Create(text);
            Insert(idxPos, newLine);
        }

        void Insert(const std::vector<const Line::Ref>::iterator &it, Line::Ref line) {
            line->SetOnChangeDelegate([this](const Line &line){
                OnLineChanged(line);
            });
            lines.insert(it, line);
        }

        const std::vector<Line::Ref> &Lines() { return lines; }

        Line::Ref LineAt(size_t idxLine) {
            if (idxLine > NumLines()) {
                return nullptr;
            }
            return lines[idxLine];
        }

        size_t NumLines() {
            return lines.size();
        }

        void DeleteLineAt(size_t idxLine) {
            auto line = LineAt(idxLine);
            line->Lock();
            lines.erase(lines.begin() + idxLine);
            line->Release();
            ChangeBufferState(kBuffer_Changed);
        }

        void CopyRegionToString(std::string &outText, const Point &start, const Point &end);

        void SetLanguage(LanguageBase::Ref newLanguage) {
            language = newLanguage;
            Reparse();
        }

        void Flatten(char *outBuffer, size_t maxBytes, size_t idxFromLine, size_t nLines);

        bool HaveLanguage() { return language!= nullptr; }
        LanguageBase &LangParser() { return *language; }

        bool CanEdit();
        void Reparse();
        void ReparseRegion(size_t idxStartLine, size_t idxEndLine);
        const ParseMetrics &GetParseMetrics() {
            return parseMetrics;
        }
    public:
        // For unit testing...
        BufferState GetBufferState() {
            return bufferState;
        }
        ParseState  GetParseState() {
            return parseState;
        }
    protected:
        void UpdateLanguageParserFromFilename();
        void OnLineChanged(const Line &line);
        void ChangeBufferState(BufferState newState);
    private:
        enum class ParseJobType {
            kParseFull,
            kParseRegion,
        };
        struct ParseJob {
            ParseJobType jobType = ParseJobType::kParseFull;
            size_t idxLineStart = {};
            size_t idxLineEnd = {};
        };
    private:
        void StartParseThread();
        void ParseThread();
        void ChangeParseState(ParseState newState);
        void StartParseJob(ParseJobType jobType, size_t idxLineStart = 0, size_t idxLineEnd = 0);
        void ExecuteParseJob(const ParseJob &job);
        void ExecuteFullParse();
        void ExecuteRegionParse(size_t idxLineStart, size_t idxLineEnd);
    private:
        volatile ParseState parseState = kState_None;

        BufferState bufferState = kBuffer_Empty;
        std::filesystem::path pathName = "";     // full path filename


        std::vector<Line::Ref> lines;

        // Language parsing variables
        LanguageBase::Ref language = nullptr;
        std::thread *reparseThread = nullptr;
        std::mutex parseQueueLock;
        std::deque<ParseJob> parseQueue;


        bool bQuitReparse = false;

        // Do not put the edit controller here - might make sense, but it will cause problems later
        // Example: split-window editing with same file => won't work
        // EditController *editController = nullptr;
    };
}


#endif //EDITOR_TEXTBUFFER_H
