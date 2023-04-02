//
// Created by gnilk on 15.02.23.
//

#ifndef EDITOR_TEXTBUFFER_H
#define EDITOR_TEXTBUFFER_H

#include <vector>

#include "Core/Language/LanguageBase.h"
#include "Core/Line.h"
#include "Core/Cursor.h"
#include <memory>

namespace gedit {
    class TextBuffer {
    public:
        using Ref = std::shared_ptr<TextBuffer>;
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

        void CopyRegionToString(std::string &outText, const Cursor &start, const Cursor &end);

        const std::string &Name() const {
            return name;
        }

        void SetLanguage(LanguageBase *newLanguage) {
            language = newLanguage;
            Reparse();
        }

        bool HaveLanguage() { return language!= nullptr; }
        LanguageBase &LangParser() { return *language; }

        void Reparse();


    private:
        std::string name;
        std::vector<Line *> lines;
        LanguageBase *language = nullptr;

        // Do not put the edit controller here - might make sense, but it will cause problems later
        // Example: split-window editing with same file => won't work
        // EditController *editController = nullptr;
    };
}


#endif //EDITOR_TEXTBUFFER_H
