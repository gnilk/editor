//
// Created by gnilk on 11.11.26.
//

#ifndef EDITOR_YAMLLANGUAGE_H
#define EDITOR_YAMLLANGUAGE_H

#include "Core/Language/LanguageBase.h"

namespace gedit {
    // YAML (.yml / .yaml) syntax support. See docs/support-yaml.md for the plan.
    // Phase 1: comments and quoted strings in the tokenizer. Keys, list markers and typed scalars (the
    // line-anchored part) come in Phase 2 through OnPostProcessParsedLine.
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
    };
}

#endif //EDITOR_YAMLLANGUAGE_H
