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


## 9. 'Running NPM update in the terminal does not produce the expected output'
**Where:** Terminal emulator/window
**What's Wrong:** When running certain apps (in this case `npm update` on a project) the color output is missing
**Reproduce:** Initiate a project with npm (use older versions of some library) then do an update
There are possibly other applications also not working but this is one I found

## 10. 'Missing scrollback feature in terminal UI'
**Where:** Terminal emulator/window
**What's Wrong:** No scrolling through history buffer
**Reproduce:** just list a directory with many files - this is missing functionality - but it is a bit 
of a bummer - because you want this

**RESOLVED (Phase 0, ✅ done):** the scrollback *buffer* already existed (`TerminalScreen::scrollback`);
the defect was purely that `TerminalView::DrawViewContents` always pinned to the bottom. Phase 0
(TS-0a..TS-0f) shipped the scroll viewport (wheel + PageUp/Down + Home/End, abs-row anchored so a
streaming build doesn't yank you back to the bottom) — this bug is closed. Phase 1 (command blocks +
jump-per-prompt nav) also shipped on top of it. Remaining phases (downstream consumers, OSC 133,
per-block highlighting, persistence) are enhancements, not part of this bug. Full design + phase status:
[`terminal-scrollback.md`](done/terminal-scrollback.md).

---

## 11. Raw TAB byte (`0x09`) from the pty is silently dropped — garbles multi-column shell output

**Where:** `src/Core/Editor/Controllers/TerminalController.cpp`, `TerminalController::HandleTerminalData`
— the byte-dispatch `if/else if` chain after stripping ANSI commands only handles backspace (`0x08`),
newline (`0x0a`), carriage return (`0x0d`), printable `0x20-0x7e`, and `>=0x80` (UTF-8 continuation). A
raw `0x09` (tab) matches none of these branches, so it is silently consumed and discarded: no cursor
advance, no cell written.

**Symptom:** macOS BSD `ls`'s default multi-column listing (no `-la`) pads BETWEEN columns with tab
characters (its long-format `-la` output is one-entry-per-line and never hits this path, which is why
that one always rendered correctly). With the tab eaten, consecutive column entries land back-to-back
with zero separation — e.g. `CMakeCache.txt`, `_CPack_Packages`, `generated`, `resources` (4 separate
column entries) render as one fused string `CMakeCache.txt_CPack_Packagesgeneratedresources`.

**Likely related symptom:** the stray inverse-video `%` zsh sometimes prints before a prompt (including
after just pressing Enter on an empty line) is zsh's own `PROMPT_EOL_MARK` — "the previous line didn't
end at column 0". Because the eaten tab leaves our `cursor.x` short of where the real shell output left
it, our terminal's cursor column can desync from what zsh thinks it wrote, tripping that marker on the
next redraw even though nothing is actually wrong with that next command.

**Discovered:** 2026-06-23, manually verifying TS-0e/TS-0f (terminal-scrollback) — a plain `ls` in
the scrollback view showed the fused filenames above. Pre-existing in the live grid (not introduced by
the scrollback work); scrollback just faithfully preserves whatever was already in the grid, including
this.

**Proper fix:** add a `0x09` case to `HandleTerminalData` that advances `cursor.x` to the next tab stop
(traditionally a multiple of 8) — most simply by calling `screen.PutChar(U' ')` (or an equivalent
"advance without drawing a visible char if you want true passthrough") in a loop until the next stop,
mirroring how `NewLine()`/`PutChar()` already wrap at `cols`. Add a `test_terminalscreen` or
`test_terminalcontroller` case asserting a `0x09` byte mid-row advances the cursor to the next multiple-
of-8 column and fills the skipped cells with blanks (current pen colors), rather than being dropped.

## 12. 'Consolidate Terminal and CommandView in config.yml'
Note: This is not a bug per-se, more of a 'reduce noise' situation
**Where:** The CommandView and CommandController together with the Terminal implementation currently
using two different sections in the config file. This doesn't make sense from an end user perspective.
**ProperFix:** Consolidate everything under 'terminal' - it is the logical place. Just because it is
called 'CommandView' internally, doesn't mean it has surface to the user.