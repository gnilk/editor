//
// Created by gnilk on 11.06.26.
//
#include "IndentEngine.h"

#include <algorithm>

#include "Core/Line.h"
#include "Core/Language/SyntaxRegion.h"

using namespace gedit;

// On Enter, compute the indent level of the new (split-off) line, relative to the line being left. The new
// line indents one level when the reference line opens a block and dedents one when the moved content closes
// one; both opening and closing (cursor between '{' and '}') asks for the three-line expansion.
IndentEngine::Action IndentEngine::OnNewLine(const Context &ctx) {
    int tabSize = (ctx.tabSize > 0) ? ctx.tabSize : 4;
    auto before = ctx.lineText.substr(0, (size_t)std::max(0, ctx.cursorX));
    auto after = (ctx.cursorX >= 0 && (size_t)ctx.cursorX < ctx.lineText.size())
                     ? ctx.lineText.substr((size_t)ctx.cursorX)
                     : std::u32string_view();

    int baseLevel = LeadingIndentLevel(before, tabSize);

    Action action;
    action.type = kIndentAction::kSetIndent;

    // No language rules -> just copy the reference line's indent (the dumb-editor fallback).
    if ((ctx.table == nullptr) || ctx.table->IsEmpty()) {
        action.indentLevel = baseLevel;
        return action;
    }

    bool opens = ctx.table->IsIndentAfter(LastNonSpace(before));
    bool closesNext = ctx.table->IsDedentOn(FirstNonSpace(after));

    // Cursor sits between an opener and its closer -> expand into three lines: an indented empty line in
    // the middle, the closer line back at the base level.
    if (opens && closesNext) {
        action.indentLevel = baseLevel;
        action.insertBlankLine = true;
        action.blankLineLevel = baseLevel + 1;
        return action;
    }

    int level = baseLevel + (opens ? 1 : 0) - (closesNext ? 1 : 0);
    action.indentLevel = (level < 0) ? 0 : level;
    return action;
}

// On a typed character, electrically dedent the current line: typing a closer as the first non-blank char
// of a line snaps it back one level (re-homes the dropped '}'-dedent). Suppressed inside a string/comment.
IndentEngine::Action IndentEngine::OnInsertChar(const Context &ctx, char32_t typed) {
    Action none;
    if ((ctx.table == nullptr) || ctx.table->IsEmpty()) {
        return none;
    }
    if (!ctx.table->IsDedentOn(typed)) {
        return none;
    }
    if (ctx.table->IsSuppressed(ctx.tokenClassAtCursor)) {
        return none;
    }
    int tabSize = (ctx.tabSize > 0) ? ctx.tabSize : 4;
    auto before = ctx.lineText.substr(0, (size_t)std::max(0, ctx.cursorX));
    // Only when the closer would be the FIRST non-blank on the line (a manual block-close), never mid-line.
    if (!IsAllWhitespace(before)) {
        return none;
    }
    int currentLevel = LeadingIndentLevel(before, tabSize);
    Action action;
    action.type = kIndentAction::kSetIndent;
    action.indentLevel = (currentLevel > 0) ? (currentLevel - 1) : 0;
    return action;
}

// Reformat a range of lines: re-derive each line's indent from scratch via a stepped walk, seeded by the
// trusted anchor level (the line above the range). Unlike OnNewLine this is stateful across lines - it does
// NOT trust any line's own current whitespace (that is the thing being fixed); the running depth carries the
// nesting forward. With no language rules every line keeps its existing indent (plaintext is left alone).
std::vector<int> IndentEngine::ReindentRange(const RangeContext &ctx) {
    int tabSize = (ctx.tabSize > 0) ? ctx.tabSize : 4;
    std::vector<int> levels;
    levels.reserve(ctx.lines.size());

    if ((ctx.table == nullptr) || ctx.table->IsEmpty()) {
        for (const auto &line : ctx.lines) {
            levels.push_back(LeadingIndentLevel(line, tabSize));
        }
        return levels;
    }

    int depth = (ctx.anchorLevel < 0) ? 0 : ctx.anchorLevel;
    for (size_t i = 0; i < ctx.lines.size(); i++) {
        const auto &line = ctx.lines[i];
        bool haveSyn = (i < ctx.syntax.size());
        // A line inside a multi-line construct (block comment / multi-line string) is left byte-faithful and
        // contributes no structure - rewriting its indent would corrupt a raw string and its braces are text.
        if (haveSyn && ctx.syntax[i].isFrozen) {
            levels.push_back(-1);
            continue;
        }
        // A blank line holds the prevailing level and does not move the running depth.
        if (IsAllWhitespace(line)) {
            levels.push_back(depth);
            continue;
        }
        // Trigger chars: token-resolved when supplied (braces in strings/comments are masked to 0), else the
        // raw first/last non-space char of the text. A 0 trigger (e.g. a comment-only line) is neutral.
        char32_t firstCh = haveSyn ? ctx.syntax[i].firstStructural : FirstNonSpace(line);
        char32_t lastCh = haveSyn ? ctx.syntax[i].lastStructural : LastNonSpace(line);
        // A line whose first structural char is a closer sits one level out from the running depth.
        bool closesHere = (firstCh != 0) && ctx.table->IsDedentOn(firstCh);
        int lineLevel = closesHere ? std::max(0, depth - 1) : depth;
        levels.push_back(lineLevel);
        // A line ending with a structural opener pushes the next line in one level.
        bool opensNext = (lastCh != 0) && ctx.table->IsIndentAfter(lastCh);
        depth = lineLevel + (opensNext ? 1 : 0);
    }
    return levels;
}

// Token class of the char at (line, x): the span whose start is the greatest idxOrigString <= x (attribs are
// ascending). kRegular when the line is unparsed - so an unparsed brace still counts (degrades to naive
// matching). NB: does NOT use Line::AttributeAt - that returns the FIRST span for any x in the last span,
// which would misread a brace in a trailing comment (always the last span) as code.
static kLanguageTokenClass TokenClassAtChar(const Line::Ref &line, int x) {
    auto &attribs = line->Attributes();
    if (attribs.empty()) {
        return kLanguageTokenClass::kRegular;
    }
    kLanguageTokenClass cls = kLanguageTokenClass::kRegular;
    for (const auto &a : attribs) {
        if (a.idxOrigString > x) {
            break;
        }
        cls = a.tokenClass;
    }
    return cls;
}

// The '{ }' pair enclosing the cursor: scan backward for the nearest unmatched '{' and forward for its
// matching '}', counting brace depth and skipping braces inside strings/comments (suppressIn token classes).
IndentEngine::BlockRange IndentEngine::FindEnclosingBlock(const std::vector<Line::Ref> &lines, size_t cursorY,
                                                         int cursorX, const IndentTable *table) {
    BlockRange result;
    if (lines.empty() || (cursorY >= lines.size())) {
        return result;
    }
    auto isCountingBrace = [&](size_t y, int x, char32_t want) -> bool {
        const auto &buf = lines[y]->Buffer();
        if ((x < 0) || (x >= (int)buf.size()) || (buf[x] != want)) {
            return false;
        }
        return (table == nullptr) || !table->IsSuppressed(TokenClassAtChar(lines[y], x));
    };

    // Backward: the nearest enclosing '{' - any '}' seen first must be matched by its own '{' before it.
    bool foundOpen = false;
    size_t openY = 0;
    {
        int depth = 0;
        size_t y = cursorY;
        int x = cursorX - 1;
        while (true) {
            int lineLen = (int)lines[y]->Buffer().size();
            if (x > lineLen - 1) {
                x = lineLen - 1;
            }
            for (; x >= 0; x--) {
                if (isCountingBrace(y, x, U'}')) {
                    depth++;
                } else if (isCountingBrace(y, x, U'{')) {
                    if (depth == 0) {
                        foundOpen = true;
                        openY = y;
                        break;
                    }
                    depth--;
                }
            }
            if (foundOpen || (y == 0)) {
                break;
            }
            y--;
            x = (int)lines[y]->Buffer().size() - 1;
        }
    }
    if (!foundOpen) {
        return result;
    }

    // Forward: the matching '}' - any '{' seen first must be closed by its own '}' before it.
    bool foundClose = false;
    size_t closeY = 0;
    {
        int depth = 0;
        size_t y = cursorY;
        int x = cursorX;
        while (y < lines.size()) {
            int lineLen = (int)lines[y]->Buffer().size();
            for (; x < lineLen; x++) {
                if (isCountingBrace(y, x, U'{')) {
                    depth++;
                } else if (isCountingBrace(y, x, U'}')) {
                    if (depth == 0) {
                        foundClose = true;
                        closeY = y;
                        break;
                    }
                    depth--;
                }
            }
            if (foundClose) {
                break;
            }
            y++;
            x = 0;
        }
    }
    if (!foundClose) {
        return result;
    }

    result.found = true;
    result.openY = openY;
    result.closeY = closeY;
    return result;
}

// The trusted seed level for reformatting a range that starts at startY: walk back to the nearest clean line
// (lexer state baseline, i.e. not inside a multi-line comment/string) and take its leading indent, plus one
// level if that line opens a block. This is read only - the anchor line is never rewritten.
int IndentEngine::FindAnchorLevel(const std::vector<Line::Ref> &lines, size_t startY,
                                  const IndentTable *table, int tabSize) {
    if ((startY == 0) || lines.empty()) {
        return 0;
    }
    size_t idxAnchor = SyntaxRegion::SeekCleanBackward(lines, startY);
    const auto &anchorText = lines[idxAnchor]->Buffer();
    int level = LeadingIndentLevel(anchorText, tabSize);
    if ((table != nullptr) && !table->IsEmpty() && table->IsIndentAfter(LastNonSpace(anchorText))) {
        level += 1;
    }
    return level;
}

// Forward-extend the reformat range so a selection ending inside a multi-line construct is completed: the
// clean-line seek lands on the first line PAST the construct, so the last line still inside it is one before.
// When endY is not mid-construct this is a no-op (returns endY).
size_t IndentEngine::FindRangeEnd(const std::vector<Line::Ref> &lines, size_t endY) {
    if (lines.empty()) {
        return endY;
    }
    size_t cleanAfter = SyntaxRegion::SeekCleanForward(lines, endY);
    size_t extended = (cleanAfter > 0) ? (cleanAfter - 1) : endY;
    if (extended < endY) {
        extended = endY;
    }
    if (extended >= lines.size()) {
        extended = lines.size() - 1;
    }
    return extended;
}

// Leading whitespace measured in tab units: a space advances one column, a tab to the next tab stop; the
// column count divided by tabSize is the indent level.
int IndentEngine::LeadingIndentLevel(std::u32string_view text, int tabSize) {
    int cols = 0;
    for (char32_t ch : text) {
        if (ch == U' ') {
            cols++;
        } else if (ch == U'\t') {
            cols += tabSize - (cols % tabSize);
        } else {
            break;
        }
    }
    return cols / tabSize;
}

char32_t IndentEngine::LastNonSpace(std::u32string_view text) {
    for (auto it = text.rbegin(); it != text.rend(); ++it) {
        if ((*it != U' ') && (*it != U'\t')) {
            return *it;
        }
    }
    return 0;
}

char32_t IndentEngine::FirstNonSpace(std::u32string_view text) {
    for (char32_t ch : text) {
        if ((ch != U' ') && (ch != U'\t')) {
            return ch;
        }
    }
    return 0;
}

bool IndentEngine::IsAllWhitespace(std::u32string_view text) {
    for (char32_t ch : text) {
        if ((ch != U' ') && (ch != U'\t')) {
            return false;
        }
    }
    return true;
}
