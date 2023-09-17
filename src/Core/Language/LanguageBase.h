//
// Created by gnilk on 29.01.23.
//

#ifndef EDITOR_LANGUAGEBASE_H
#define EDITOR_LANGUAGEBASE_H

#include "LangLineTokenizer.h"
#include "Core/Cursor.h"
#include "Core/Line.h"
#include <memory>
#include "Core/Config/ConfigNode.h"

namespace gedit {
    class LanguageBase : public ConfigNode {
    public:
        enum class kInsertAction {
            kDefault,
            kNoInsert,
            kNewLine,
        };
    public:
        using Ref = std::shared_ptr<LanguageBase>;
    public:
        LanguageBase() = default;
        virtual ~LanguageBase() = default;

        // Implement this and setup the tokenizer
        virtual bool Initialize() { return false; }; // You should really implement this...
        virtual const std::u32string &Identifier() {
            static std::u32string noIdentifier = U"default";
            return noIdentifier;
        }
        LangLineTokenizer &Tokenizer() { return tokenizer; }
        virtual const std::u32string &GetLineComment() {
            static std::u32string lineCommentPrefix = U"";
            return lineCommentPrefix;
        }

        // Not too key on adding dependencies on these things
        virtual kInsertAction OnPreInsertChar(Cursor &cursor, Line::Ref line, char32_t ch) { return kInsertAction::kDefault; }
        virtual kInsertAction OnPreCreateNewLine(const Line::Ref newLine) { return kInsertAction::kDefault; }
        virtual void OnPostInsertChar(Cursor &cursor, Line::Ref line, char32_t ch) { }

        // Common Language Settings
        int GetTabSize();
    protected:
        void ConfigFromNodeName(const std::string nodeName);

    protected:
        LangLineTokenizer tokenizer;
    };
}


#endif //EDITOR_LANGUAGEBASE_H
