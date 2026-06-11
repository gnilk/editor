//
// Created by gnilk on 11.06.26.
//
// The LOGIC half of auto-indentation: pure decisions, sibling to AutoPairEngine. Two entry points -
// OnNewLine (the indent of the line created on Enter) and OnInsertChar (electric dedent: typing a closer
// snaps the current line back a level). Everything it needs is in Context (a u32string_view + cursor +
// tabSize + the token class at the cursor), so it is unit-tested directly with a hand-built IndentTable -
// no UI / Line / tokenizer dependency. The model is RELATIVE: indent derives from the reference line's
// leading whitespace plus the trigger deltas, not an absolute brace-depth. See docs/indent-plan.md.
//

#ifndef EDITOR_INDENTENGINE_H
#define EDITOR_INDENTENGINE_H

#include <string_view>

#include "Core/Language/IndentTable.h"
#include "Core/Language/LanguageTokenClass.h"

namespace gedit {

    class IndentEngine {
    public:
        enum class kIndentAction {
            kNone,        // leave indentation to the default path
            kSetIndent,   // set the target line's leading whitespace to indentLevel * tabSize spaces
        };
        struct Action {
            kIndentAction type = kIndentAction::kNone;
            int indentLevel = 0;            // target indent in TAB UNITS (multiply by tabSize for spaces)
            // Newline-only: the '{|}' expansion. An extra empty line at blankLineLevel sits between, and the
            // closer line lands at indentLevel.
            bool insertBlankLine = false;
            int blankLineLevel = 0;
        };
        // For OnNewLine, lineText[0..cursorX) is the part staying (the reference line) and lineText[cursorX..)
        // is the content moving down - one field captures both halves. For OnInsertChar, lineText is the
        // current line and cursorX is where the char would land.
        struct Context {
            const IndentTable *table = nullptr;
            std::u32string_view lineText;
            int cursorX = 0;
            int tabSize = 4;
            kLanguageTokenClass tokenClassAtCursor = kLanguageTokenClass::kRegular;
        };

        static Action OnNewLine(const Context &ctx);
        static Action OnInsertChar(const Context &ctx, char32_t typed);

    private:
        static int LeadingIndentLevel(std::u32string_view text, int tabSize);
        static char32_t LastNonSpace(std::u32string_view text);
        static char32_t FirstNonSpace(std::u32string_view text);
        static bool IsAllWhitespace(std::u32string_view text);
    };
}

#endif //EDITOR_INDENTENGINE_H
