//
// Created by gnilk on 29.01.23.
//

#ifndef EDITOR_LANGUAGEBASE_H
#define EDITOR_LANGUAGEBASE_H

#include "LangLineTokenizer.h"
#include "Core/Graphics/Cursor.h"
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

        // Newline behaviour hook (indent / auto-newline next to '}') - used by Document::NewLine. The old
        // per-char insert hooks (OnPreInsertChar/OnPostInsertChar) were removed in favour of the
        // data-driven AutoPairEngine; this one stays until the indent engine subsumes it.
        virtual kInsertAction OnPreCreateNewLine(const Line::Ref newLine) { return kInsertAction::kDefault; }
        // Used by single line parsers (like 'makefile') to enhance the attributes after initial parsing
        virtual void OnPostProcessParsedLine(Line::Ref line) {}

        // The config name this language was configured from (the 'languages:' key, e.g. "cpp") - also the
        // key into autopairs.yml. Empty if the language never called ConfigFromNodeName.
        const std::string &GetConfigNodeName() const { return configNodeName; }

        // Common Language Settings
        int GetTabSize();
    protected:
        void ConfigFromNodeName(const std::string nodeName);

    protected:
        LangLineTokenizer tokenizer;
        std::string configNodeName;
    };
}


#endif //EDITOR_LANGUAGEBASE_H
