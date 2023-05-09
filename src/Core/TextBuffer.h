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

        typedef enum {
            kBuffer_Empty,      // Buffer has been created, but nothing commited
            kBuffer_FileRef,    // Buffer references a file - but data is not loaded
            kBuffer_Loaded,     // Data from file has been loaded
            kBuffer_Changed,    // Data has been changed
        } BufferState;

    public:
        explicit TextBuffer(const std::string &bufferName) : name(bufferName) {
        }
        virtual ~TextBuffer() = default;

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
        void SetPathName(const std::filesystem::path &newPathName) {
            pathName = newPathName;
        }

        void SetNameFromFileName(const std::string &newFileName);

        void Close();
        const std::string GetFileName() {
            return pathName.filename().string();
        }

        void AddLine(Line::Ref line) {
            lines.push_back(line);
        }
        void AddLine(const char *textString) {
            auto newLine = std::make_shared<Line>(textString);
            lines.push_back(newLine);
        }
        std::vector<Line::Ref> &Lines() { return lines; }

        Line::Ref LineAt(size_t idxLine) {
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
        }

        void CopyRegionToString(std::string &outText, const Point &start, const Point &end);

        const std::string &Name() const {
            return name;
        }

        void SetLanguage(LanguageBase::Ref newLanguage) {
            language = newLanguage;
            Reparse();
        }

        bool HaveLanguage() { return language!= nullptr; }
        LanguageBase &LangParser() { return *language; }

        std::optional<Line::Ref>FindParseStart(size_t idxStartLine);

        void Reparse();

    protected:
        void StartReparseThread();
    private:
        volatile ParseState state = kState_None;

        BufferState bufferState = kBuffer_Empty;

        std::filesystem::path pathName = "";     // full path filename
        std::string name;

        std::vector<Line::Ref> lines;
        LanguageBase::Ref language = nullptr;
        std::thread *reparseThread = nullptr;

        bool bQuitReparse = false;

        // Do not put the edit controller here - might make sense, but it will cause problems later
        // Example: split-window editing with same file => won't work
        // EditController *editController = nullptr;
    };
}


#endif //EDITOR_TEXTBUFFER_H
