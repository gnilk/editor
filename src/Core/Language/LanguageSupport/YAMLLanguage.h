//
// Created by gnilk on 11.11.26.
//

#ifndef EDITOR_YAMLLANGUAGE_H
#define EDITOR_YAMLLANGUAGE_H

#include "Core/Language/LanguageBase.h"

namespace gedit {
    // YAML (.yml / .yaml) syntax support. See docs/support-yaml.md for the plan.
    // The tokenizer does comments and quoted strings (§2.A); keys, list markers, document markers, typed
    // scalars, anchors/aliases/tags and one-line flow collections are line-anchored and live in the
    // post-pass (§2.B).
    class YAMLLanguage : public LanguageBase {
    public:
        YAMLLanguage() = default;
        virtual ~YAMLLanguage() = default;

        static LanguageBase::Ref Create() {
            return std::make_shared<YAMLLanguage>();
        }

        bool Initialize() override;
        const std::u32string &Identifier() override {
            static std::u32string yamlIdentifier = U"yaml";
            return yamlIdentifier;
        }
        const std::u32string &GetLineComment() override {
            static std::u32string lineCommentPrefix = U"#";
            return lineCommentPrefix;
        }

        void OnPostProcessParsedLine(Line::Ref line) override;
    };
}

#endif //EDITOR_YAMLLANGUAGE_H
