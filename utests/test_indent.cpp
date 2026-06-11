//
// Created by gnilk on 11.06.26.
//
// Step 0/1 of the indent engine (docs/indent-plan.md): the pure decision logic. IndentEngine takes a
// primitive Context (table + line text + cursor + tabSize + token-class) and returns one Action, so it is
// tested directly with a hand-built IndentTable - no live editor, no tokenizer, no YAML yet. These cases
// ARE the v1 rule contract: newline indent after an opener, copy-on-neutral, dedent when the moved content
// closes a block, the '{|}' three-line expansion, electric dedent on a whitespace-only line, electric
// suppressed mid-line / inside a comment, and empty-table => copy-previous-indent + never electric.
//
#include <testinterface.h>

#include <string>

#include "Core/Language/IndentEngine.h"
#include "Core/Language/IndentTable.h"
#include "Core/Language/LanguageTokenClass.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_indent(ITesting *t);
DLL_EXPORT int test_indent_newline_indent(ITesting *t);
DLL_EXPORT int test_indent_newline_copy(ITesting *t);
DLL_EXPORT int test_indent_newline_dedent(ITesting *t);
DLL_EXPORT int test_indent_newline_expand(ITesting *t);
DLL_EXPORT int test_indent_electric(ITesting *t);
DLL_EXPORT int test_indent_electric_suppress(ITesting *t);
DLL_EXPORT int test_indent_emptytable(ITesting *t);
}

using kIA = IndentEngine::kIndentAction;
using kTC = gedit::kLanguageTokenClass;

static const int kTab = 4;

// A C-like table: indent after the openers, dedent on the closers, electric off in strings/comments.
static IndentTable CTable() {
    IndentTable tbl;
    tbl.indentAfter = {U'{', U'[', U'('};
    tbl.dedentOn = {U'}', U']', U')'};
    tbl.suppressIn = {kTC::kString, kTC::kChar, kTC::kLineComment, kTC::kBlockComment, kTC::kCommentedText};
    return tbl;
}

static IndentEngine::Action NL(const IndentTable &tbl, const std::u32string &line, int x) {
    IndentEngine::Context c;
    c.table = &tbl;
    c.lineText = line;
    c.cursorX = x;
    c.tabSize = kTab;
    return IndentEngine::OnNewLine(c);
}

static IndentEngine::Action Ins(const IndentTable &tbl, const std::u32string &line, int x, char32_t typed,
                                kTC cls = kTC::kRegular) {
    IndentEngine::Context c;
    c.table = &tbl;
    c.lineText = line;
    c.cursorX = x;
    c.tabSize = kTab;
    c.tokenClassAtCursor = cls;
    return IndentEngine::OnInsertChar(c, typed);
}

DLL_EXPORT int test_indent(ITesting *t) {
    return kTR_Pass;
}

// Enter at the end of a line that opens a block indents the new line one level past the reference line.
DLL_EXPORT int test_indent_newline_indent(ITesting *t) {
    auto tbl = CTable();

    // "if (x) {" at col 0 -> new line at level 1.
    auto a = NL(tbl, U"if (x) {", 8);
    TR_ASSERT(t, a.type == kIA::kSetIndent);
    TR_ASSERT(t, a.indentLevel == 1);
    TR_ASSERT(t, !a.insertBlankLine);

    // Already at level 1 ("    foo() {") -> new line at level 2.
    auto b = NL(tbl, U"    foo() {", 11);
    TR_ASSERT(t, b.indentLevel == 2);
    return kTR_Pass;
}

// Enter on a neutral line copies the reference line's indent.
DLL_EXPORT int test_indent_newline_copy(ITesting *t) {
    auto tbl = CTable();

    TR_ASSERT(t, NL(tbl, U"foo", 3).indentLevel == 0);
    TR_ASSERT(t, NL(tbl, U"    bar", 7).indentLevel == 1);     // keeps the 4-space indent
    TR_ASSERT(t, NL(tbl, U"        baz;", 12).indentLevel == 2);
    return kTR_Pass;
}

// Enter just before a closer (the closer moves down) dedents the new line one level.
DLL_EXPORT int test_indent_newline_dedent(ITesting *t) {
    auto tbl = CTable();

    // Cursor before the trailing '}' on an indented line -> new line dedents.
    auto a = NL(tbl, U"    }", 4);
    TR_ASSERT(t, a.indentLevel == 0);

    // Deeper nesting.
    auto b = NL(tbl, U"        }", 8);
    TR_ASSERT(t, b.indentLevel == 1);
    return kTR_Pass;
}

// Enter with the cursor between an opener and its closer expands into three lines.
DLL_EXPORT int test_indent_newline_expand(ITesting *t) {
    auto tbl = CTable();

    // "{|}" at col 0: closer line at level 0, an empty middle line at level 1.
    auto a = NL(tbl, U"{}", 1);
    TR_ASSERT(t, a.type == kIA::kSetIndent);
    TR_ASSERT(t, a.insertBlankLine);
    TR_ASSERT(t, a.blankLineLevel == 1);
    TR_ASSERT(t, a.indentLevel == 0);

    // Indented "    foo() {|}" -> middle at level 2, closer at level 1.
    auto b = NL(tbl, U"    foo() {}", 11);
    TR_ASSERT(t, b.insertBlankLine);
    TR_ASSERT(t, b.blankLineLevel == 2);
    TR_ASSERT(t, b.indentLevel == 1);
    return kTR_Pass;
}

// Typing a closer as the first non-blank char of a line snaps it back one level.
DLL_EXPORT int test_indent_electric(ITesting *t) {
    auto tbl = CTable();

    // Whitespace-only line at level 1, type '}' -> dedent to level 0.
    auto a = Ins(tbl, U"    ", 4, U'}');
    TR_ASSERT(t, a.type == kIA::kSetIndent);
    TR_ASSERT(t, a.indentLevel == 0);

    // Level 2 -> level 1.
    auto b = Ins(tbl, U"        ", 8, U'}');
    TR_ASSERT(t, b.indentLevel == 1);

    // Already at level 0 -> stays at 0 (no negative indent).
    TR_ASSERT(t, Ins(tbl, U"", 0, U'}').indentLevel == 0);
    return kTR_Pass;
}

// Electric dedent never fires mid-line, on a non-closer, or inside a string/comment.
DLL_EXPORT int test_indent_electric_suppress(ITesting *t) {
    auto tbl = CTable();

    // Not the first non-blank char -> no dedent.
    TR_ASSERT(t, Ins(tbl, U"    foo", 7, U'}').type == kIA::kNone);

    // Not a dedent trigger -> no dedent.
    TR_ASSERT(t, Ins(tbl, U"    ", 4, U';').type == kIA::kNone);

    // Inside a comment / string -> suppressed even on a blank prefix.
    TR_ASSERT(t, Ins(tbl, U"    ", 4, U'}', kTC::kLineComment).type == kIA::kNone);
    TR_ASSERT(t, Ins(tbl, U"    ", 4, U'}', kTC::kString).type == kIA::kNone);
    return kTR_Pass;
}

// An empty table (no language section / plaintext) copies the previous indent and never dedents on type.
DLL_EXPORT int test_indent_emptytable(ITesting *t) {
    IndentTable empty;
    TR_ASSERT(t, empty.IsEmpty());

    // Newline just copies the reference indent - no open/close logic.
    auto a = NL(empty, U"    foo() {", 11);
    TR_ASSERT(t, a.type == kIA::kSetIndent);
    TR_ASSERT(t, a.indentLevel == 1);
    TR_ASSERT(t, !a.insertBlankLine);

    // Never electric.
    TR_ASSERT(t, Ins(empty, U"    ", 4, U'}').type == kIA::kNone);
    return kTR_Pass;
}
