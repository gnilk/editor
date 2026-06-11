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

#include "Core/Language/IndentCache.h"
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
DLL_EXPORT int test_indent_reindent_nested(ITesting *t);
DLL_EXPORT int test_indent_reindent_anchor(ITesting *t);
DLL_EXPORT int test_indent_reindent_blank(ITesting *t);
DLL_EXPORT int test_indent_reindent_emptytable(ITesting *t);
DLL_EXPORT int test_indent_config(ITesting *t);
DLL_EXPORT int test_indent_config_unknown(ITesting *t);
DLL_EXPORT int test_indent_config_asset(ITesting *t);
}

using kIA = IndentEngine::kIndentAction;
using kTC = gedit::kLanguageTokenClass;

static const int kTab = 4;

// A C-like table: indent after the openers, dedent on the closers, electric off in strings/comments.
static IndentTable  CTable() {
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

// Reformat helper: build a RangeContext over the given lines (views stay valid for the call) and return the
// per-line indent levels. The u32strings must outlive the call, so they are passed by const-ref.
static std::vector<int> RR(const IndentTable &tbl, const std::vector<std::u32string> &lines, int anchor) {
    IndentEngine::RangeContext c;
    c.table = &tbl;
    c.tabSize = kTab;
    c.anchorLevel = anchor;
    for (const auto &l : lines) {
        c.lines.emplace_back(l);
    }
    return IndentEngine::ReindentRange(c);
}

DLL_EXPORT int test_indent(ITesting *t) {
    return kTR_Pass;
}

// Reindent a flat-anchored block: the walk re-derives levels from the trigger chars, ignoring the lines'
// own (here deliberately wrong) leading whitespace - the mis-indented middle line is fixed.
DLL_EXPORT int test_indent_reindent_nested(ITesting *t) {
    auto tbl = CTable();
    // anchor 0; middle line is garbage-indented but must come out at level 1, closer back at 0.
    auto r = RR(tbl, {U"if (x) {", U"            mess();", U"}"}, 0);
    TR_ASSERT(t, r.size() == 3);
    TR_ASSERT(t, r[0] == 0);
    TR_ASSERT(t, r[1] == 1);
    TR_ASSERT(t, r[2] == 0);
    return kTR_Pass;
}

// Anchor != 0: a block already nested one level in seeds the walk at the anchor; nesting is relative to it.
DLL_EXPORT int test_indent_reindent_anchor(ITesting *t) {
    auto tbl = CTable();
    auto r = RR(tbl, {U"foo() {", U"bar();", U"baz();", U"}"}, 1);
    TR_ASSERT(t, r.size() == 4);
    TR_ASSERT(t, r[0] == 1);    // opener line sits at the anchor level
    TR_ASSERT(t, r[1] == 2);    // body one level past
    TR_ASSERT(t, r[2] == 2);
    TR_ASSERT(t, r[3] == 1);    // closer back to the anchor level
    return kTR_Pass;
}

// A blank line holds the prevailing level and does not move the running depth.
DLL_EXPORT int test_indent_reindent_blank(ITesting *t) {
    auto tbl = CTable();
    auto r = RR(tbl, {U"{", U"", U"x"}, 0);
    TR_ASSERT(t, r.size() == 3);
    TR_ASSERT(t, r[0] == 0);
    TR_ASSERT(t, r[1] == 1);    // blank inside the block holds level 1
    TR_ASSERT(t, r[2] == 1);
    return kTR_Pass;
}

// Empty table => every line keeps its existing leading-indent level (plaintext is never reflowed).
DLL_EXPORT int test_indent_reindent_emptytable(ITesting *t) {
    IndentTable empty;
    // leading ws -> levels: "  x"=0 (2 spaces), "y"=0, "        z"=2 (8 spaces).
    auto r = RR(empty, {U"  x", U"y", U"        z"}, 5);
    TR_ASSERT(t, r.size() == 3);
    TR_ASSERT(t, r[0] == 0);
    TR_ASSERT(t, r[1] == 0);
    TR_ASSERT(t, r[2] == 2);    // unchanged, and the anchor is ignored for an empty table
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

// The YAML loader: a child folds in its inherited parent (generic) then overrides. Hermetic via
// LoadFromString - no asset loader.
DLL_EXPORT int test_indent_config(ITesting *t) {
    auto &cache = IndentCache::Instance();
    const std::string yaml =
        "indent:\n"
        "  generic:\n"
        "    suppress_in:  [string, comment]\n"
        "    indent_after: [\"{\", \"[\", \"(\"]\n"
        "    dedent_on:    [\"}\", \"]\", \")\"]\n"
        "  cpp:\n"
        "    inherit: generic\n"
        "  python:\n"
        "    inherit: generic\n"
        "    indent_after: [\":\"]\n";          // overrides the inherited set
    TR_ASSERT(t, cache.LoadFromString(yaml));

    auto cpp = cache.GetTableForLanguage("cpp");
    TR_ASSERT(t, cpp != nullptr);
    TR_ASSERT(t, cpp->IsIndentAfter(U'{'));
    TR_ASSERT(t, cpp->IsIndentAfter(U'('));
    TR_ASSERT(t, cpp->IsDedentOn(U'}'));
    TR_ASSERT(t, cpp->IsSuppressed(kTC::kString));
    TR_ASSERT(t, cpp->IsSuppressed(kTC::kLineComment));

    // python REPLACES indent_after (only ':'), keeps inherited dedent_on + suppress_in.
    auto py = cache.GetTableForLanguage("python");
    TR_ASSERT(t, py->IsIndentAfter(U':'));
    TR_ASSERT(t, !py->IsIndentAfter(U'{'));
    TR_ASSERT(t, py->IsDedentOn(U'}'));

    cache.Clear();
    return kTR_Pass;
}

// A language with no section (e.g. plaintext "default") resolves to an empty table => copy-previous-indent.
DLL_EXPORT int test_indent_config_unknown(ITesting *t) {
    auto &cache = IndentCache::Instance();
    TR_ASSERT(t, cache.LoadFromString(
        "indent:\n  generic:\n    indent_after: [\"{\"]\n    dedent_on: [\"}\"]\n"));
    auto def = cache.GetTableForLanguage("default");
    TR_ASSERT(t, (def != nullptr) && def->IsEmpty());
    cache.Clear();
    return kTR_Pass;
}

// Loads the actual shipped indent.yml via the asset loader (the EnsureLoaded path). Validates the file
// parses and the cpp section resolves (generic openers/closers), while plaintext 'default' stays empty.
DLL_EXPORT int test_indent_config_asset(ITesting *t) {
    auto &cache = IndentCache::Instance();
    cache.Clear();   // force EnsureLoaded to read resources/indent.yml

    auto cpp = cache.GetTableForLanguage("cpp");
    TR_ASSERT(t, cpp != nullptr);
    TR_ASSERT(t, cpp->IsIndentAfter(U'{'));
    TR_ASSERT(t, cpp->IsIndentAfter(U'['));
    TR_ASSERT(t, cpp->IsIndentAfter(U'('));
    TR_ASSERT(t, cpp->IsDedentOn(U'}'));
    TR_ASSERT(t, cpp->IsDedentOn(U')'));
    TR_ASSERT(t, cpp->IsSuppressed(kTC::kString));
    TR_ASSERT(t, cpp->IsSuppressed(kTC::kBlockComment));

    // No 'default' section -> plaintext gets no language indent rules.
    TR_ASSERT(t, cache.GetTableForLanguage("default")->IsEmpty());

    cache.Clear();
    return kTR_Pass;
}
