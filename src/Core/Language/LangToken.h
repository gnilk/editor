//
// Created by gnilk on 29.01.23.
//

#ifndef EDITOR_LANGTOKEN_H
#define EDITOR_LANGTOKEN_H

namespace gnilk {
// Extend this as we go along...
    enum class kLanguageTokenClass : int {
        kUnknown = 0,
        kRegular = 1,
        kOperator = 2,
        kKeyword = 3,
        kKnownType = 4,
        // FIXME: Implement this => Require custom matching for identifiers => see note in "identifierlist"
        kNumber = 5,
        kString = 6,
        kLineComment = 7,
        kBlockComment = 8,
        kCommentedText = 9,
        kLastTokenClass = 10,         // this is used as numeric detection of the last token class
        kFunky = 196,       // USED for debugging..
    };



    bool IsLanguageTokenClass(int num);
    const std::string &LanguageTokenClassToString(kLanguageTokenClass tokenClass);


    class LangToken {
    public:
        std::string string;     // The token
        int idxOrigStr;         // The position/index in original string
        kLanguageTokenClass classification;     // Classification (keyword, user, operator, reserved, comment, etc...)

        const std::string &String() const { return string; }
    };
}

#endif //EDITOR_LANGTOKEN_H
