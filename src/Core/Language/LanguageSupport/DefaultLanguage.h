//
// Created by gnilk on 28.04.23.
//

#ifndef EDITOR_DEFAULTLANGUAGE_H
#define EDITOR_DEFAULTLANGUAGE_H


#include "Core/Language/LanguageBase.h"

namespace gedit {
    class DefaultLanguage : public LanguageBase {
    public:
        DefaultLanguage() = default;
        virtual ~DefaultLanguage() = default;

        static LanguageBase::Ref Create() {
            return std::make_shared<DefaultLanguage>();
        }

        bool Initialize() override;

        const std::u32string &Identifier() override {
            static std::u32string identifier = U"default";
            return identifier;
        }
    };
}


#endif //EDITOR_DEFAULTLANGUAGE_H
