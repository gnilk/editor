# Indent engine — plan

A data-driven auto-indentation engine, configured from a standalone YAML file, sibling to the
`AutoPairEngine`. It owns two behaviours: **newline auto-indent** (the indent of the line created on
Enter) and **dedent-on-type** (typing a closer like `}` snaps the current line back one level — the
behaviour that was *dropped* when the auto-pair experiment was deleted, with re-homing deferred to here).

## Why

Indentation today is smeared across two imperative, per-subclass places, and the electric dedent is
missing entirely:

- **Newline indent** lives in `Document::NewLine` (`Document.cpp:623`): after splitting a line it
  reparses a ±2 region and calls `newLine->Indent(tabSize)`, where `Line::indent` was stamped by the
  tokenizer's brace-depth counter (`LangLineTokenizer::ParseLine`, `LangLineTokenizer.cpp:135` —
  `nextIndent++/--` on `kCodeBlockStart/End` and `kArrayStart/End`). The `{|}` three-line expansion
  ("press Enter between `{` and `}`") is a *second* mechanism: `CPPLanguage::OnPreCreateNewLine`
  (`CPPLanguage.cpp:147`) returns `kNewLine` when the split line's last char is `}`, and `NewLine`
  injects an extra empty line for it.
- **Dedent-on-type is gone.** `CPPLanguage::OnPreInsertChar` used to dedent the cursor a tab when you
  typed `}`; deleting the auto-pair experiment removed it (autopair-plan.md "Decided — the `}`
  auto-dedent drops in v1 (option a)"). It was explicitly punted to *this* engine.

The *seam* is right — "the language rule-set drives indent" — but it is imperative C++ per subclass, the
two newline mechanisms are uncoordinated, and the electric dedent has no home. Same shape problem the
auto-pair engine just solved, so the same cure: **pure decision logic + declarative data, selected by
language config name.**

## Target — three clean separations (mirrors AutoPair)

| Piece | What it is | Notes |
|---|---|---|
| **`IndentTable`** | the *data*: `indent_after` / `dedent_on` char sets, `suppress_in` token-classes | built from YAML; one per language config name, `inherit`-resolved |
| **`IndentEngine`** | the *logic*: `OnNewLine(ctx) → Action`, `OnInsertChar(ctx, typed) → Action`, pure, no UI/tokenizer/Line dependency | the guard logic lives here, written once, unit-tested directly |
| **`IndentCache`** | build-once, `inherit`-resolve, cache by config name; `LoadFromString` for hermetic tests | a line-for-line clone of `AutoPairCache` |

**Architectural stance (identical to AutoPair):** not new `virtual` hooks on the language subclass, not
part of `LangLineTokenizer`. A standalone pair (logic + data) selected by `LanguageBase::GetConfigNodeName()`
(already added for AutoPair). No section ⇒ empty table ⇒ engine is a no-op (plaintext indents like a dumb
editor: copy the previous line's leading whitespace, the default fallback — see below).

### Indent model — RELATIVE, not absolute brace-depth (the key decision)

The existing newline indent is **absolute**: `Line::indent` is a depth counter the tokenizer maintains
across lines, and the new line gets `tabSize × depth` spaces. The engine instead computes indent
**relative to the reference line's actual leading whitespace** (VS Code's `indentationRules` model):

- New-line indent = `leadingWhitespace(lineBeingLeft)` **+1 level** if that line (up to the cursor) ends
  with an `indent_after` char, **−1 level** if the content moving down starts with a `dedent_on` char.

This is *local and pure* — it needs only the current line text + cursor, no cross-line tokenizer state,
no full reparse — and it is robust on already-mis-indented code (it tracks the reference line you are
actually on rather than a recomputed absolute depth). Trade-off: it cannot see nesting the current line's
whitespace doesn't already reflect; in practice the reference line's whitespace *is* the depth, so this
matches. (This is why the locked answer chose data-driven YAML over "pure engine over tokenizer depth":
the trigger chars are the signal, the reference line's whitespace is the base.) The tokenizer's
brace-depth `Line::indent` stops driving newline indent; it stays only for any drawing/indent-guide use
(`EditorView.cpp:248`) — full retirement is deferred, not required for v1.

### Per-line metadata — none needed (same as AutoPair)

The only extra context is the **token class at the cursor** (to suppress electric dedent inside a
string/comment), already on `Line::LineAttrib` via `Line::AttributeAt(cursorX)->tokenClass`. No new
`Line` fields, no tokenizer prerequisite.

## The YAML config

A standalone `Assets/Resources/indent.yml`, referenced from `config.yml` `main.indent:` (mirroring
`main.autopairs:`), self-contained and independently unit-testable. `generic` base; languages `inherit`
and override; intra-file inheritance resolved by section name; keyed by the language **config name**
(`cpp`, `json`, …), not `Identifier()`.

```yaml
indent:
  generic:
    suppress_in:  [string, comment]     # token-class at cursor where electric dedent is OFF
    indent_after: ["{", "[", "("]       # line ending with one of these => next line indents one level
    dedent_on:    ["}", "]", ")"]       # line starting with one of these => that line dedents one level
  cpp:
    inherit: generic
  json:
    inherit: generic
```

**Selection rule (same as AutoPair):** look up the section by config name. **No section ⇒ empty table.**
An empty table makes the engine fall back to "new line copies the previous line's leading whitespace"
(dumb but correct for plaintext) and never dedents on type.

**Loading/inheritance owner:** `IndentCache`, a clone of `AutoPairCache` — build each table once, resolve
the intra-file `inherit` chain through itself (child overrides/unions parent), cache by config name,
cycle-guarded, `LoadFromString` for hermetic tests, `EnsureLoaded` for the shipped asset.

## The engine — decision model

`IndentEngine::OnNewLine(ctx) → Action` and `IndentEngine::OnInsertChar(ctx, typed) → Action`.

**Context** (one struct, superset — mirrors `AutoPairEngine::Context`):
`{ table, lineText, cursorX, tabSize, tokenClassAtCursor }`. For newline, `lineText[0..cursorX)` is the
part staying (the reference line) and `lineText[cursorX..)` is the content moving down — one field
captures both halves, exactly like AutoPair derives `CharLeft`/`CharRight`.

**Action** (the caller applies exactly one):
- `kNone` — leave indentation to the default path.
- `kSetIndent{ indentLevel }` — set the target line's leading whitespace to `indentLevel × tabSize`
  spaces. (Newline: the level of the *new* line. Electric: the new level of the *current* line.)
- newline-only fields `insertBlankLine` + `blankLineLevel` — the `{|}` expansion: an extra empty line at
  `blankLineLevel` sits between, and the `}` line lands at `indentLevel`.

**Rules (the v1 contract — these are the unit tests):**

- **`OnNewLine`:**
  - `baseLevel = leadingWhitespaceColumns(lineBeforeCursor) / tabSize`.
  - `opens` = trimmed-right `lineBeforeCursor` ends with an `indent_after` char.
  - `closesNext` = trimmed-left `lineAfterCursor` starts with a `dedent_on` char.
  - new level = `baseLevel + (opens ? 1 : 0) − (closesNext ? 1 : 0)`, clamped ≥ 0.
  - **`{|}` expansion:** `opens && closesNext` (cursor between an opener and its closer) ⇒
    `insertBlankLine`, `blankLineLevel = baseLevel + 1`, and the closer line's `indentLevel = baseLevel`.
  - Empty table ⇒ `kSetIndent{ baseLevel }` (copy previous indent; no open/close logic).
- **`OnInsertChar` (electric dedent):** typed char ∈ `dedent_on`, **and** `lineText[0..cursorX)` is all
  whitespace (the closer is the first non-blank on the line), **and** `tokenClassAtCursor ∉ suppress_in`
  ⇒ `kSetIndent{ max(0, currentLevel − 1) }` for the current line. Else `kNone`.

Electric dedent is deliberately the only on-type trigger in v1 (it re-homes the dropped `}`-dedent). No
`case:`/`else`/label re-indent, no comment-continuation, no reformat-on-paste (Deferred).

## Wiring — two decision points (Step 3)

- **Newline — `Document::NewLine` (`Document.cpp:623`).** Replace the `OnPreCreateNewLine` +
  `newLine->Indent(tabSize)` block with one `IndentEngine::OnNewLine` consult. Build the context from the
  line being split + cursor (token class via `Line::AttributeAt`, guarded). Apply: set the new line's
  leading whitespace to `indentLevel × tabSize`; if `insertBlankLine`, inject the empty middle line at
  `blankLineLevel` (subsumes `OnPreCreateNewLine`). Keep the undo grouping + `CaptureWantedColumn`. The
  ±2 reparse stays only if drawing still needs `Line::indent`; the indent *value* now comes from the
  engine, not the reparse.
- **Electric dedent — `EditController::HandleKeyPress` (`EditController.cpp:46`).** After the auto-pair
  consult, on a printable key consult `IndentEngine::OnInsertChar`; on `kSetIndent` rewrite the current
  line's leading whitespace (re-using `Line::Indent`/`Unindent` mechanics or a direct splice) so the
  typed `}` snaps to its opener's level. Mind the ordering with auto-pair skip-over (typing `}` over an
  auto-inserted `}` is a skip, not a fresh close — electric should act on the manual-close case). Build
  the context through a single glue helper, mirroring `BuildAutoPairContext`.
- **Delete the experiment:** `CPPLanguage::OnPreCreateNewLine` + its `CPPLanguage.h` decl + the
  `LanguageBase::OnPreCreateNewLine` virtual (`LanguageBase.h:43`). The `{|}` expansion is now the
  engine's `insertBlankLine`.

**Verification:** unit-test `OnNewLine`/`OnInsertChar` directly (Step 0), then integration-test through
`Document::NewLine` / `EditController` in `test_document` (Enter after `{` indents; Enter between `{|}`
makes the three-line block; typing `}` dedents; no electric inside a comment; plaintext just copies the
previous indent). Then the SDL on-screen feel-check.

## Ordered steps — each compiles, keeps the verified-green set green

- **Step 0 — Pin the contract (test-first).** New `utests/test_indent.cpp` (`test_indent` module + CMake
  entry). Discriminating cases against the not-yet-built engine + a hand-built `IndentTable`: newline
  indent after an opener, newline copy when neutral, dedent when the moved content starts with a closer,
  the `{|}` blank-line expansion, electric dedent on a whitespace-only line, electric suppressed
  mid-line / inside a comment, and **empty table ⇒ copy-previous-indent / never electric**. Red first.
- **Step 1 — `IndentTable` + `IndentEngine`.** Structs + the two `On*` methods. Pure, in-memory table
  (no YAML). Turn Step 0 green. (Commit: `IND.0/1`.)
- **Step 2 — YAML loader + inheritance.** `indent.yml` (`generic`+`cpp`+`json`) referenced from
  `config.yml`; `IndentCache` cloned from `AutoPairCache` (build-once / inherit-resolve / `LoadFromString`
  / asset path). Loader tests. Green. (Commit: `IND.2`.)
- **Step 3 — Wire + delete the experiment.** `Document::NewLine` consult + electric dedent in
  `EditController`; delete `OnPreCreateNewLine` (+ the `LanguageBase` virtual). Integration tests in
  `test_document`; live SDL verify (Enter after `{`, the `{|}` block, `}`-dedent, no electric in a
  comment, plaintext copy-indent). Green. (Commit: `IND.3a` newline, `IND.3b` electric — sub-split like
  AutoPair's 3a/3b.)

Per repo discipline: contract tests precede the code; the engine is fully covered before it is wired into
the live edit path.

**STATUS (2026-06-11): Steps 0/1/2 + 3a + 3b DONE. Indent v1 feature-complete; GUI feel-check pending.**
- `IndentEngine` + `IndentTable` (pure, `test_indent` 9 cases) — `IND.0/1`.
- `indent.yml` + `IndentCache` (clone of `AutoPairCache`, `test_indent` 12 cases) — `IND.2`.
- **3a — newline indent wired.** `Document::NewLine` now consults `IndentEngine::OnNewLine` (file-local
  `ComputeNewLineIndent` + `ApplyLeadingIndent`) for the new line's indent and the `{|}` expansion,
  replacing `newLine->Indent(tabSize)` + `OnPreCreateNewLine`. The experiment is deleted:
  `CPPLanguage::OnPreCreateNewLine` (+ decl), the `LanguageBase::OnPreCreateNewLine` virtual, and the
  now-unused `LanguageBase::kInsertAction` enum. Also guarded the null reparse-job (threaded-off path
  parses synchronously and returns null — the old `->WaitComplete()` would have crashed; no test hit
  `NewLine` before). 3 integration tests in `test_document` (newline / dedent / `{|}` expand) assert the
  buffer + cursor through `Document::NewLine`. Verified-green set **187**. **GUI feel-check still
  pending** (Enter after `{`, the `{|}` block, plaintext copy-indent).
- **3b — electric dedent-on-type wired.** `EditController::HandleKeyPress` consults
  `IndentEngine::OnInsertChar` (via `BuildIndentContext`) AFTER the auto-pair block (so a skip-over /
  insert-pair has already returned) and BEFORE the default insert: on `kSetIndent` it replaces the line's
  leading-whitespace run with the new level and reparks the cursor, then the typed closer is inserted
  (`ApplyElectricDedent`). 2 integration tests in `test_document` (electric dedent on a blank-prefix line;
  no dedent mid-line). Verified-green set **189**.

**Indent v1 is feature-complete** (newline auto-indent, `{|}` expansion, electric dedent-on-type, the
experiment deleted). Remaining: the GUI feel-check (Enter after `{`, the `{|}` block, `}`-dedent, no
electric inside a comment / in `readme.txt`).

## Decisions — locked (confirmed before planning, 2026-06-11)

1. **v1 scope = newline auto-indent + dedent-on-type**, both as engine decisions (the full
   consolidation the auto-pair plan deferred here).
2. **Data-driven YAML, mirroring AutoPair** — `IndentTable` + `IndentCache` + `indent.yml`, selected by
   config name; *not* a pure engine driven off the tokenizer's brace-depth.
3. **Relative indent model** (reference line's leading whitespace + trigger deltas), not absolute
   tokenizer depth — pure, local, robust on mis-indented code.
4. **Test-first**: `IndentEngine` + `IndentTable` covered by a `test_indent` module before wiring.

## Out of scope / deferred

- **Retiring the tokenizer brace-depth indent** (`Line::indent`, `LangLineTokenizer` `nextIndent`) — kept
  for any indent-guide/drawing use; v1 only stops *reading* it for newline indent.
- **Richer electric triggers** — `case:`/`default:`/access-specifier re-indent, `else`/`elif` snap,
  comment-continuation (`*` on the next line of a block comment), Python `:`-driven indent.
- **Reformat-on-paste / whole-region re-indent**, and per-language *aligned* (vs stepped) continuation
  indent for wrapped expressions.
- **Language-architecture refactor** (dissolving class-per-language into data) — this engine is another
  step in that direction but does not perform it.
