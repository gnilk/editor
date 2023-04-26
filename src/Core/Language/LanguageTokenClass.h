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
        kKnownType = 4,
        // FIXME: Implement this => Require custom matching for identifiers => see note in "identifierlist"
        kNumber = 5,
        kString = 6,
        kLineComment = 7,
        kBlockComment = 8,
        kCommentedText = 9,
        kCodeBlockStart = 10,
        kCodeBlockEnd = 11,
        kLastTokenClass = 12,         // this is used as numeric detection of the last token class
        kFunky = 196,       // USED for debugging..
    };
}

#endif //EDITOR_LANGUAGETOKENCLASS_H
