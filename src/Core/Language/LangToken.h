//
// Created by gnilk on 29.01.23.
//

#ifndef EDITOR_LANGTOKEN_H
#define EDITOR_LANGTOKEN_H

#include <functional>
#include <string>

#include "Core/Line.h"
#include "Core/Language/LanguageTokenClass.h"

namespace gedit {

    bool IsLanguageTokenClass(int num);
    const std::string &LanguageTokenClassToString(kLanguageTokenClass tokenClass);
    // The theme key that colours a class: its own, or the closest one the theme has
    std::string ThemeKeyForTokenClass(kLanguageTokenClass tokenClass, const std::function<bool(const std::string &)> &hasColor);

    class LangToken {
    public:
        std::u32string string;     // The token
        int idxOrigStr;         // The position/index in original string
        kLanguageTokenClass classification;     // Classification (keyword, user, operator, reserved, comment, etc...)

        const std::u32string &String() const { return string; }

        bool operator == (const kLanguageTokenClass otherClassification) const {
            return classification == otherClassification;
        }
        bool operator != (const kLanguageTokenClass otherClassification) const {
            return classification != otherClassification;
        }

        // FIXME: Move this out of here
        static void ToLineAttrib(std::vector<gedit::Line::LineAttrib> &attribs, std::vector<LangToken> &tokens);
    };
}

#endif //EDITOR_LANGTOKEN_H
