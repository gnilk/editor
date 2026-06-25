# Per-block syntax highlighting (terminal command blocks) — design / deferred

Status: **DEFERRED / not started.** Extracted from the (now closed) terminal-scrollback effort
([`done/terminal-scrollback.md`](done/terminal-scrollback.md)) so that doc could close cleanly. This is an
*optional, independent* follow-on — it depends only on the command-block infrastructure that already
shipped (terminal-scrollback Phases 0–3 + 5); nothing here is on a critical path. No effort spent yet.

## Goal

Detect a tool command (`cmake …`, `make`, `cargo`, …) and highlight *that command block's* output lines
with the matching language — e.g. CMake syntax colors over a `cmake` run — **without splitting the
scrollback buffer**, and without disturbing the surrounding ANSI-colored history.

## What already exists (the seam)

The terminal scrollback is one shared, append-only `TextBuffer`; a command block
(`TerminalScreen::CommandBlock`) is a line-range **reference** `[startAbsRow, endAbsRow)` into it (not a
copy). Two facts shape the design:

- A block already resolves to a contiguous line range and carries an optional `LanguageBase::Ref language`
  (`nullptr` = plain ANSI colors). The persisted DTO (`TerminalBlockSession`, terminal-scrollback §8.1)
  carries a `std::string language` id too — so the storage seam round-trips across sessions already. v1
  only had to **not preclude** this, and it doesn't: the `language` field and block→line-range resolution
  are both in place.
- BUT `TextBuffer::SetLanguage` + `Reparse` are **buffer-global**, and the tokenizer threads state
  line-to-line (`Line::stateDepthAtStart`). So you cannot natively highlight lines 10–50 as CMake and
  51–80 as plain in one buffer today.

## The reachable design — "hard-region tokenize"

Give the block a language and run a region tokenize over *its* line range, writing the resulting attribs
into those lines. "Hard-region" is the operative word: the normal reparse walks *backwards* from the edit
to a line where the tokenizer state stack is at depth 1 (a safe restart point). For a block we want a
**fixed boundary** — the block's first line IS the reset, never look back past it, so the previous block's
tokenizer state can't bleed in.

`TextBuffer::ReparseRegion(idxStartLine, idxEndLine)` is the starting point, but:

- (a) it uses the buffer's single global `language`, so it must take an **explicit** language (or read a
  line-range→language map);
- (b) the tokenizer runs on a **background thread / `Job`** — so this is not a trivial add: the region
  parse must target exactly the block's range, write attribs back safely, and not disturb neighbours, all
  while the scrollback buffer is otherwise append-only.

The append-only / read-only nature of historical blocks helps (a finished block's lines never change), but
the threading is the real cost.

## Work packages

- **TS-4a — command→language map.** argv0 → `LanguageBase` (a small config-driven map); set
  `block.language` on block open. The command text is already captured on the block.
- **TS-4b — hard-region tokenize.** New `TextBuffer` capability: reparse a block's line range under an
  *explicit* language with a *hard* start boundary (no look-back past the block), writing token attribs.
  Runs on a background `Job`. **The bulk of the work.**
- **TS-4c — block-boundary isolation.** Reparsing block N must not bleed tokenizer state into block N±1.

## Tests (a new `terminalblocks`-style module)

- A `cmake` block's lines get CMake token attribs.
- An un-languaged block keeps its ANSI attribs.
- Hard-region reparse of block N leaves block N±1 untouched (no state bleed across the boundary).

## Settled decision (carried over)

Per-block highlighting **storage**: blocks are line-range *references* into one shared scrollback
`TextBuffer` (not split per-block); per-block language is an additive region-reparse. This was the chosen
direction during the terminal-scrollback design — recorded so it isn't re-litigated.

## Related

- Block infrastructure (line-range refs, abs-id spine, the `language` field, persistence):
  [`done/terminal-scrollback.md`](done/terminal-scrollback.md) §3.2 / §8.
- Tracked as deferred in [`deferred.md`](deferred.md).
