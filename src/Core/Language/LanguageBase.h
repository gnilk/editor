//
// Created by gnilk on 29.01.23.
//

#ifndef EDITOR_LANGUAGEBASE_H
#define EDITOR_LANGUAGEBASE_H

#include "LangLineTokenizer.h"
#include "Core/Cursor.h"
#include "Core/Line.h"

namespace gedit {
    class LanguageBase {
    public:
        LanguageBase() = default;
        virtual ~LanguageBase() = default;

        // Implement this and setup the tokenizer
        virtual bool Initialize() { return false; }; // You should really implement this...
        virtual const std::string &Identifier() {
            static std::string noIdentifier = "default";
            return noIdentifier;
        }
        LangLineTokenizer &Tokenizer() { return tokenizer; }

        // Not too key on adding dependencies on these things
        virtual void OnPreInsertChar(Cursor &cursor, Line *line, int ch) {}
        virtual void OnPostInsertChar(Cursor &cursor, Line *line, int ch) {}
    protected:
        LangLineTokenizer tokenizer;
    };
}


#endif //EDITOR_LANGUAGEBASE_H
