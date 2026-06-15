//
// Created by gnilk on 15.06.26.
//

#ifndef EDITOR_MARKDOWNLANGUAGE_H
#define EDITOR_MARKDOWNLANGUAGE_H

#include "Core/Language/LanguageBase.h"

namespace gedit {
    // Markdown (.md) syntax support. See docs/support-markdown.md for the full plan.
    // v1 skeleton: language detection + an (intentionally empty) tokenizer state setup.
    class MarkdownLanguage : public LanguageBase {
    public:
        MarkdownLanguage() = default;
        virtual ~MarkdownLanguage() = default;

        static LanguageBase::Ref Create() {
            return std::make_shared<MarkdownLanguage>();
        }

        bool Initialize() override;
        const std::u32string &Identifier() override {
            static std::u32string markdownIdentifier = U"markdown";
            return markdownIdentifier;
        }

        // §2.B: line-anchored block syntax (headings, blockquotes, list markers, thematic breaks). The
        // tokenizer (§2.A) has no line-start awareness, so this runs as a post-pass over each parsed line.
        void OnPostProcessParsedLine(Line::Ref line) override;
    };
}

#endif //EDITOR_MARKDOWNLANGUAGE_H
