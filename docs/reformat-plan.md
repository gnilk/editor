# Reformat-selection (reindent-a-range) — plan

A "reformat selection" command: re-derive the **correct indentation for a range of lines** and apply it.
Tied to a key-binding (the explicit command) and, optionally, to the auto-pair selection-wrap path (so
surrounding a block with `{ }` re-indents its body — the deferred **H.18b** from the autopair feel-check).

This is the **third** member of the indent family, built on the engine that already ships on `main`
(`IndentEngine` + `IndentTable` + `IndentCache`, `indent.yml`). It adds **one new capability** to the
engine — reindent a *range* — and two thin invocation surfaces.

## Why

The shipped `IndentEngine` is **pure, per-line, and RELATIVE**: `OnNewLine` and `OnInsertChar` each look
at a single line's own leading whitespace plus the trigger chars on that line. That is exactly right for
reactive editing — but it is the wrong model for *reformatting*, because the whole premise of reformat is
that the lines' **current indentation is wrong** and must be recomputed. You cannot seed each line's level
from its own (broken) whitespace.

Two concrete gaps motivate the feature:

- **No "format this region" command at all.** `kActionIndent`/`kActionUnindent` only *shift* a selection
  by one level (manual Tab / Shift-Tab); nothing re-derives the *correct* levels.
- **H.18b (from `docs/check-autopair.md`):** selection-wrap with `{` surrounds a block faithfully but does
  **not** re-indent the body — `{foo();\n    bar();}` instead of a properly nested block. That fix *is*
  this capability applied to the just-wrapped range.

## Target — one new engine method + two invocation surfaces

| Piece | What it is | Notes |
|---|---|---|
| **`IndentEngine::ReindentRange`** | the *new logic*: a **stateful walk** over N lines that returns one absolute indent level per line, seeded by an anchor level | pure, no UI/Line/tokenizer dependency, unit-tested directly with a hand-built `IndentTable` — same discipline as the existing two methods |
| **`kActionReformatSelection`** | the explicit command (key-binding) → reformat the selected range, or the current line when there is no selection | new `kAction` + keymap entries + an `EditController` handler |
| **wrap integration (H.18b)** | after a block-introducing wrap (`{ }`), run `ReindentRange` over the wrapped body | optional follow-on; gated to block pairs only — see Decisions |

**Architectural stance (unchanged):** the new method stays in `IndentEngine` (pure logic), the caller
(`Document`) applies the returned levels via the existing `ApplyLeadingIndent` helper. No new `Line`
fields. No tokenizer dependency. Selected by the same `LanguageBase::GetConfigNodeName()` table —
**no section ⇒ empty table ⇒ reformat is a no-op** (plaintext is left untouched, which is correct).

## The engine — decision model

`IndentEngine::ReindentRange(const RangeContext &ctx) → std::vector<int>` (one indent level, in **tab
units**, per input line).

**RangeContext** (a new, range-shaped sibling of `Context` — it carries N lines, not one):
```
struct RangeContext {
    const IndentTable *table;
    std::vector<std::u32string_view> lines;   // the lines to reformat, in order
    int anchorLevel;                          // indent level of the line ABOVE the range (0 at file top)
    int tabSize;
    // v1: per-line token class is NOT consulted (see Deferred — multiline string/comment safety)
};
```

**The walk (the v1 contract — these are the unit tests):**
Seed `depth = anchorLevel`. For each line `L` in order:
1. **Leading dedent:** if `firstNonSpace(L)` ∈ `dedent_on` → the line itself sits one level out:
   `lineLevel = max(0, depth - 1)`. Else `lineLevel = depth`.
2. **Emit** `lineLevel` for `L`.
3. **Trailing indent for the NEXT line:** recompute `depth` from `lineLevel` adjusted by the line's net
   opener/closer signal: `depth = lineLevel + (lastNonSpace(L) ∈ indent_after ? 1 : 0)`.

This is the classic stepped re-indent (a running brace counter), but driven by the **same
`indent_after`/`dedent_on` char sets** the engine already loads, and **seeded by an anchor** rather than
an absolute file-wide depth — so it composes with surrounding code and is robust mid-file.

- **Empty table ⇒ return each line's *existing* level unchanged** (plaintext: reformat is a no-op, never
  collapses indentation it doesn't understand).
- Blank / whitespace-only lines emit `depth` (the prevailing level) and do not move `depth`.
- All levels clamped ≥ 0.

**Why a vector, not an Action stream:** the caller needs all levels to apply leading whitespace line by
line; a flat `vector<int>` keyed by input index is the simplest pure return. The single-line methods keep
their `Action` shape; this method is range-shaped because the *decision is inherently cross-line*.

### Anchor seeding (the one subtle input)

`anchorLevel` = the indent level of the line **immediately above** the range (`leadingWhitespace /
tabSize`), or `0` if the range starts at line 0. Using the *existing* whitespace of the (untouched) anchor
line is deliberate: reformat stays relative to the surrounding context above and never fights correctly-
indented code outside the selection.

## Invocation surfaces — wiring

### 1. Explicit command — `kActionReformatSelection`

- **Action enum:** add `kActionReformatSelection` to `kAction` (`src/Core/Action.h`), near
  `kActionIndent`/`kActionUnindent`.
- **Keymap:** bind it in the editor keymaps (`Assets/Resources/{Linux,macOS}/default_keymap.yml`). Default
  key TBD — see Decisions (CLion's reformat is Cmd+Alt+L; a free Alt-chord fits the project's
  `UINavigation == Alt` convention on macOS/SDL3).
- **Handler (`EditController`):** new `OnActionReformatSelection` (registered via the action-handler
  mixin). It resolves the line range:
  - selection active → `[Selection::GetStart().y .. GetEnd().y]` (linewise: a partial last line still
    reformats whole-line; reformat is a line operation).
  - no selection → just the current line.
  Then: read the range's line texts, compute `anchorLevel` from the line above, call
  `IndentEngine::ReindentRange`, and apply each returned level with `ApplyLeadingIndent` under **one** undo
  item bracketing the whole range. Reparse the touched region; keep/clamp the cursor sensibly (park at the
  first reformatted line's first non-blank, or preserve column where it still maps).
- **Apply path lives in `Document`** (mirrors `ComputeNewLineIndent`/`ApplyLeadingIndent`): a
  `Document::ReindentLineRange(startY, endY)` that builds the `RangeContext`, calls the engine, and applies
  — so both the action handler and the wrap path share one entry point.

### 2. Wrap integration — H.18b (optional follow-on)

`EditController::TryWrapSelection` currently inserts opener/closer faithfully and returns. For a
**block-introducing** wrap (the wrapped pair's `open` ∈ `indent_after`, i.e. `{`), call
`Document::ReindentLineRange` over the now-wrapped body **after** the closer/opener are inserted, before
parking the cursor. Non-block wraps (`(`, `[`, quotes) stay faithful — no reindent. This is gated, not
automatic on every wrap (see Decisions).

## Ordered steps — each compiles, keeps the verified-green set green

- **Step 0 — Pin the contract (test-first).** Extend `utests/test_indent.cpp` with `ReindentRange` cases
  against a hand-built `IndentTable`: nested block from a flat anchor; a leading-closer line dedents; a
  blank line holds the level; mixed already-correct lines stay put; **empty table ⇒ unchanged**; anchor ≠ 0
  (reformat inside an existing block). Red first.
- **Step 1 — `IndentEngine::ReindentRange` + `RangeContext`.** The stateful walk. Pure, in-memory table.
  Turn Step 0 green. (Commit `RF.0/1`.)
- **Step 2 — `Document::ReindentLineRange` + `kActionReformatSelection` + keymap + handler.** Wire the
  explicit command end-to-end; integration tests in `test_document` (reformat a mis-indented selection;
  reformat single line with no selection; plaintext no-op) asserting buffer + cursor through the action.
  Live SDL feel-check. (Commit `RF.2`.)
- **Step 3 — Wrap integration (H.18b).** `TryWrapSelection` calls `ReindentLineRange` for block pairs only;
  integration test (wrap a two-line body with `{` → nested + closer placement). Update
  `docs/check-autopair.md` H.18b to RESOLVED. Live feel-check. (Commit `RF.3`.)

Per repo discipline: contract tests precede the code; `ReindentRange` is fully covered before it is wired
into the live edit path.

## Decisions — to confirm before Step 1

1. **No-selection scope:** reformat **just the current line** (simple, predictable) vs. the **whole
   enclosing block** (find the brace pair around the cursor — more work, needs a block-finder). *Suggested:
   current line for v1; enclosing-block later.*
2. **Wrap integration (Step 3):** auto-reindent on a `{ }`-wrap (fixes H.18b, but mutates more than the
   user typed) vs. **leave wrap faithful** and require the explicit reformat command afterwards.
   *Suggested: do it for block pairs only — it is the natural fix for H.18b and matches the user's framing.*
3. **Default key-binding** for `kActionReformatSelection` (per-platform). *Needs a pick.*
4. **Lines inside a multi-line string / block comment** within the range: v1 **reindents them like code**
   (the engine does not yet consult per-line token class). Acceptable for v1, or gate now? *Suggested:
   accept for v1, note as a refinement (most reformat targets are code blocks).*

## Out of scope / deferred

- **Multiline string / comment safety** — skipping (not touching) lines whose start token-class ∈
  `suppress_in`. Needs per-line token class in `RangeContext`; the hook is reserved (the field comment
  above). A v2 refinement once the v1 walk is proven.
- **Aligned (vs stepped) continuation indent** — lining up wrapped expression arguments under an open
  paren, switch-`case` indentation, access-specifier outdent. The walk is purely stepped in v1.
- **Format-on-paste** and whole-buffer reformat — same `ReindentRange` over a different range; trivial once
  the command exists, but not in v1 scope.
- **Retiring the tokenizer brace-depth `Line::indent`** — still kept for indent-guide drawing, unchanged
  by this work.
- **Language-architecture refactor** (class-per-language → data) — this feature is another step in that
  direction but does not perform it.
