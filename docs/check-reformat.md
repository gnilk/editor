# Reformat-selection — GUI feel-check checklist

Manual on-screen verification of the reformat feature (`docs/reformat-plan.md`). Keys are
**Cmd** on macOS / **Ctrl** on Linux (`CopyPasteModifier`). Use a `.cpp` file for the indent rules and a
`.txt` for the plaintext no-op. `|` marks the cursor; check the box when the observed result matches.

Commands under test:
- **ReformatLine** — `Cmd/Ctrl+L` — re-derive the current line's indent.
- **ReformatBlock** — `Cmd/Ctrl+I` — selection → that line range; no selection → enclosing `{ }` block.
- **Block-surround** — type `{` over a *multi-line* selection — braces on their own lines + body nested.

## A. ReformatLine (Cmd/Ctrl+L)
- [ ] 1. Over-indent a body line by hand (`            foo();` inside a function), cursor on it, press
  `Cmd+L` → snaps to the correct one level (`    foo();`).
- [ ] 2. Under-indent a line (flush left inside a block), `Cmd+L` → indents to the right level.
- [ ] 3. An already-correct line + `Cmd+L` → unchanged (idempotent; press twice, nothing moves).
- [ ] 4. Anchor respected: the line above is the reference — a line right after `if (x) {` lands one level
  past it; a neutral line keeps the previous line's level.

## B. ReformatBlock with a selection (Cmd/Ctrl+I)
- [ ] 5. Select a mis-indented multi-line body, `Cmd+I` → every selected line re-derived (body to one level,
  a closing `}` line back out).
- [ ] 6. Selection spanning a nested block → inner block indents one level deeper than its `{`.
- [ ] 7. Linewise selection ending at column 0 of the line *past* the block → that trailing line is NOT
  reformatted (only the lines actually selected).

## C. ReformatBlock with NO selection — enclosing block (Cmd/Ctrl+I)
- [ ] 8. Cursor on a body line inside `{ … }`, no selection, `Cmd+I` → the whole enclosing block reformats
  (open line, body, close line).
- [ ] 9. Cursor inside a *nested* block → only the nearest enclosing `{ }` reformats (not the outer one).
- [ ] 10. Braces in strings/comments don't fool it: a `// }` or `"{"` on a line inside the block does not
  break the match (the block boundaries are still the real braces).
- [ ] 11. Cursor NOT inside any block (top level) + `Cmd+I` → just the current line reformats (graceful
  fallback, no wild range).

## D. Block-surround — multi-line `{`-wrap (the H.18b payoff)
- [ ] 12. Select two+ body lines, type `{` → braces land on their **own lines**, body indented one level:
  ```
  {
      foo();
      bar();
  }
  ```
- [ ] 13. The block nests relative to its surroundings (inside an already-indented scope it lands at that
  scope's level + 1, not column 0).
- [ ] 14. Single-line selection + `{` → still the **faithful inline** `{selected}` (NOT block-surround).
- [ ] 15. Non-block pair over a multi-line selection (`(`, `[`, `"`) → faithful inline wrap, **no**
  brace-on-own-line restructure.
- [ ] 16. Cursor lands sensibly after the surround (at/after the closing brace), no stray selection left.

## E. Undo (single press)
- [ ] 17. After a block-surround (D.12), **one** undo restores the original lines exactly — the two brace
  lines gone, body whitespace back to original. Not two presses, no stray `{`/`}` left.
- [ ] 18. After a ReformatLine / ReformatBlock, one undo restores the original indentation of the whole
  affected range in a single step.
- [ ] 19. Redo after the undo re-applies cleanly.

## F. Plaintext / no-op
- [ ] 20. In a `.txt` file (no indent table), `Cmd+L` / `Cmd+I` → nothing changes (no reflow, no crash).
- [ ] 21. Block-surround path: multi-line selection + `{` in `.txt` → no pairing/surround at all (autopair
  is already off for plaintext).

## G. Feel / edge cases
- [ ] 22. Blank lines inside a reformatted range come out **empty** (no trailing-whitespace indentation).
- [ ] 23. Reformat near the top of the file (range starting at line 0) behaves (anchor = level 0).
- [ ] 24. Reformat a range/block ending mid-construct (selection stops inside a block comment) → the
  forward-extend completes the construct rather than leaving it half-reformatted.
- [ ] 25. Cursor column after any reformat stays valid (line shrank/grew) — vertical nav afterwards lands
  where expected.

---

## Findings summary
(record step # + actual buffer/cursor; flag SDL3/macOS-specific issues)
