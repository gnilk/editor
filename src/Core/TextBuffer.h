//
// Created by gnilk on 15.02.23.
//

#ifndef EDITOR_TEXTBUFFER_H
#define EDITOR_TEXTBUFFER_H

#include <vector>

#include "Core/Language/LanguageBase.h"
#include "Core/Line.h"
#include "Core/Point.h"
#include <thread>
#include <memory>
#include <optional>

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
        } State;

    public:
        explicit TextBuffer(const std::string &bufferName) : name(bufferName) {

        }
        virtual ~TextBuffer() = default;

        void Close();

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
        volatile State state = kState_None;
        std::string name;
        //std::vector<Line *> lines;
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
