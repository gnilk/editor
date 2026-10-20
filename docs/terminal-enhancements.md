# Terminal enhancements — running the Claude CLI (and other modern TUIs) in the embedded terminal

**Status:** analysis + implementation plan, nothing implemented. Written 2026-10-20 against `main` @ `660a586`.
**Trigger:** starting `claude` in the editor's terminal "didn't fly at all".

The short version: the Claude CLI is not exotic — it is a normal raw-mode TUI that speaks a mainstream
xterm dialect. It fails in GoatEdit for a handful of **general** terminal-emulation gaps, any one of which is
enough to wreck it. Fixing them makes the terminal correct for the whole class (fzf, htop, npm/inquirer
prompts, tmux, …), not just Claude. Nothing here needs a Claude-specific hack.

---

## 1. How this was established (evidence, not guesswork)

1. **Read the terminal stack:** `VTermParser`, `TerminalController`, `TerminalScreen`, `TerminalView`,
   `Shell` (pty), and the keymap dispatch (`Runloop` → `RootView` → `TerminalView`).
2. **Captured what the CLI actually emits.** `claude` 2.1.304 was run under a pty (100×30,
   `TERM=xterm-256color` — exactly what `Shell::Begin` sets) in a scratch folder, recording every byte:
   startup, the folder-trust dialog, the main input screen, typing `hello`, Backspace×2, Ctrl-C×2. Done twice
   more: once answering its terminal queries like xterm would, once with `COLORTERM=truecolor`. Recipe in
   §7.
3. **Replayed the captures through the editor's real code.** A temporary trun case fed each capture into a
   default-constructed `TerminalController::HandleTerminalData` (4096-byte chunks, as `Shell::ConsumePty`
   delivers them, and 64-byte chunks to model a split read) and dumped the resulting `TerminalScreen` grid.
   The grid excerpts quoted below are that output — i.e. what the editor's model really ends up holding.

---

## 2. What the Claude CLI does on the wire (observed, v2.1.304)

Two rendering phases in one binary:

- **Inline phase (normal screen)** — startup and dialogs such as *"Accessing workspace … Is this a project you
  created or one you trust?"* are drawn in the normal screen, Ink-style: relative cursor moves
  (`CSI n A/B/C/D`), erase-in-line, and `\r\r\n` line breaks. After drawing, the cursor is parked **inside**
  the dialog (`CSI 4A` — four rows above the bottom of what it drew).
- **Fullscreen phase (alternate screen)** — once past the dialogs: `OSC 0` title, `CSI ?1049h`, `CSI 2J`,
  `CSI H`, mouse tracking on, then absolute-positioned redraws (`CSI row;col H`, `CSI n B` + `\r`).

| Feature | Sequence(s) seen | Count in main capture |
|---|---|---|
| Column positioning (CHA) — **used instead of spaces between words** | `CSI n G` | 78 |
| Relative cursor moves | `CSI n A/B/C/D` | ~80 |
| Absolute position, erase | `CSI r;c H`, `CSI K`, `CSI 2J` | 25 / 43 / 1 |
| 256-colour / 24-bit colour SGR | `38;5;n`, `48;5;n` — or `38;2;r;g;b`, `48;2;r;g;b` when `COLORTERM=truecolor` | 26 |
| Dim / normal intensity, bold, underline | SGR `2`, `22`, `1`, `4`, `39`, `49` | |
| Save/restore cursor, scroll-region reset | `ESC 7`, `CSI r`, `ESC 8` | |
| Cursor hide/show (around every frame) | `CSI ?25l` / `CSI ?25h` | 9 / 9 |
| Bracketed paste, focus events, `?2031` | `CSI ?2004h`, `CSI ?1004h`, `CSI ?2031h` (+ `l`) | |
| Alternate screen | `CSI ?1049h` | 1 |
| Mouse tracking (SGR encoding) | `CSI ?1000h ?1002h ?1003h ?1006h` | |
| xterm modifyOtherKeys | `CSI >4m` | 1 |
| Charset reset | `ESC ( B`, `SI` (0x0F) | |
| Window title | `OSC 0;✳ Claude Code BEL` | |
| **Queries** (sent after the first frame) | `CSI >0q` (XTVERSION), `CSI ?u` (kitty keyboard flags), `OSC 7501;? BEL`, `CSI c` (DA1, the "end of replies" sentinel) | |
| Non-ASCII text | box drawing `─ ╭ ╮`, blocks `▐ ▛ █ ▜ ▝`, `❯ ⏵ ⚠ ◐ ·`, U+00A0 | |

With no replies the CLI carries on after its DA1 sentinel; the editor already answers DA1 (`CSI ?1;2c`), so
**query replies are not a blocker** (nice-to-have, §4.H).

Not seen in this capture but standard for the same class and cheap to cover: synchronized output
(`CSI ?2026h/l`), OSC 8 hyperlinks, OSC 52 clipboard, OSC 9/777 notifications, combined private modes
(`CSI ?1049;25h`), DECRQM (`CSI ?n$p`).

Keys the CLI tells the user about on screen: **Enter** to confirm, **Esc** to cancel/interrupt, **Up/Down** in
lists, **Shift+Tab** to cycle modes, **Ctrl-C** twice to exit. Plus paste (multi-line prompts).

---

## 3. What the replay shows (the symptoms, explained)

Trust dialog, replayed as-is (inline phase):

```
00 |(B                                                ← ESC ( B printed as text
03 |AEnterto confirm ·Esc to cancel                   ← CSI ?u taken as "restore cursor" → overdraw
08 |Quicksafetycheck:Isthisaprojectyoucreatedoroneyoutrust?(Likeyourowncode,a   ← CSI n G ignored
16 |❯No,exit
17 |Yes,Itrustthisfolder
```

Main screen (alternate screen), 4096-byte chunks:

```
01u| ▐..███..█Claude Codev2.1.304     ← "▛" (E2 96 9B) broken into "..";  'u' = underline from CSI >4m
26 |  Try "write a test for <filepath>"
27 |❯ Try "write a test for <filepath>" ← duplicated row; the "◐ medium · /effort" row is missing
```

Same capture, 64-byte chunks (a split read — realistic once frames exceed one `read()`):

```
==== alt=0                              ← CSI ?1049h was split → never entered the alternate screen
01u|;174m ▐..███..█Claude Codev2.1.304  ← tail of a split SGR printed as text
29 |[2220m⏵⏵ auto mode on (shift+tab to cycle);1H
```

With `COLORTERM=truecolor` (inherited whenever goatedit is started from a terminal that exports it):
rows with yellow text (`38;2;255;193;7`) get **inverse video** set — the trailing `7` is read as SGR 7.

And in the editor UI on top of all that: in the inline phase keystrokes never reach the CLI (local line
editing), so the trust dialog cannot even be answered; in the fullscreen phase Esc, Shift+Tab and paste are
swallowed.

---

## 4. The gaps

Priorities: **P0** = blocks Claude (and the TUI class) outright; **P1** = usable but broken in obvious ways;
**P2** = polish/fidelity.

### A. Input routing — who owns the keyboard (P0)

`TerminalController::HandleKeyPress` has three paths: alt-screen → forward raw; `doesShellOwnLineEditing`
(Tab completion) → forward raw; otherwise **local line editing**, sending `text + "\n"` on Enter
(`CommitLine`). The local editor is a feature for the *shell prompt*, but it is applied to **any program in
the foreground that has not switched to the alternate screen**. A raw-mode program in the normal screen
(Claude's inline phase, fzf `--height`, inquirer/npm prompts, `ssh` host-key prompts, …) receives nothing until
Enter — and then gets `text\n` (LF, not the CR a raw-mode app expects for Enter). Arrow keys go to the editor's
command history instead.

**Fix.** Make the rule "local line editing applies only while the **shell itself** is the foreground job":
- `Shell` already has the child `pid` (the shell is the session leader, pgid == pid). Expose
  `IsShellForeground()` = `tcgetpgrp(amaster) == pid` (works on the pty master on Linux and macOS — verify on
  both). When a job is in the foreground → **passthrough** (forward raw, kernel line discipline does cooked
  editing/echo for cooked-mode programs, exactly like a real terminal).
- When OSC 133 shell integration is on, `;C` (output start) / `;D` (command end) give the same signal
  precisely; use them when present, pgrp otherwise.
- Becomes a fourth orthogonal input in the established priority order: (1) alt-screen, (2) **foreground
  job ≠ shell**, (3) shell-owns-line, (4) local edit. Paths 1–3 share `ForwardKeyPressToShell`.
- Check on each key press (one syscall) and in `HandleTerminalData` (the view needs it for rendering, §4.D).

### B. Parser — `VTermParser` (P0 unless noted)

1. **Not a stream parser.** `HandleTerminalData` builds a fresh `VTermParser` per chunk; a sequence (or a
   UTF-8 character) split across `read()`s is truncated, and its tail printed as text. `Shell::ConsumePty`
   coalesces up to 4096 bytes, so any frame larger than that — or any read that happens mid-write — splits.
   Fix: make the parser a persistent member with a state machine (ground / escape / CSI-param /
   CSI-intermediate / OSC-string / DCS-passthrough / …, per vt100.net's DEC ANSI parser) that keeps partial
   state between calls, plus a carried-over incomplete UTF-8 tail (`HandleTerminalData` currently emits `.`
   for a truncated sequence). The file's own header comment already calls for "stream based / state machine".
2. **8-bit C1 controls in a UTF-8 stream.** 0x9B (CSI), 0x9D (OSC), 0x90 (DCS), 0x9C (ST) are treated as
   controls, but they are ordinary UTF-8 *continuation* bytes: `▛` = `E2 96 9B`, `✳` = `E2 9C B3`. Fix: in
   UTF-8 mode never interpret 0x80–0x9F as C1 (xterm/VTE do the same). This also stops OSC 7/133 payloads
   being cut at 0x9C.
3. **Missing CSI finals that the CLI uses:** `G` (CHA — 78 uses; the "no spaces between words" symptom). Add
   the rest of the common set at the same time: `d` (VPA), `E`/`F` (CNL/CPL), `X` (ECH), `S`/`T` (scroll
   up/down), `` ` `` (HPA), `b` (REP), `Z` (CBT), `g`/`ESC H` (tab stops — optional).
4. **Parameter prefix is discarded.** `<`, `=`, `>` are skipped and `?` only sets a flag, so prefixed
   sequences dispatch as their unprefixed namesakes:
   - `CSI >4m` (modifyOtherKeys) → SGR 4 underline. `CSI >…m` must be ignored (or recorded, §4.E).
   - `CSI ?u` (kitty query) / `CSI >1u` (push) / `CSI <u` (pop) → **restore cursor**. Only bare `CSI u` is
     SCORC.
   - `CSI >c` (DA2) → answered as DA1. Reply `CSI >0;10;1c`-style or ignore.
   Fix: carry the prefix byte into dispatch and key on (prefix, intermediates, final).
5. **nF escapes.** `ESC ( B` (designate G0), `ESC ) 0`, `ESC # 8`, … — the intermediate byte isn't consumed,
   so `(B` is printed. Fix: consume `ESC <0x20–0x2F>+ <final>`; optionally implement DEC Special Graphics for
   `ESC ( 0` (line drawing used by tmux, mc, dialog) — P2. `SI`/`SO` (0x0F/0x0E) are already dropped; fine.
6. **SGR.**
   - `38;2;r;g;b` / `48;2;r;g;b` (and the colon form `38:2::r:g:b`) unsupported; the components are then
     walked as standalone codes (`…;7` → inverse, `…;0` → reset, `…;31` → red). Needs a new
     `kSetForegroundRGB`/`kSetBackgroundRGB` command.
   - Missing: `2` dim, `22` normal intensity, `23` no italic, `24` no underline, `27` no inverse, `9`/`29`
     strike, `53`/`55`. Today `22` is ignored, so bold never turns off.
   - `TerminalScreen::SetAttributes` **replaces** the pen attributes, so `SGR 1` then `SGR 4` loses bold. It
     must set/clear individual bits.
7. **One private mode per sequence.** `CSI ?1049;25h` handles only 1049. Loop over all params.
8. **String terminators.** `OSC_ParseStringToBel` stops only at BEL — an ST-terminated (`ESC \`) title eats
   the rest of the buffer. DCS/APC/PM skip until byte 0x9C only, so a 7-bit `ESC \` terminator eats the rest
   of the buffer. Accept BEL, `ESC \` (and, outside UTF-8 mode only, 0x9C) everywhere. `std::stoi` on an OSC
   number can throw on a malformed/empty number — guard it.
9. **Logging cost (P2).** `Parse` hex-formats every chunk into a string for a Debug log even when debug is
   off. Gate it.

### C. Screen model — `TerminalScreen` (P0/P1)

1. **No deferred wrap (DECAWM "pending wrap") — P0.** `PutChar` wraps the moment the last column is written.
   Real terminals set a pending-wrap flag and wrap only when the *next* printable arrives (cleared by CR, cursor
   moves, etc.). The CLI draws full-width rules (`─` × cols) followed by `\r\r\n`; without pending wrap every
   such line produces an extra blank row and the relative-move bookkeeping (`CSI n A` back up) lands on the
   wrong rows — the duplicated/missing rows above. Also honour `CSI ?7l` (autowrap off).
2. **Wide characters — P1.** Every code point takes one cell. Emoji and CJK (in Claude's *conversation*
   content, file names, etc.) are two cells wide, so everything right of them shifts. Needs a `wcwidth`
   (Unicode East-Asian-Width + emoji presentation) table, a 2-cell write with a continuation cell, and
   combining marks appended to the previous cell. Render side must skip continuation cells.
3. **Resize while in the alternate screen — verify.** Resize must resize `savedGrid` too and clamp the saved
   cursor; the CLI redraws on SIGWINCH. Add a test.

### D. Rendering — `TerminalView` (P0/P1)

1. **Normal-screen view is a shell-prompt composite — P0 (with §A).** In shell mode the view draws history
   rows `[0..cursor.y)`, then on the cursor row only the cells **left of the cursor**, then the local
   `inputLine`. Anything right of or below the cursor is never drawn. The CLI parks the cursor inside its
   dialog, so the dialog's bottom rows and the right half of the cursor row are invisible. Fix: when a
   foreground job owns the terminal (§A), draw the live grid like the alt-screen path does (all rows,
   cursor at the grid cursor, no `inputLine` overlay), keeping the scrollback-paging affordances. Same
   "one model flag, the view picks the render path" pattern as `isAltScreen`.
2. **Live cells ignore attributes — P1.** `DrawScreenRow` batches by fg/bg only; bold, dim, italic,
   underline and inverse are lost on the live grid (they do render once a row reaches scrollback, via
   `RowToLine`). Batch by (fg, bg, attrs) and use `DrawStringWithAttributesAt`; implement inverse as an fg/bg
   swap and dim as a blend toward the background.
3. **Cursor visibility — P2.** `CSI ?25l/h` is parsed and dropped. Track it on the screen and let the view
   hide the caret; avoids a caret flickering through every frame.
4. **Redraw cadence — P2.** One `TriggerUIRedraw` per chunk can show a half-drawn frame. Honour synchronized
   output (`CSI ?2026h` … `l` — defer the redraw until the end marker, with a timeout) and/or coalesce
   redraws per UI tick.

### E. Keyboard encoding — `ForwardKeyPressToShell` / `ForwardActionToShell` (P0/P1)

1. **Non-ASCII text is truncated — P0.** `shell.Write((uint8_t)keyPress.key)` sends one byte of a
   `char32_t`; å/ä/ö, `€`, any non-Latin-1 character is corrupted. Encode UTF-8.
2. **Keys bound to editor actions are swallowed in passthrough — P0.** In alt-screen mode `TerminalView`
   routes every *action* to `ForwardActionToShell`, whose `default: return true` swallows anything it
   doesn't translate. With the terminal keymap inheriting `default_keymap`:
   - **Esc** = `EnterCommandMode` → swallowed (Claude's cancel/interrupt key).
   - **Shift+Tab** = `Unindent` → swallowed (Claude's mode cycle; should send `CSI Z`).
   - **Cmd+V** = `PasteFromClipboard` → swallowed (see §F).
   - The view-cycling / focus actions → swallowed too, so **while a fullscreen app runs you cannot leave the
     terminal by keyboard** (only by mouse click).
   Fix: in passthrough, translate the keys an app needs (Esc → `0x1b`, Shift+Tab → `CSI Z`, Enter → CR, …)
   and let a small **escape-hatch set** of editor actions through untouched — focus/view cycling, the
   UI-navigation-modifier shortcuts, paste/copy. Everything else is forwarded, not swallowed. Decide the
   escape-hatch set explicitly (open question §6).
3. **Modifiers on special keys — P1.** Ctrl/Alt/Shift + arrows/Home/End/PgUp/PgDn/F-keys must use the xterm
   form `CSI 1;<1+mods> X` / `CSI n;<1+mods> ~`. Alt+char → `ESC` prefix (meta). Option/Shift+Enter →
   `ESC CR` (the conventional "insert newline" in Claude and other line editors).
4. **Ctrl+non-letter — P2.** `Ctrl+@ / [ / \ / ] / ^ / _ / Space` map to 0x00, 0x1B–0x1F; only `[` is handled.
5. **Keyboard protocols — P2.** `CSI >4m` (modifyOtherKeys) and the kitty keyboard protocol (`CSI >flags u`)
   are opt-in by the app; not replying to `CSI ?u` keeps the CLI on legacy encoding, which is fine. Only
   implement if a concrete need appears (the gansi `InputParser` already *decodes* kitty input — the reverse
   direction).

### F. Paste (P1)

There is no paste path into the terminal at all (paste is `Document`-only). Add one:
- local-edit mode: insert into `inputLine` (first line; multi-line → decide: send line by line or refuse);
- passthrough: send the text, wrapped in `ESC[200~` … `ESC[201~` when the app enabled bracketed paste
  (`CSI ?2004h` — track the mode on the screen/controller). Strip `ESC[201~` from the pasted text itself.
- Per the **E.18 lesson**, GUI paste arrives as an OS-pasteboard *external* item — test over that round-trip.

### G. Mouse, focus, notifications (P1/P2)

1. **Mouse reporting — P1.** The CLI turns on `?1000/1002/1003` + `?1006` (SGR encoding). In the alternate
   screen there is no scrollback, so the wheel is how the conversation scrolls; `TerminalView::OnMouseEvent`
   returns `false` for the wheel in alt-screen. Track the requested modes; when on, encode wheel (buttons
   64/65) and clicks as `CSI < b;x;y M/m` and send them. Keep a modifier (e.g. Shift+wheel) to force local
   behaviour, as most terminals do.
2. **Focus events — P2.** When `?1004` is on, send `CSI I` / `CSI O` from `TerminalView::OnActivate`.
3. **OSC 8 hyperlinks / OSC 52 clipboard / OSC 9 & 777 notifications — P2.** Parse cleanly (they are
   consumed today only if BEL-terminated); act on them later.

### H. Environment and query replies (P2)

- `TERM=xterm-256color` is set in the child (`Shell::Begin`); `COLORTERM` is **inherited** from however the
  editor was launched. Until §B.6 lands, *unset* `COLORTERM` in the child so apps stay on 256 colours; after
  it lands, set `COLORTERM=truecolor` explicitly so behaviour doesn't depend on the launch path. Consider
  `TERM_PROGRAM=goatedit` (+ `TERM_PROGRAM_VERSION`).
- Replies: DA1 exists. Add DA2 (`CSI >c`), XTVERSION (`CSI >0q` → `DCS >|goatedit(ver) ST`), DECRQM for the
  modes we support (`CSI ?n$p` → `CSI ?n;1|2$y`), `CSI ?6n` (DECXCPR). OSC 10/11 colour queries → reply with
  the theme's terminal fg/bg (lets apps pick a light/dark palette). All optional for Claude.

---

## 5. Implementation plan

Order is chosen so each step is independently testable and the CLI becomes usable after Phase 1. Every
package follows the repo's reproduce-before-fix rule: write the failing test first (for the parser/screen
these are byte-stream → grid assertions, §7), then fix.

### Phase 1 — make it work (all P0)

| WP | What | Where | Test |
|---|---|---|---|
| TE-1 | Stateful stream parser: persistent member, DEC-style state machine, partial-sequence + partial-UTF-8 carry-over; never treat 0x80–0x9F as C1 | `VTermParser`, `TerminalController::HandleTerminalData` | Same stream fed whole vs. split at **every** byte offset → identical grid (property test) |
| TE-2 | Prefix/intermediate-aware CSI dispatch; nF escapes consumed; all private-mode params looped; ST handling for OSC/DCS/APC/PM | `VTermParser` | `CSI >4m` sets no attr; `CSI ?u` doesn't move the cursor; `ESC ( B` prints nothing; `CSI ?1049;25h` sets both |
| TE-3 | CSI `G d E F X S T \` b Z` | `VTermParser`, `HandleAnsiCmd`, `TerminalScreen` | Per-command grid assertions; the trust-dialog text has its spaces |
| TE-4 | SGR: truecolor (`;` and `:` forms), 2/22/23/24/27/9/29; `SetAttributes` → set/clear bits | `VTermParser`, `HandleAnsiCmd`, `TerminalScreen` | `38;2;255;193;7` → that fg, **no** inverse; `1` then `4` keeps bold |
| TE-5 | Deferred (pending) wrap + `?7` autowrap | `TerminalScreen::PutChar` & every cursor mover | Full-width line + `\r\n` advances exactly one row |
| TE-6 | Foreground-job detection → passthrough input + full-grid render in the normal screen | `Shell`, `TerminalController::HandleKeyPress`, `TerminalView::OnAction/DrawViewContents` | Controller test with an injected "foreground ≠ shell" flag: keys forwarded raw, Enter → CR; view draws rows below the cursor |
| TE-7 | Key encoding: UTF-8, Esc, Shift+Tab → `CSI Z`, Enter → CR; passthrough forwards instead of swallowing; explicit escape-hatch action set | `ForwardKeyPressToShell`, `ForwardActionToShell`, `TerminalView::OnAction` | Byte-exact output for each key (write into a pipe/mock instead of the pty) |
| TE-8 | Child env: unset `COLORTERM` (until TE-4) | `Shell::Begin` | — |

**Exit criterion:** `claude` started inside goatedit shows a correct trust dialog that can be answered with
arrows + Enter, a correct main screen, accepts typed (incl. non-ASCII) prompts, Esc interrupts, Shift+Tab
cycles, Ctrl-C×2 exits back to a working shell prompt. Plus the TE-1 replay fixtures pass.

### Phase 2 — make it good (P1)

| WP | What |
|---|---|
| TE-9 | Paste into the terminal (local + passthrough, bracketed paste mode tracking) — test over the external-clipboard round-trip (E.18) |
| TE-10 | Live-grid attributes in `DrawScreenRow` (bold/italic/underline/inverse/dim) |
| TE-11 | SGR mouse reporting (wheel + clicks) when the app enables it; Shift-modifier override |
| TE-12 | Modifier-encoded special keys, Alt-as-meta, Option/Shift+Enter → `ESC CR` |
| TE-13 | Wide characters + combining marks (wcwidth table, continuation cells) |
| TE-14 | Alt-screen resize correctness (verify + test) |

### Phase 3 — polish (P2)

Cursor visibility (`?25`), synchronized output (`?2026`) / redraw coalescing, focus events (`?1004`), query
replies (DA2, XTVERSION, DECRQM, OSC 10/11), `COLORTERM=truecolor` + `TERM_PROGRAM`, DEC Special Graphics,
OSC 8/52/9/777, parser debug-log gating, `Ctrl+<symbol>` bytes.

---

## 6. Open questions / decisions for the owner

1. **Escape-hatch set in passthrough.** Which editor actions must still work while an app owns the
   keyboard? Proposal: view cycling/focus (`UINavigation` modifier combos), copy/paste, and nothing else.
   This interacts with the macOS Option-as-Alt setup (left Option = Alt = UI navigation): Claude users expect
   Option+Enter for a newline — so Alt+Enter must not be claimed by the editor in passthrough.
2. **Local line editor vs. passthrough at the shell prompt.** The local editor stays for the shell prompt
   (that's the existing design: history, plugin commands via `CommitLine`). TE-6 only changes behaviour when a
   *job* is in the foreground. Confirm that's the desired split.
3. **Multi-line paste at the local prompt.** Send as-is to the shell, or paste the first line only?
4. **Fixtures.** The captures contain the account plan string and local paths; for committed test
   fixtures either scrub them or hand-build equivalent byte streams (preferred — smaller, intent-revealing).

---

## 7. Recipe — capture and replay (for regression fixtures and future debugging)

**Capture** (Python, stdlib only): `pty.fork()` a child that sets `TERM=xterm-256color` (and optionally
`COLORTERM`), `chdir`s to a scratch folder, and `execvp("claude", ["claude"])`; set the size with
`ioctl(fd, TIOCSWINSZ, struct.pack("HHHH", 30, 100, 0, 0))`; `select`/`os.read` into a byte array while
writing scripted input (`\x1b[B` + `\r` to accept the trust dialog in a fresh folder — note the default
selection is *No, exit*; then `hello`, `\x7f\x7f`, `\x03`, `\x03`). Optionally answer queries
(`CSI c` → `CSI ?62;22c`, `CSI ?u` → `CSI ?0u`, `CSI >0q` → `DCS >|xterm(380) ST`) to compare behaviour.
Accepting the dialog records the scratch folder as trusted in `~/.claude.json`.

**Decode:** tokenize CSI / OSC / DCS / ESC / C0 with a regex and tally by (prefix, final) — that is how the
table in §2 was produced.

**Replay into the editor:** a trun case that reads a byte file, feeds it to a default-constructed
`TerminalController` via `HandleTerminalData` in N-byte chunks (`TerminalController c; c.Resize(100, 30);`
— no shell needed; DA/DSR replies go to `amaster == -1` and fail harmlessly), then dumps
`GetScreen().GetRow(y)` as text plus attribute markers. Chunk sizes 4096 (what `ConsumePty` delivers) and
small values (split reads) both matter. Keep the harness as a permanent `test_terminalreplay` module once
fixtures exist, asserting the expected grid text rather than printing it.
