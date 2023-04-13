//
// Created by gnilk on 15.02.23.
//

#ifndef EDITOR_TEXTBUFFER_H
#define EDITOR_TEXTBUFFER_H

#include <vector>

#include "Core/Language/LanguageBase.h"
#include "Core/Line.h"
#include "Core/Point.h"
#include <memory>

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

        std::vector<Line *> &Lines() { return lines; }

        Line *LineAt(size_t idxLine) {
            return lines[idxLine];
        }

        size_t NumLines() {
            return lines.size();
        }

        void CopyRegionToString(std::string &outText, const Point &start, const Point &end);

        const std::string &Name() const {
            return name;
        }

        void SetLanguage(LanguageBase *newLanguage) {
            language = newLanguage;
            Reparse();
        }

        bool HaveLanguage() { return language!= nullptr; }
        LanguageBase &LangParser() { return *language; }

        std::optional<Line *>FindParseStart(size_t idxStartLine);

        void Reparse();

    protected:
        void StartReparseThread();
    private:
        volatile State state = kState_None;
        std::string name;
        std::vector<Line *> lines;
        LanguageBase *language = nullptr;
        std::thread *reparseThread = nullptr;

        // Do not put the edit controller here - might make sense, but it will cause problems later
        // Example: split-window editing with same file => won't work
        // EditController *editController = nullptr;
    };
}


#endif //EDITOR_TEXTBUFFER_H
