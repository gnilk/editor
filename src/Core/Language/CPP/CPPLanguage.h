//
// Created by gnilk on 29.01.23.
//

#ifndef EDITOR_CPPLANGUAGE_H
#define EDITOR_CPPLANGUAGE_H

#include "Core/Language/LanguageBase.h"

namespace gedit {
    class CPPLanguage : public LanguageBase {
    public:
        CPPLanguage() = default;
        virtual ~CPPLanguage() = default;

        bool Initialize() override;
        const std::string &Identifier() override {
            static std::string cppIdentifier = "c/c++";
            return cppIdentifier;
        }

        void OnPreInsertChar(Cursor &cursor, Line::Ref line, int ch) override;
        void OnPostInsertChar(Cursor &cursor, Line::Ref line, int ch) override;
    private:

// declare in-string operators
    };
}


#endif //EDITOR_CPPLANGUAGE_H
