//
// Created by gnilk on 27.04.23.
//

#ifndef EDITOR_JSONLANGUAGE_H
#define EDITOR_JSONLANGUAGE_H

#include "Core/Language/LanguageBase.h"

namespace gedit {
    class JSONLanguage : public LanguageBase {
    public:
        JSONLanguage() = default;
        virtual ~JSONLanguage() = default;

        static LanguageBase::Ref Create() {
            return std::make_shared<JSONLanguage>();
        }


        bool Initialize() override;
        const std::string &Identifier() override {
            static std::string cppIdentifier = "json";
            return cppIdentifier;
        }

        kInsertAction OnPreInsertChar(Cursor &cursor, Line::Ref line, int ch) override;
        void  OnPostInsertChar(Cursor &cursor, Line::Ref line, int ch) override;

    };
}


#endif //EDITOR_JSONLANGUAGE_H
