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

#include <cstddef>
#include <string_view>
#include <vector>

#include "Core/Line.h"
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

        // Reformat (reindent-a-range): the range-shaped sibling of Context. The decision is inherently
        // cross-line, so it carries N lines and an anchor (the trusted indent level of the line ABOVE the
        // range, 0 at file top) rather than one line + cursor. Pure: views + table, no Line dependency.
        struct RangeContext {
            const IndentTable *table = nullptr;
            std::vector<std::u32string_view> lines;     // the lines to reformat, in order
            int anchorLevel = 0;                        // indent level (tab units) of the line above the range
            int tabSize = 4;
        };

        static Action OnNewLine(const Context &ctx);
        static Action OnInsertChar(const Context &ctx, char32_t typed);
        // Returns one absolute indent level (tab units) per input line - a stepped walk seeded by anchorLevel.
        // Empty table => each line's existing leading-indent level, unchanged (plaintext no-op).
        static std::vector<int> ReindentRange(const RangeContext &ctx);

        // Region search for reformat (uses the shared SyntaxRegion clean-line seek over live lines - the only
        // part of the engine that is not pure; the live lines carry the stamped lexer state-stack depth).
        // FindAnchorLevel: the trusted seed level for reformatting [startY..] - the clean line above the range,
        // its leading indent +1 if it opens a block. 0 at file top. Read-only: never rewrites the anchor.
        // FindRangeEnd: forward-extend endY through any multi-line construct it ends inside (so a selection
        // ending mid-comment/string is completed, not half-reformatted). Returns >= endY, < lines.size().
        static int FindAnchorLevel(const std::vector<Line::Ref> &lines, size_t startY,
                                   const IndentTable *table, int tabSize);
        static size_t FindRangeEnd(const std::vector<Line::Ref> &lines, size_t endY);

    private:
        static int LeadingIndentLevel(std::u32string_view text, int tabSize);
        static char32_t LastNonSpace(std::u32string_view text);
        static char32_t FirstNonSpace(std::u32string_view text);
        static bool IsAllWhitespace(std::u32string_view text);
    };
}

#endif //EDITOR_INDENTENGINE_H
