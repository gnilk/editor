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

## 10. 'GansiDrawContext does not respect fg/bg colors when drawing overlays' — FIXED 2026-06-27
**Where:** GansiDrawContext::DrawLineOverlays
**What was wrong:** the cell-grid overlay path did a plain video-invert (`std::swap(cell.fg, cell.bg)`)
and ignored the application-set overlay color. `LineRender::DrawLines` points `fgColor` at the theme
`selection` color right before calling `DrawLineOverlays`, exactly as the SDL backends rely on.
**Fix:** highlight by blending the overlay color (`fgColor`) into each covered cell's BACKGROUND only
— glyph + its fg left intact so text stays readable on a grid with no alpha compositing. Covered by
`test_gansibackend_overlay`. **Caveat:** the blend carries two stopgaps — `if (a > 1) a /= 255`
(theme alphas are stored 0..255, not 0..1) and `a = 1.0 - a` ("invert for now", to match SDL's
faintness) — both tracked under bug 11 below; drop them together once 11 is fixed.

---

## 11. Theme/color alpha is stored 0..255, not normalized 0..1

**Where:** `src/Core/Sublime/SublimeConfigColorScript.cpp`, `ExecuteAlpha` —
`col.SetAlpha(args[0].Number())` stores the raw argument. `alpha(224)` (used by the content
`selection` color, `color(var(orange), var(selection_alpha))`) therefore yields `ColorRGBA::a = 224.0`,
even though `a` is treated as a 0..1 fraction everywhere else (the existence of
`ColorRGBA::AlphaAsInt()` = `a * 255` shows 0..1 was the intent).

**What's wrong / blast radius:** every consumer that reads alpha assumes 0..1:
- `ColorRGBA::AlphaAsInt()` returns `a * 255` → correct for a 0..1 alpha, but `224 * 255 = 57120` for
  the stored value.
- SDL `SDLColor` (SDL2 + SDL3) feeds `AlphaAsInt()` into `SDL_SetRenderDrawColor` — only "works"
  because 57120 wraps to a Uint8 of 32 (a faint tint). Accidental, not intentional.
- The Gansi overlay blend (bug 10) overflowed its `uint8` cast and rendered the selection **green**;
  it carries a local `if (a > 1) a /= 255` workaround as a result.

**Compounding (visual tuning, not just magnitude):** the theme alpha *values* look authored against
SDL's accidental faint render — `alpha(224)` reaches SDL as a Uint8 of 32 (≈ 0.12), not 0.88. To make
the ANSI selection match that faintness, `GansiDrawContext::DrawLineOverlays` carries a *second*
stopgap, `a = 1.0 - a` ("invert for now"), which turns the normalized 0.88 back into ≈ 0.12. So the
proper fix is not only "normalize at parse": once `a` means 0..1, the theme's alpha values almost
certainly need re-tuning (e.g. `alpha(224)` → ~`alpha(32)`), after which BOTH the `a > 1` guard AND the
`1.0 - a` invert in the Gansi blend should come out together.

**Why deferred:** the parser change itself is a one-liner (`SetAlpha(n / 255.0f)`, and audit the
`*`-alpha multiply in `ExecuteColor`), but it shifts the meaning of `ColorRGBA::a` for ALL consumers at
once — both SDL backends' `SDLColor::AlphaAsInt`, the JS/theme color APIs, any `FromRGBA(int…)` path
(already divides by 255), etc. Needs a coordinated sweep with a per-backend re-verify, so it's its own
branch.

**When fixing:** normalize at parse, then (a) drop the `a > 1` guard in
`GansiDrawContext::DrawLineOverlays` (bug 10), and (b) re-verify `SDLColor::AlphaAsInt` — the `* 255`
becomes genuinely correct once `a` is 0..1, but confirm nothing else relied on the wrap.

**Discovered:** 2026-06-27, while fixing the Gansi overlay color (bug 10) — the green selection traced
straight back to `fgColor.A()` returning 224.

