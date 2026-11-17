//
// Created by gnilk on 29.01.23.
//

#ifndef EDITOR_LANGUAGEBASE_H
#define EDITOR_LANGUAGEBASE_H

#include "LangLineTokenizer.h"
#include "Core/Graphics/Cursor.h"
#include "Core/Line.h"
#include <bitset>
#include <memory>
#include "Core/Config/ConfigNode.h"

namespace gedit {
    class LanguageBase : public ConfigNode {
    public:
        using Ref = std::shared_ptr<LanguageBase>;
    public:
        LanguageBase() {
            // Bridge the tokenizer's per-line hook to the virtual post-process. Resolves to the most-
            // derived override at parse time (base is a no-op), so only languages that need line-anchored
            // markup (e.g. markdown) do any work here.
            tokenizer.SetPostLineCallback([this](const Line::Ref &line) {
                OnPostProcessParsedLine(line);
            });
        }
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

        // Used by single line parsers (like 'makefile') to enhance the attributes after initial parsing
        virtual void OnPostProcessParsedLine(Line::Ref line) {}

        // On Enter: the column the new line's indent is measured from, given the text before the cursor.
        // -1 = its leading whitespace (the default). YAML: '- key: v' lines up the next line with 'key'.
        virtual int GetContinuationColumn(std::u32string_view lineBeforeCursor) { return -1; }

        // On Enter: whether the text before the cursor opens a block the indent table's last-char triggers can't see.
        // YAML: a block scalar header with a chomping indicator ('run: |-') or a comment after it ('run: |  # x').
        virtual bool OpensBlockAtEnd(std::u32string_view lineBeforeCursor) { return false; }

        // The config name this language was configured from (the 'languages:' key, e.g. "cpp") - also the
        // key into autopairs.yml. Empty if the language never called ConfigFromNodeName.
        const std::string &GetConfigNodeName() const { return configNodeName; }

        // Common Language Settings
        int GetTabSize();

        // A class the code index gives C++ (function, enum_value, ...) that this language's own tokenizer produces -
        // CMake's commands are functions by syntax. The semantic names leave those alone (TextBuffer::ApplySemanticNames).
        bool IsSyntaxSemanticClass(kLanguageTokenClass tokenClass) const {
            auto idx = static_cast<size_t>(tokenClass);
            return (idx < syntaxSemanticClasses.size()) && syntaxSemanticClasses.test(idx);
        }
    protected:
        void ConfigFromNodeName(const std::string nodeName);
        void MarkSyntaxSemanticClass(kLanguageTokenClass tokenClass) {
            auto idx = static_cast<size_t>(tokenClass);
            if (idx < syntaxSemanticClasses.size()) {
                syntaxSemanticClasses.set(idx);
            }
        }

    protected:
        LangLineTokenizer tokenizer;
        std::string configNodeName;
        std::bitset<static_cast<size_t>(kLanguageTokenClass::kLastTokenClass)> syntaxSemanticClasses;
    };
}


#endif //EDITOR_LANGUAGEBASE_H
