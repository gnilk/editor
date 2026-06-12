# Open bugs / known-wrong code (cross-session tracker)

Durable list of known defects we've chosen NOT to fix in-place yet, with enough context to pick each
up cold. Pointer to this file lives in CLAUDE.md ("Remaining / deferred") so it surfaces each session.

---

## 1. `Line::AttributeAt(pos)` returns the WRONG span for any position in the last token span

**Where:** `src/Core/Line.cpp`, `Line::LineAttribIterator Line::AttributeAt(int pos)`.

**What's wrong:** the lookup loops over adjacent pairs `(i, i+1)` and returns span `i` when
`attribs[i].idxOrigString <= pos < attribs[i+1].idxOrigString`. For a `pos` at/after the LAST span's
start (`pos >= attribs[last].idxOrigString`) no pair matches, so it falls through to
`return attribs.begin()` — the FIRST span (almost always `kRegular`). So the token class of anything in
the last span of a line is misreported as code. A **trailing comment is always the last span**
(`foo(); // note`), so its class reads as `kRegular`.

**Discovered:** 2026-06-12, while making reformat token-aware (C.10). A `{` in a trailing `// ... {`
read as a real block opener.

**Why it's still open (don't naively "fix" it):** `Document::OnActionWordRight` (word-jump, right)
*depends on the buggy fallback*. With the cursor in the last token, `AttributeAt` returns `begin()`
(idxOrigString 0); the `attrib->idxOrigString < cursor.position.x` test is then true and routes
word-jump into its "jump to end of line" branch. A corrected `AttributeAt` (returning the real last
span) would instead fall through to the `else` branch and do `attrib++` → dereference `end()` (UB) when
the cursor sits exactly at the last token's start. So the method cannot be fixed in isolation.

**Current workaround (what "works" today):** the two reformat/indent token-class lookups do NOT call
`AttributeAt`. Each does a correct ascending scan ("the span whose start is the greatest
`idxOrigString <= x`"):
- `TokenClassAtChar` in `src/Core/Document.cpp`
- `TokenClassAtChar` in `src/Core/Language/IndentEngine.cpp`
(near-duplicate of each other — the duplication is the cost of not fixing the shared method.)

**Proper fix (its own small branch):**
1. Fix `AttributeAt`: when `pos >= attribs.back().idxOrigString`, return `attribs.end() - 1`.
2. Rewrite `Document::OnActionWordRight` so it no longer relies on the `begin()` fallback (and guard the
   `attrib++` against `end()`); re-verify word-jump-right behavior (esp. cursor at the last token's start
   and at end-of-line).
3. Collapse both `TokenClassAtChar` helpers back onto `AttributeAt`.
4. Add a `Line::AttributeAt` unit test covering: pos in first span, middle span, last span, and past EOL.

**Other callers to re-check when fixing:** `Document::OnActionWordLeft` (uses `AttributeAt` too),
`EditController.cpp` `TokenClassAt` (already guards `== attribs.end()`, which `AttributeAt` never returns
today — revisit once it can).

---

## 2. `Document::SetCursorPosition` writes an ABSOLUTE line index into the screen-relative `cursor.position.y`

**Where:** `src/Core/Document.cpp`, `Document::SetCursorPosition(idxLine, idxChar)` — `GetCursor().position.y = idxLine;`.

**What's wrong:** `cursor.position.y` is the SCREEN row the caret is drawn at — `EditorView::SetWindowCursor`
hands `position.y` straight to the native caret with no `viewTopLine` subtraction, and the vertical-nav model
maintains it as `position.y = idxActiveLine - viewTopLine` (see `VerticalNavigationViewModel.cpp`).
`SetCursorPosition` instead assigns the ABSOLUTE `idxLine`. It then calls `RefocusViewArea()`, which adjusts
`viewTopLine`/`viewBottomLine` but does NOT recompute `position.y`. So whenever the target ends up on a
scrolled view (`viewTopLine != 0`) the caret is drawn at the wrong row / off-screen.

**Symptom path:** `JumpToSearchHit` → `SetCursorPosition` — jumping to a search hit deep in the file scrolls
the view (so `viewTopLine` becomes large) yet leaves `position.y` at the absolute hit line, so the caret is
mis-placed. Any other "go to line N" caller is affected the same way.

**Discovered:** 2026-06-12, while fixing the block-surround / inline-wrap "cursor gone" bugs (RF.2f), which
were the same defect class in the reformat paths. Those were fixed locally (write screen-relative y +
`RefocusViewArea`); `SetCursorPosition` itself was left alone as it's a wider-blast-radius shared method.

**Proper fix:** after `RefocusViewArea()`, set `position.y = idxLine - viewTopLine` (clamped >= 0). Re-verify
every caller: `JumpToSearchHit` (search nav), plus any "goto line"/jump callers. Add a unit test:
`SetCursorPosition` to a line below the fold on a short, scrolled view → assert
`position.y == idxActiveLine - viewTopLine` and the line is inside `[viewTopLine, viewBottomLine]`.

**Reference pattern (already fixed this way):** `Document::SurroundLineRangeWithBlock` and
`EditController::TryWrapSelection` both now write `position.y = absoluteLine - viewTopLine` (+ refocus where
the line count changed) — copy that.

---

## 3. Syntax highlighter mis-tags an identifier that directly abuts `{` (no separating whitespace)

**Where:** the tokenizer — `src/Core/Language/LangLineTokenizer.cpp` and/or the CPP config in
`CPPLanguage.cpp` (token boundary handling when an identifier abuts an opener). NOT yet root-caused.

**Symptom (observed, not yet traced):** with the caret at `{|foo()` — i.e. `{` immediately followed by an
identifier with NO space between — the highlighter tags `foo` with a non-identifier class (looks like an
operator: it renders the SAME color as the brace). Typing a single space after the `{` (then it
re-tokenizes) fixes the coloring.

**Repro:**
```cpp
static void func() {foo();
}
```
Place the caret between `{` and `foo` and observe `foo`'s color; insert a space → correct.

**Hypothesis (unverified):** the tokenizer doesn't break a token at the `{`/identifier boundary when they
abut, so `{foo` (or `foo` right after `{`) is matched against the operator/brace rule instead of starting a
fresh identifier token. The space forces a token boundary.

**Discovered:** 2026-06-12 (feel-check). Deferred to a later bug sweep — do NOT fix piecemeal.

**When fixing:** add a `test_cpplang` case asserting the token class of an identifier immediately following
an opener (`{foo`, and likely `(foo`, `[foo`) is `kRegular`/identifier, not operator/brace. Check the
tokenizer's longest-match / boundary logic at operator↔identifier transitions.

---

## 4. Whole-line cut/paste drops the trailing newline when the selection ends at EOL of the last line — FIXED 2026-06-12

**FIXED 2026-06-12 — TWO mechanisms (the GUI bug was the second one).**

**(A) Selection-end normalization (internal path).** A whole-line selection (starts at column 0, ends at the
END of its last line) has its end normalized to **column 0 of the following line** at the copy/cut site
(`Document::SelectionEndForCopy`, used by both `kActionCopyToClipboard` and `kActionCutToClipboard`; the cut
now calls `DeleteRange(start, normEnd)` instead of `DeleteSelection()`). That reuses the already-tested
`end.x==0` copy/delete paths. EOF selections stay charwise. Guards: `test_document_cut_paste_linewise`,
`_linewise_undo`, `_charwise`.

**(B) OS-clipboard round-trip (THE actual GUI manifestation).** The GUI paste does NOT use the internal item:
`Document::PasteFromClipboard` first calls `UpdateClipboardData()`, which re-reads the OS pasteboard as an
**external** item (`ClipBoard::CopyFromExternal`). The cut serializes to the OS clipboard via the backend's
`OnUpdate` hook. Two bugs there dropped the trailing newline: (1) the hook flattened `GetLineCount()` lines with
`TextBuffer::Flatten` (which truncated the resolved trailing-empty segment), and (2) `CopyFromExternal` used
`std::getline`, which **discards the final empty segment after a trailing `\n`**. So a whole-line copy round-tripped
as `"foo\nbar\n"` → `{"foo","bar"}` → paste joined the destination's next line (`bar();}`). Fix: a single
serialization point `ClipBoard::ClipBoardItem::AsText()` (resolved segments joined by `\n`, no phantom trailing
newline) used by **both** SDL2/SDL3 `OnUpdate` hooks, and `CopyFromExternal` now splits manually to **preserve the
trailing empty segment**. Guards: `test_clipboard_external_roundtrip_linewise` (the GUI path), `_charwise`
(control), `test_clipboard_external_trailing_newline`.

`NewUndoFromSelection` already snapshots `ptEnd.y+1` for the EOL case, so a single undo restores cleanly
(restore action becomes `kInsertAsNew`). The `ClipBoard` internal-paste layer (`ResolveSegments`/`PasteToBuffer`)
is unchanged. Original write-up kept below.

**Where:** the selection model + clipboard — `Document::UpdateSelection` (`Document.h`),
`ClipBoard::ClipBoardItem::ResolveSegments` / `PasteToBuffer` (`src/Core/ClipBoard.cpp`).

**What's wrong:** a selection's END is just the live caret — `UpdateSelection` sets
`end = (cursor.x, idxActiveLine)`. The clipboard's only "selection ended on a line break" signal is
`end.x == 0`; `ResolveSegments` appends the trailing-newline marker (an empty final segment) ONLY then.
So a "whole-line" selection whose caret finished at the END of the last line (`end.x == lastLine.length`,
not column 0 of the next line) is stored WITHOUT a trailing newline. On paste the multi-line splice does
`Insert(insertAt, segs.back() + tail)`, joining the destination's following line onto the last pasted line.

**Symptom (feel-check E.18):** cut a whole-line block including its braces, paste it back, and the line
after the paste point is pulled up onto the last pasted line:
```
    }|}     <- caret between the inner '}' (pasted) and the outer '}' (destination tail, joined on)
```
Trace matches exactly: `segs.back() = "    }"`, `tail = "}"` → `"    }}"`, caret returned at
`segs.back().length()` (col 5, between the braces).

**Not a corruption — a charwise-vs-linewise gap.** Selecting to the last *character* of a line is
charwise-correct (the newline isn't included), but a whole-line selection should paste linewise (each line
on its own, trailing newline preserved). There is no explicit linewise-selection concept today; `end.x == 0`
is the only proxy and the EOL-of-last-line case slips past it.

**Decided fix — Option A (capture a real linewise flag):** mark a selection/cut as *linewise* at the point
it is made (e.g. full-line selection / line-cut), carry that onto the `ClipBoardItem`, and have
`ResolveSegments` preserve the trailing newline for a linewise item regardless of `end.x`. Touches the
selection model + clipboard but is the semantically correct fix (rejected Option B: a clipboard heuristic
treating `end.x == lastLine.length` as a line-end — it conflates "selected to EOL charwise" with
"whole-line").

**When fixing:** reproduce-first with a clipboard-level unit test — copy a multi-line selection ending at
`(lastLine.length, lastY)` with the linewise flag set, paste before an existing line, assert the following
line stays on its own line (trailing newline preserved). Then a charwise selection ending at the same column
must still join (no newline). Check both cut and copy.
