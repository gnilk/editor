//
// Created by gnilk on 05.02.23.
//

#ifndef EDITOR_LANGUAGETOKENCLASS_H
#define EDITOR_LANGUAGETOKENCLASS_H

// Extend this as we go along...
namespace gedit {
    enum class kLanguageTokenClass : int {
        kUnknown = 0,
        kRegular = 1,
        kOperator = 2,
        kKeyword = 3,
        kSeparator = 4,
        kKnownType = 5,
        // FIXME: Implement this => Require custom matching for identifiers => see note in "identifierlist"
        kNumber = 6,
        kString = 7,
        kLineComment = 8,
        kBlockComment = 9,
        kCommentedText = 10,
        kCodeBlockStart = 11,
        kCodeBlockEnd = 12,
        kArrayStart = 13,
        kArrayEnd = 14,
        kChar = 15,
        kLastTokenClass = 16,         // this is used as numeric detection of the last token class
        kFunky = 196,       // USED for debugging..
    };
}

#endif //EDITOR_LANGUAGETOKENCLASS_H
