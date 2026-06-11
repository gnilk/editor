//
// Created by gnilk on 11.06.26.
//
// Step 0/1 of the auto-pairing engine (docs/autopair-plan.md): the pure decision logic. AutoPairEngine
// takes a primitive Context (table + line text + cursor + token-class + selection) and returns one Action,
// so it is tested directly with a hand-built AutoPairTable - no live editor, no tokenizer, no YAML yet.
// These cases ARE the v1 rule contract: guarded auto-close, type-through, selection-wrap, backspace-pair,
// quote open/skip, suppress-in-string/comment, and empty-table => always None (plaintext).
//
#include <testinterface.h>

#include <string>

#include "Core/Language/AutoPairCache.h"
#include "Core/Language/AutoPairEngine.h"
#include "Core/Language/AutoPairTable.h"
#include "Core/Language/LanguageTokenClass.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_autopair(ITesting *t);
DLL_EXPORT int test_autopair_autoclose(ITesting *t);
DLL_EXPORT int test_autopair_skipover(ITesting *t);
DLL_EXPORT int test_autopair_suppress(ITesting *t);
DLL_EXPORT int test_autopair_quotes(ITesting *t);
DLL_EXPORT int test_autopair_wrap(ITesting *t);
DLL_EXPORT int test_autopair_backspace(ITesting *t);
DLL_EXPORT int test_autopair_emptytable(ITesting *t);
DLL_EXPORT int test_autopair_config(ITesting *t);
DLL_EXPORT int test_autopair_config_unknown(ITesting *t);
DLL_EXPORT int test_autopair_config_asset(ITesting *t);
}

using kPA = AutoPairEngine::kPairAction;
using kTC = gedit::kLanguageTokenClass;

// A C-like table: the standard brackets + quote pairs, generic guard rules.
static AutoPairTable CTable() {
    AutoPairTable tbl;
    tbl.pairs = {
        {U'(', U')', false},
        {U'[', U']', false},
        {U'{', U'}', false},
        {U'"', U'"', true},
        {U'\'', U'\'', true},
    };
    tbl.autoCloseBefore = {U')', U']', U'}', U',', U';', U' ', U'\t'};
    tbl.autoCloseBeforeEol = true;
    tbl.suppressIn = {kTC::kString, kTC::kChar, kTC::kLineComment, kTC::kBlockComment, kTC::kCommentedText};
    return tbl;
}

// Run a typed-char decision against a line; line outlives the synchronous call (Action is returned by value).
static AutoPairEngine::Action Ins(const AutoPairTable &tbl, const std::u32string &line, int x, char32_t typed,
                                  kTC cls = kTC::kRegular, bool sel = false) {
    AutoPairEngine::Context c;
    c.table = &tbl;
    c.lineText = line;
    c.cursorX = x;
    c.tokenClassAtCursor = cls;
    c.selectionActive = sel;
    return AutoPairEngine::OnInsertChar(c, typed);
}

static AutoPairEngine::Action Bsp(const AutoPairTable &tbl, const std::u32string &line, int x) {
    AutoPairEngine::Context c;
    c.table = &tbl;
    c.lineText = line;
    c.cursorX = x;
    return AutoPairEngine::OnBackspace(c);
}

DLL_EXPORT int test_autopair(ITesting *t) {
    return kTR_Pass;
}

// Auto-close fires only when the next char invites it (EOL / whitespace / closer / separator), never mid-word.
DLL_EXPORT int test_autopair_autoclose(ITesting *t) {
    auto tbl = CTable();

    // At end of line -> pair.
    auto a = Ins(tbl, U"foo", 3, U'(');
    TR_ASSERT(t, a.type == kPA::kInsertPair);
    TR_ASSERT(t, a.open == U'(' && a.close == U')');

    // Before a word char -> NO auto-close (would make "(|)foo").
    TR_ASSERT(t, Ins(tbl, U"foo", 0, U'(').type == kPA::kNone);

    // Before a closer / whitespace / separator -> pair.
    TR_ASSERT(t, Ins(tbl, U")", 0, U'(').type == kPA::kInsertPair);
    TR_ASSERT(t, Ins(tbl, U" x", 0, U'(').type == kPA::kInsertPair);
    TR_ASSERT(t, Ins(tbl, U",y", 0, U'[').type == kPA::kInsertPair);

    // Bracket char passes through its own pair lookup (square/curly too).
    TR_ASSERT(t, Ins(tbl, U"", 0, U'{').type == kPA::kInsertPair);
    return kTR_Pass;
}

// Typing a closer that already sits to the right just steps over it.
DLL_EXPORT int test_autopair_skipover(ITesting *t) {
    auto tbl = CTable();

    // "()" cursor between, type ')' -> skip.
    auto a = Ins(tbl, U"()", 1, U')');
    TR_ASSERT(t, a.type == kPA::kSkipOver);

    // ')' with nothing matching to the right -> default insert (None).
    TR_ASSERT(t, Ins(tbl, U"(", 1, U')').type == kPA::kNone);
    return kTR_Pass;
}

// No auto-close when the cursor is inside a string or comment.
DLL_EXPORT int test_autopair_suppress(ITesting *t) {
    auto tbl = CTable();
    TR_ASSERT(t, Ins(tbl, U"// x", 4, U'(', kTC::kLineComment).type == kPA::kNone);
    TR_ASSERT(t, Ins(tbl, U"ab", 2, U'(', kTC::kString).type == kPA::kNone);
    TR_ASSERT(t, Ins(tbl, U"ab", 2, U'[', kTC::kBlockComment).type == kPA::kNone);
    return kTR_Pass;
}

// Quotes: same key opens and closes -> open a pair on fresh ground, skip an existing close, never pair
// right after an identifier (don't) or inside an existing string.
DLL_EXPORT int test_autopair_quotes(ITesting *t) {
    auto tbl = CTable();

    // Fresh ground -> open a pair.
    auto a = Ins(tbl, U"x ", 2, U'"');
    TR_ASSERT(t, a.type == kPA::kInsertPair);
    TR_ASSERT(t, a.open == U'"' && a.close == U'"');

    // Right after an identifier -> plain insert (apostrophe in don't).
    TR_ASSERT(t, Ins(tbl, U"don", 3, U'\'').type == kPA::kNone);

    // Closing an open quote -> skip-over.
    TR_ASSERT(t, Ins(tbl, U"\"\"", 1, U'"').type == kPA::kSkipOver);

    // Inside a string, not at the closing quote -> plain insert.
    TR_ASSERT(t, Ins(tbl, U"\"ab\"", 2, U'"', kTC::kString).type == kPA::kNone);
    return kTR_Pass;
}

// With a selection active, typing an opener or a quote wraps the selection.
DLL_EXPORT int test_autopair_wrap(ITesting *t) {
    auto tbl = CTable();

    auto a = Ins(tbl, U"foo", 0, U'(', kTC::kRegular, true);
    TR_ASSERT(t, a.type == kPA::kWrapSelection);
    TR_ASSERT(t, a.open == U'(' && a.close == U')');

    auto q = Ins(tbl, U"foo", 0, U'"', kTC::kRegular, true);
    TR_ASSERT(t, q.type == kPA::kWrapSelection);
    return kTR_Pass;
}

// Backspace on an adjacent empty pair removes both halves; otherwise it is a normal backspace.
DLL_EXPORT int test_autopair_backspace(ITesting *t) {
    auto tbl = CTable();

    TR_ASSERT(t, Bsp(tbl, U"()", 1).type == kPA::kDeletePair);
    TR_ASSERT(t, Bsp(tbl, U"\"\"", 1).type == kPA::kDeletePair);

    // Not an empty pair -> nothing special.
    TR_ASSERT(t, Bsp(tbl, U"(x", 1).type == kPA::kNone);
    TR_ASSERT(t, Bsp(tbl, U"()", 0).type == kPA::kNone);   // cursor before the pair
    return kTR_Pass;
}

// An empty table (no language section / plaintext) is always a no-op.
DLL_EXPORT int test_autopair_emptytable(ITesting *t) {
    AutoPairTable empty;
    TR_ASSERT(t, empty.IsEmpty());
    TR_ASSERT(t, Ins(empty, U"foo", 3, U'(').type == kPA::kNone);
    TR_ASSERT(t, Ins(empty, U"()", 1, U')').type == kPA::kNone);
    TR_ASSERT(t, Bsp(empty, U"()", 1).type == kPA::kNone);
    return kTR_Pass;
}

// The YAML loader: a child folds in its inherited parent (generic) then appends/overrides. Hermetic via
// LoadFromString - no asset loader.
DLL_EXPORT int test_autopair_config(ITesting *t) {
    auto &cache = AutoPairCache::Instance();
    const std::string yaml =
        "autopairs:\n"
        "  generic:\n"
        "    suppress_in: [string, comment]\n"
        "    auto_close_before: [\")\", whitespace, eol]\n"
        "    pairs:\n"
        "      - { open: \"(\", close: \")\" }\n"
        "      - { open: \"\\\"\", close: \"\\\"\", quote: yes }\n"
        "  cpp:\n"
        "    inherit: generic\n"
        "    pairs:\n"
        "      - { open: \"<\", close: \">\" }\n";
    TR_ASSERT(t, cache.LoadFromString(yaml));

    auto cpp = cache.GetTableForLanguage("cpp");
    TR_ASSERT(t, cpp != nullptr);
    // Inherited the generic pairs + its own <>.
    TR_ASSERT(t, cpp->FindByOpen(U'(') != nullptr);
    TR_ASSERT(t, cpp->FindByOpen(U'<') != nullptr);
    TR_ASSERT(t, cpp->FindByClose(U'>') != nullptr);
    auto q = cpp->FindByOpen(U'"');
    TR_ASSERT(t, (q != nullptr) && q->isQuote);
    // Inherited suppress_in + auto_close_before.
    TR_ASSERT(t, cpp->IsSuppressed(kTC::kString));
    TR_ASSERT(t, cpp->IsSuppressed(kTC::kLineComment));
    TR_ASSERT(t, cpp->AutoCloseBefore(0));        // eol
    TR_ASSERT(t, cpp->AutoCloseBefore(U' '));     // whitespace
    TR_ASSERT(t, cpp->AutoCloseBefore(U')'));
    TR_ASSERT(t, !cpp->AutoCloseBefore(U'x'));

    // The generic base itself does not have the cpp-only <>.
    auto gen = cache.GetTableForLanguage("generic");
    TR_ASSERT(t, gen->FindByOpen(U'(') != nullptr);
    TR_ASSERT(t, gen->FindByOpen(U'<') == nullptr);

    cache.Clear();
    return kTR_Pass;
}

// A language with no section (e.g. plaintext "default") resolves to an empty table => no pairing.
DLL_EXPORT int test_autopair_config_unknown(ITesting *t) {
    auto &cache = AutoPairCache::Instance();
    TR_ASSERT(t, cache.LoadFromString(
        "autopairs:\n  generic:\n    pairs:\n      - { open: \"(\", close: \")\" }\n"));
    auto def = cache.GetTableForLanguage("default");
    TR_ASSERT(t, (def != nullptr) && def->IsEmpty());
    cache.Clear();
    return kTR_Pass;
}

// Loads the actual shipped autopairs.yml via the asset loader (the EnsureLoaded path). Validates the file
// parses and the cpp section resolves (generic brackets + cpp <>), while plaintext 'default' stays empty.
DLL_EXPORT int test_autopair_config_asset(ITesting *t) {
    auto &cache = AutoPairCache::Instance();
    cache.Clear();   // force EnsureLoaded to read resources/autopairs.yml

    auto cpp = cache.GetTableForLanguage("cpp");
    TR_ASSERT(t, cpp != nullptr);
    TR_ASSERT(t, cpp->FindByOpen(U'(') != nullptr);
    TR_ASSERT(t, cpp->FindByOpen(U'[') != nullptr);
    TR_ASSERT(t, cpp->FindByOpen(U'{') != nullptr);
    TR_ASSERT(t, cpp->FindByOpen(U'<') == nullptr);     // '<' is intentionally NOT auto-paired (ambiguous)
    auto dq = cpp->FindByOpen(U'"');
    TR_ASSERT(t, (dq != nullptr) && dq->isQuote);
    TR_ASSERT(t, cpp->IsSuppressed(kTC::kString));
    TR_ASSERT(t, cpp->IsSuppressed(kTC::kBlockComment));

    // No 'default' section -> plaintext gets no pairing.
    TR_ASSERT(t, cache.GetTableForLanguage("default")->IsEmpty());

    cache.Clear();
    return kTR_Pass;
}
