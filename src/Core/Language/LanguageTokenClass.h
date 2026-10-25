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
        kImport = 16,
        kPreProcessor = 17,
        kMacroIdentifier = 18,
        // Document / markup classes (markdown, and any future prose/markup language). Kept generic on
        // purpose - these name document structure, not a specific syntax.
        kHeading = 19,          // section heading
        kStrong = 20,           // strong emphasis (bold)
        kEmphasis = 21,         // emphasis (italic)
        kCode = 22,             // inline or fenced verbatim code (distinct from kString)
        kListMarker = 23,       // list bullet / ordinal marker
        kBlockQuote = 24,       // quoted block text
        kLink = 25,             // hyperlink / reference
        kRule = 26,             // horizontal rule / thematic break
        // Semantic classes - an identifier the code index knows (docs/code-index.md §10.1). The tokenizer
        // never produces these; a 'regular' identifier is reclassified after the line is parsed.
        kUserType = 27,         // class / struct / union / enum / alias
        kFunction = 28,         // function or method
        kMember = 29,           // data member
        kMacro = 30,            // a #define'd name where it is used
        kEnumValue = 31,        // enumerator
        kNamespace = 32,
        kLastTokenClass = 33,         // this is used as numeric detection of the last token class
        kFunky = 196,       // USED for debugging..
    };
}

#endif //EDITOR_LANGUAGETOKENCLASS_H
