//
// Created by gnilk on 29.01.23.
//

#ifndef EDITOR_LANGTOKEN_H
#define EDITOR_LANGTOKEN_H

#include "Core/Line.h"
#include "Core/Language/LanguageTokenClass.h"

namespace gnilk {


    bool IsLanguageTokenClass(int num);
    const std::string &LanguageTokenClassToString(kLanguageTokenClass tokenClass);

    class LangToken {
    public:
        std::string string;     // The token
        int idxOrigStr;         // The position/index in original string
        kLanguageTokenClass classification;     // Classification (keyword, user, operator, reserved, comment, etc...)

        const std::string &String() const { return string; }

        bool operator == (const kLanguageTokenClass otherClassification) const {
            return classification == otherClassification;
        }

        // FIXME: Move this out of here
        static void ToLineAttrib(std::vector<gedit::Line::LineAttrib> &attribs, std::vector<LangToken> &tokens);
    };
}

#endif //EDITOR_LANGTOKEN_H
