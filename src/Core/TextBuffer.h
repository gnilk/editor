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

#include "logger.h"

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
        explicit TextBuffer();
        virtual ~TextBuffer();

        static TextBuffer::Ref CreateEmptyBuffer();
        static TextBuffer::Ref CreateFileReferenceBuffer();
        static TextBuffer::Ref CreateBufferFromFile(const std::filesystem::path &fromPath);

        bool Save(const std::filesystem::path &pathName);
        bool SaveForce(const std::filesystem::path &pathName);
        bool Load(const std::filesystem::path &pathName);

        bool IsEmpty() {
            return ((bufferState == kBuffer_Empty) || (bufferState == kBuffer_FileRef));
        }

        void Close();

        void AddLine(Line::Ref line) {
            line->SetOnChangeDelegate([this](const Line &line){
                OnLineChanged(line);
            });
            lines.push_back(line);
            ChangeBufferState(kBuffer_Changed);
        }

        void AddLine(const std::u32string string) {
            auto newLine = Line::Create(string);
            AddLine(newLine);
        }

        void AddLineUTF8(const char *textString) {
            auto newLine = Line::Create(textString);
            AddLine(newLine);
        }

        void Insert(size_t idxPos, Line::Ref line) {
            line->SetOnChangeDelegate([this](const Line &line){
                OnLineChanged(line);
            });
            auto it = lines.begin() + idxPos;
            lines.insert(it, line);
            ChangeBufferState(kBuffer_Changed);
        }

        void Insert(std::vector<Line::Ref>::const_iterator it, Line::Ref line) {
            line->SetOnChangeDelegate([this](const Line &line){
                OnLineChanged(line);
            });
            lines.insert(it, line);
            ChangeBufferState(kBuffer_Changed);
        }


        void Insert(size_t idxPos, const std::string &text) {
            auto newLine = Line::Create(text);
            Insert(idxPos, newLine);
        }

        void Insert(size_t idxPos, const std::u32string &text) {
            auto newLine = Line::Create(text);
            Insert(idxPos, newLine);
        }

        const std::vector<Line::Ref> &Lines() { return lines; }

        Line::Ref LineAt(size_t idxLine) {
            if (idxLine >= NumLines()) {
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

        void CopyRegionToString(std::u32string &outText, const Point &start, const Point &end);

        void SetLanguage(LanguageBase::Ref newLanguage) {
            language = newLanguage;
            Reparse();
        }

        // Flatten the buffer to a regular char array with CRLN
        // nLines = 0, process as much data as possible..
        // returns the number of line..
        size_t Flatten(std::u32string &out, size_t idxFromLine, size_t nLines);

        bool HaveLanguage() { return language!= nullptr; }
        LanguageBase &GetLanguage() { return *language; }

        void SetReadOnly(bool newIsReadOnly) {
            bIsReadOnly = newIsReadOnly;
        }
        bool IsReadOnly() {
            return bIsReadOnly;
        }

        bool CanEdit();
        void Reparse();
        void ReparseRegion(size_t idxStartLine, size_t idxEndLine);
        void WaitForParseCompletion();
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
        void OnLineChanged(const Line &line);
        void ChangeBufferState(BufferState newState);
        bool DoSave(const std::filesystem::path &pathName, bool skipChangedCheck);
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

        std::vector<Line::Ref> lines;

        // Language parsing variables
        LanguageBase::Ref language = nullptr;
        std::thread *reparseThread = nullptr;
        std::mutex parseThreadLock;
        std::deque<ParseJob> parseQueue;

        bool bIsReadOnly = false;   // assume they are not
        bool bQuitReparse = false;

        gnilk::ILogger *logger = nullptr;
    };
}


#endif //EDITOR_TEXTBUFFER_H
