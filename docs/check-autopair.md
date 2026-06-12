# Auto-pair v1 — GUI feel-check checklist

Manual on-screen verification of auto-pairing v1. Open the editor with a folder (`./goatedit .`) and
create three scratch files to cover language selection: a `.cpp`, a `.json`, and a `.txt`.
`|` marks the cursor; check the box when the observed result matches.

## A. No pairing in plaintext (headline negative) — PASS
- [x] 1. In `readme.txt` (`default` language), type `(` → just `(`, **no** auto-`)`.
- [x] 2. Same for `[`, `{`, `"`, `'` → none pair. *(no `default` section ⇒ no pairing.)*

## B. Insert-pair (headline positive) — in a `.cpp` file — PASS
- [x] 3. Empty line, type `(` → `(|)` (cursor between).
- [x] 4. Same: `[` → `[|]`, `{` → `{|}`, `"` → `"|"`, `'` → `'|'`.
- [x] 5. cpp: type `<` → just `<`, **no** `>`. *(`<>` intentionally excluded — `<` is ambiguous with
  arithmetic/comparison in `for`/`while`/`if`. Implementation is correct; the original expectation was wrong.)*

## C. Guarded auto-close (don't pair mid-word) — PASS
- [x] 6. Cursor at `|word`, type `(` → `(word`, **no** closer. *(`w` ∉ auto_close_before.)*
- [x] 7. Cursor before `)`/`,`/`;`/whitespace/EOL, type `(` → **pairs**.

## D. Skip-over (type-through) — PASS
- [x] 8. From `(|)`, type `)` → `()|`, no second `)`.
- [x] 9. From `"|"`, type `"` → `""|`.

## E. Quotes — tricky guards — PASS
- [x] 10. Type `don`, then `'`, then `t` → `don't` (the `'` does **not** pair after identifier).
- [x] 11. In `"abc|"`, type `"` → skip-over to `"abc"|`, no new pair.

## F. Suppress in string / comment — PASS
- [x] 12. Inside `// comment`, type `(` → `(`, no pair. *(tokenClass == comment ∈ suppress_in.)*
- [x] 13. Inside string `"abc|def"`, type `(` → `(`, no pair. *(tokenClass == string.)*

## G. Backspace deletes the pair — PASS
- [x] 14. Type `(` → `(|)`, Backspace → both gone.
- [x] 15. Negative: in `(x|)`, Backspace deletes only `x` → `(|)`. *(only adjacent **empty** pair deletes both.)*

## H. Selection-wrap — PASS (H.18a resolved, H.18b resolved in RF.3)
- [x] 16. Select `word`, type `(` → `(word)`, cursor after closer, selection cancels.
- [x] 17. Select `word`, type `"` → `"word"`.
- [x] 18. Multi-line: select two lines, type `{` → opener before selection start, closer after selection end.
  **H.18a — RESOLVED (keep current / faithful):** the wrap inserts at the literal selection boundary
  (`endLine->Insert(end.x, …)`). When the selection includes the trailing newline (end at col 0 of the
  line past the block), the closer lands on a new line — this is the faithful reproduction of the
  selected range (matches VS Code), not a bug. Decision: keep it. No code change.
  **H.18b — RESOLVED (RF.3):** wrapping a **multi-line** selection with `{` is now a *block-surround* —
  the braces land on their **own lines** and the body is reindented one level (single-line / non-block
  pairs keep the faithful inline wrap). Built on `Document::SurroundLineRangeWithBlock` +
  `IndentEngine::ReindentRange`, committed as one undo item via the new `kReplaceRange` undo primitive.
  Covered by `test_document_reformat_surround{,_undo}`. **GUI feel-check still pending.**

## I. Per-language (json vs cpp) — PASS
- [x] 19. In a `.json` file: `(`/`[`/`{`/`"` pair, but `<` does **not** (`<|`). *(json inherits generic, no `<>`.)*

## J. Undo grouping & cursor feel — feels fine (stress-testing more)
- [x] 20. After insert-pair `(|)`, one Undo removes the whole pair in one step (no stray `)`).
- [x] 21. After skip-over, Undo restores the pre-skip cursor cleanly.
- [x] 22. Vertical nav after a skip-over keeps the expected column (`wantedColumn` recomputed on skip).

---

## Findings summary
- **B.5 expectation corrected:** cpp does **not** auto-close `<>` (arithmetic/comparison ambiguity);
  implementation is correct.
- **H.18a (resolved — keep current):** multi-line wrap is faithful to the selection boundary; closer on
  a new line is correct when the selection includes the trailing newline. No code change.
- **H.18b (deferred):** `{`-wrap doesn't re-indent the new block — indent-engine concern.
- **J (in progress):** undo/cursor feel fine on first pass; user stress-testing further.

Notes (step # + actual buffer/cursor; flag SDL3/macOS-specific issues):
