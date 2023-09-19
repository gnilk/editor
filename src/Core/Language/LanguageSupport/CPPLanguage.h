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

        static LanguageBase::Ref Create() {
            return std::make_shared<CPPLanguage>();
        }

        const std::u32string &GetLineComment() override {
            static std::u32string lineCommentPrefix = U"//";
            return lineCommentPrefix;
        }


        bool Initialize() override;
        const std::u32string &Identifier() override {
            static std::u32string cppIdentifier = U"c/c++";
            return cppIdentifier;
        }

        kInsertAction OnPreCreateNewLine(const Line::Ref newLine) override;
        kInsertAction OnPreInsertChar(Cursor &cursor, Line::Ref line, char32_t ch) override;
        void OnPostInsertChar(Cursor &cursor, Line::Ref line, char32_t ch) override;
    private:

// declare in-string operators
    };
}


#endif //EDITOR_CPPLANGUAGE_H
