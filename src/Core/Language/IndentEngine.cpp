//
// Created by gnilk on 11.06.26.
//
#include "IndentEngine.h"

#include <algorithm>

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
    for (const auto &line : ctx.lines) {
        // A blank line holds the prevailing level and does not move the running depth.
        if (IsAllWhitespace(line)) {
            levels.push_back(depth);
            continue;
        }
        // A line whose first non-blank is a closer sits one level out from the running depth.
        bool closesHere = ctx.table->IsDedentOn(FirstNonSpace(line));
        int lineLevel = closesHere ? std::max(0, depth - 1) : depth;
        levels.push_back(lineLevel);
        // A line ending with an opener pushes the next line in one level.
        bool opensNext = ctx.table->IsIndentAfter(LastNonSpace(line));
        depth = lineLevel + (opensNext ? 1 : 0);
    }
    return levels;
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
