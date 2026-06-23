# Terminal scrollback + command blocks — design / spec

Status: **Phase 0 (scroll viewport) and Phase 1 (command blocks) DONE ✅ — TS-0a..TS-0f, TS-1a..TS-1c all
shipped, GUI-verified, in the verified-green test suite.** Phases 2-5 (downstream consumers, OSC 133,
per-block highlighting, persistence) remain — see §11 for what's left. Resolves
[`open-bugs.md`](open-bugs.md) #10 ("Missing scrollback feature in terminal UI") for the viewing/scrolling
half; the *grouping* infrastructure (jump-per-command, parse-build-output, open-output-as-document) the
user wants on top of it is built (blocks exist + nav works) but not yet consumed downstream. Written
cold-start so remaining phases can be picked up later without re-deriving the terminal model.

Sources this spec is grounded in (read these first when implementing):
- `src/Core/TerminalScreen.{h,cpp}` — the `cols × rows` grid + flat `scrollback` + alt-screen save/restore.
- `src/Core/Editor/Controllers/TerminalController.cpp` — `HandleTerminalData` (pty thread), `CommitLine`,
  the alt-screen / shell-owned-line state machine.
- `src/Core/Editor/Views/TerminalView.cpp` — `DrawViewContents` (the render path that today *always
  pins to the bottom*, which is the bug).
- `src/Core/VTermParser.{h,cpp}` — escape-sequence → `CMD` parsing (where OSC 133 would land).
- `docs/done/mouse-support.md` — terminal-mouse/scrollback is already listed there as explicitly
  deferred; this is where that thread gets picked up.

---

## 0. Problem & scope

### 0.1 What bug #10 actually is

The terminal model **already keeps a scrollback buffer** (`TerminalScreen::scrollback`, a flat
`std::vector<Row>`). Rows land in it when they scroll off the top of the live grid
(`ScrollRegionUp` → `scrollback.push_back`, guarded by `scrollRegionTop == 0`) and during a shrink
`Resize`. The defect is purely on the **viewing** side: `TerminalView::DrawViewContents` computes

```
totalHistory = scrollback.size() + cursorGridRow;
startIdx     = max(0, totalHistory - (viewHeight - 1));   // ← always the bottom
```

so it only ever shows the **last `viewHeight-1` rows**. There is no scroll offset and no key/gesture to
move the window up. The history is retained but unreachable — `ls` a big directory and the top scrolls
away forever. That is bug #10.

Two latent problems ride along and should be fixed in the same effort:
- **Scrollback is uncapped.** Nothing ever trims `scrollback`; it grows for the life of the process.
- **No semantic structure.** Output is an undifferentiated row stream — there is no notion of "this block
  of rows is the output of *this* command," which is the foundation for everything in §0.2.

### 0.2 What the user wants beyond "let me scroll up"

1. A real scrollback feature — scroll/page through history.
2. **Group `command + output`** so the backlog can be *jumped* per group (prev/next command), the way
   iTerm2 / kitty / WezTerm / VSCode "command blocks" work.
3. The grouping must enable downstream features:
   - take the output of a build and **parse it** into more useful messages (a diagnostics/quickfix list);
   - run a command and **open a specific block's output as a Document** for editing.
4. **Full-screen (alt-screen) content must NOT enter the backlog** — only real commands and their output.
5. Open design question, posed by the user, that this doc must settle: *is a "group" the actual backlog
   (a list of groups), or a meta-structure sitting alongside a flat buffer?* — see §2.

### 0.3 Non-goals (this effort)

- A real build-output parser / diagnostics view. We spec the **seam** (block → text → consumer), not the
  parser. The parser is its own later effort that consumes this API.
- Reflow-on-resize of historical rows (rewrapping long lines when columns change). Out of scope; rows
  keep the width they were captured at, same as today.
- Search within scrollback. Natural follow-up once the flat buffer is addressable; not in this spec.
- Selecting/copying text out of scrollback with the mouse (drag-select) — already deferred in
  `mouse-support.md`; not pulled in here beyond what "open block as document" gives for free.

---

## 1. Current architecture recap (cold-start)

- **`TerminalScreen`** is the model: a live `grid` (`rows × cols` of `Cell{ch,fg,bg,attrs}`) plus a flat
  `scrollback` of completed `Row`s, plus pen/cursor/scroll-region state and a single `isAltScreen` flag.
- **The pty-reader thread** (`Shell::ConsumePty`) calls `TerminalController::HandleTerminalData`, which
  runs bytes through `VTermParser` and mutates the grid **under `screenLock`**. Per CLAUDE.md: *every
  path that mutates the grid must hold `screenLock`*. This is the central threading constraint for
  everything below.
- **Render** (`DrawViewContents`, UI thread, also under `screenLock`) has two paths chosen by the single
  `isAltScreen` flag (the established "one model flag, the view picks the render path" convention):
  - **alt-screen**: blit the whole grid (full-screen apps repaint themselves).
  - **shell mode**: draw `scrollback ++ grid[0..cursorRow)` pinned to the bottom, then compose the prompt
    cells + the locally-edited `inputLine` on the last row.
- **Line ownership** is two orthogonal flags (CLAUDE.md "two orthogonal flags, not a nested state
  machine"): `screen.IsAltScreen()` (model) and `doesShellOwnLineEditing` (controller, set on Tab
  completion). In the default local-edit path **we own the line** and `CommitLine` is the authoritative
  "a command was just run" signal — this is load-bearing for §4.
- **`TerminalHistory`** is **command history** (readline-style up/down recall, capped at `MAX_ENTRIES`).
  It is **not** scrollback. The naming collision is a live hazard, so **rename it `TerminalCmdHistory`**
  (TS-0) — type + the `TerminalController::history`/`historyPath` members (→ `cmdHistory`/`cmdHistoryPath`,
  per the rename-the-variable convention) + the `terminalhistory` test module. Then "history" in this
  feature unambiguously means scrollback.

---

## 2. The core question: list-of-groups vs marks-alongside

The user framed two structures. Restating them precisely:

- **Option A — the backlog *is* a list of groups.** Storage becomes `vector<CommandBlock>`, each block
  *owning* its `command` + `vector<Row>` output + metadata. The rendered row stream is a flattening over
  the blocks.
- **Option B — a group is a meta-structure *alongside* a flat row buffer.** The flat rows stay the single
  source of truth; blocks are an **index** (`{startRow, endRow, command, metadata}`) layered over them.
  Grouping is a *projection*, not the storage.

### 2.1 Decision: **Option B** (marks/spans alongside the flat buffer).

Rationale, in priority order:

1. **The live tail lives in the mutable grid, not in any block.** While a command runs, its output is
   *still being painted into the fixed-size `grid`* by in-place ANSI mutations (`EraseInLine`,
   scroll-region, cursor moves). Rows only become immutable history when they scroll off the top into
   `scrollback`. So a block's output is inherently `scrollback rows + live grid rows`, and the live rows
   are owned by the grid ring, not by the block. Option A would force us to either move rows out of the
   grid into the block as they scroll (fighting the grid's in-place mutation model) or have a block point
   at grid rows it doesn't own (which is just Option B with extra steps). Option B's "the open block has
   a start and no end yet" models the running command *exactly*.

2. **Rendering wants a flat, indexable sequence.** The scroll viewport (§5) is "show rows `[top, top+H)`
   of a virtual sequence," with partial blocks at the top and bottom edges of the viewport. Walking a
   flat array by index is trivial; flattening across block boundaries every frame (or maintaining a
   parallel flattened cache that must stay in sync with A's storage) is not. Option B keeps the render
   loop almost identical to today's.

3. **Boundary semantics are fuzzy; row storage must not depend on getting them right.** We do not always
   know precisely where a command's output ends (a command may spawn sub-shells, emit its own prompts,
   enter and leave alt-screen, or run under readline completion). With Option B, *if we get a boundary
   wrong the rows are still all there and correctly ordered* — only the index is imperfect, and it can be
   re-derived or corrected later. With Option A a missed boundary corrupts the storage itself.

4. **Trimming (the one point that favors A) is solvable in B.** A's clean win is eviction: drop the
   oldest block, done. B trims oldest rows from the flat buffer and must keep block indices valid. The
   standard fix is **absolute (monotonic) row ids** with a moving base (§3) — block ids survive trimming;
   a block whose rows are fully evicted is dropped. This is a well-trodden ring-buffer technique, not a
   novelty.

Net: Option B matches the grid/scrollback reality, keeps rendering simple, degrades gracefully on bad
boundaries, and still delivers 100% of the grouping/jump/parse/extract features (a block resolves to its
rows in O(1)). The grouping is a **view over the rows**, not the rows themselves.

This is the same shape as the established conventions: *model stays logical, the data has one home; the
controller owns the policy of when a boundary opens; the view picks the render path off simple state.*

---

## 3. Data model

### 3.0 Storage: live grid (cells) + scrollback (a `TextBuffer`)

Two stores, by lifetime, **not** one:

- **Live grid — stays `Cell`/`Row`, unchanged.** The grid is a 2D, randomly-addressable cell matrix that
  escape sequences mutate *in place* (cursor addressing, `EraseInLine`, scroll regions, per-cell SGR
  color). A `TextBuffer` (a 1-D ordered line list) cannot model that; the grid keeps its `vector<Row>` of
  `Cell{ch,fg,bg,attrs}`.
- **Scrollback — becomes a `TextBuffer`.** Once a row scrolls off the top of the grid it is immutable
  history. Instead of pushing the `Row` onto a `vector<Row>`, **convert it to a `Line` and `AddLine` it
  into a scrollback `TextBuffer`.**

**The conversion is lossless for color.** `Line::LineAttrib` already carries `ColorRGBA
foregroundColor/backgroundColor` + `kTextAttributes` per span, and `LineRender` paints a line straight
from those (`dc.SetColor(itAttrib->foregroundColor, …)`). A `Row`'s maximal runs of same-`fg/bg/attrs`
cells map 1:1 onto `LineAttrib` spans — *exactly* the run-batching `DrawScreenRow` already does. So a
scrollback `Line` keeps the original ANSI colors, and the history can render through the **existing**
`LineRender` instead of a bespoke path.

Why this is the right primitive (the payoff the user identified):
- **Syntax highlighting and ANSI color are the same attrib spans, two sources.** Row→Line fills them from
  ANSI; a language `Reparse` overwrites them from token classes. So attaching a language to a block
  (e.g. CMake highlighting for a `cmake` run) is "set a language + region-reparse" — see §3.4. The
  `LineAttrib.tokenClass` field (+ its FIXME about an experimental CMake output parser) is the existing
  seam for this.
- **Save / load for free.** `TextBuffer::Save/Load` persist the scrollback **text to its own file** —
  this is what keeps the bulky history out of `session.yml` (§8).
- **Read-only + line ops for free.** `SetReadOnly(true)`, `DeleteLineAt` (front-eviction, §7), `Lines()`.

Caution (threading): the scrollback `TextBuffer` is used as a **passive line store** — no language
attached by default, so no async tokenizer runs. A per-block region-reparse (§3.4) is a *deliberate,
post-hoc* action, never triggered from under `screenLock` on the pty thread.

### 3.1 Absolute line ids (the spine)

Introduce a monotonic 64-bit counter so positions survive scrollback eviction and so blocks/scroll
anchors can name a line stably. (The unit is a *scrollback line* or a *live grid row*; "abs row" below
spans both — scrollback lines first, then grid rows.)

- `uint64_t scrollbackBase` — absolute id of scrollback line 0. Starts at 0; **increments by N when the
  oldest N lines are evicted** (§7).
- abs id of scrollback line `i` = `scrollbackBase + i`.
- abs id of grid history row `g` (`0 ≤ g < cursorGridRow`) = `scrollbackBase + scrollbackLines + g`.
- `absBottom` (the live row) = `scrollbackBase + scrollbackLines + cursorGridRow`. Monotonically
  non-decreasing as output flows.

Helpers on `TerminalScreen`: `uint64_t AbsRowCount() const` (== absBottom), `uint64_t ScrollbackBase()`,
and a resolver `RowAtAbs(uint64_t abs)` returning the scrollback `Line::Ref`, the grid history row, or
empty if evicted/out of range. The single translation point between "absolute id" and "where the line
physically lives."

### 3.2 The block index

```cpp
struct CommandBlock {
    uint64_t              id;            // monotonic, never reused
    std::u32string        command;       // command text (empty if unknown — e.g. OSC-only boundary)
    uint64_t              startAbsRow;    // first output row (abs id)
    std::optional<uint64_t> endAbsRow;    // exclusive end; nullopt while the block is OPEN (running)
    std::optional<int>    exitCode;       // set when known (OSC 133;D or our own probe)
    std::filesystem::path cwd;            // best-effort, when known
    // timestamps optional; add when a consumer needs them
    enum class Source { kCommitLine, kOsc133, kHeuristic } source;
    LanguageBase::Ref     language = nullptr;   // optional per-block highlighter (§3.4); nullptr = ANSI colors
};
```

Stored as `std::deque<CommandBlock> blocks` on `TerminalScreen` (deque so oldest blocks pop cheaply on
eviction). **At most one open block** (the running command) at the tail, `endAbsRow == nullopt`. A block's
output **lines are resolved by abs id** (`[startAbsRow, endAbsRow)` via `RowAtAbs`) — the block is a
*reference into the shared scrollback buffer*, it does **not** own a copy (the user's "use references, not
split"). Eviction is whole-block (§7), so a retained block is always complete.

Why on `TerminalScreen` and not the controller: the block index must be **trimmed in lockstep with the
rows under the same `screenLock`** (§6). The *data + bookkeeping* lives with the rows it indexes; the
*policy* of when a block opens/closes is driven by the controller calling into the model (§4). This keeps
the model logical and the controller in charge of meaning — same split as the rest of the codebase.

Model API (all called under `screenLock`):
- `void BeginCommandBlock(const std::u32string &command, Source src)` — closes any open block at
  `absBottom`, opens a new one starting at `absBottom`.
- `void EndOpenBlock(std::optional<int> exitCode)` — sets `endAbsRow = absBottom`, records exit code.
- `void SetOpenBlockExit(int code)` / `void SetOpenBlockCwd(path)` — late metadata from OSC 133;D.
- `const std::deque<CommandBlock>& Blocks() const`.

### 3.3 What does NOT change

The **live grid** (`grid`, `Cell`/`Row`), alt-screen save/restore, and the ANSI command handlers are all
untouched. What changes: `scrollback` goes from `vector<Row>` to a scrollback `TextBuffer` (lines fed by
the lossless Row→Line conversion, §3.0); we add the abs-id counter, the block deque, eviction (§7), and
the scroll offset (§5).

### 3.4 Per-block language / highlighting (the CMake case) — seam only, deferred

The goal: detect `cmake …` (or `make`, `cargo`, …) and highlight *that block's* lines with the matching
language, **without splitting the buffer**. Two facts shape the seam:
- A block already resolves to a contiguous `[startAbsRow, endAbsRow)` line range in the one shared
  scrollback `TextBuffer`, and carries an optional `language`.
- But `TextBuffer::SetLanguage` + `Reparse` are **buffer-global** and the tokenizer threads state
  line-to-line (`Line::stateDepthAtStart`). So you can't natively highlight lines 10–50 as CMake and
  51–80 as plain in one buffer today.

The reachable design (do **not** build in v1): give the block a language and a **hard-region tokenize**
that runs *that* language over *that* line range and writes the resulting attribs into those lines.
"Hard-region" is the operative word: the normal reparse walks *backwards* from the edit looking for a line
where the tokenizer state stack is at depth 1 (a safe restart point) — for a block we want a **fixed
boundary**: the block's first line IS the reset, never look back past it (the previous block's tokenizer
state must not bleed in). `ReparseRegion(idxStartLine, idxEndLine)` is the starting point but (a) it uses
the buffer's single `language`, so it needs to take an explicit language (or read a line-range→language
map), and (b) **the tokenizer runs on a background thread/`Job`** — so this is *not* a trivial add: the
region parse must target exactly the block's range, write attribs back safely, and not disturb neighbours,
all while the scrollback buffer is otherwise append-only. The append-only/read-only nature of historical
blocks helps (a finished block's lines never change), but the threading is the real cost. This is purely
additive — blocks-as-line-ranges already make it possible — so v1 only has to **not preclude** it: keep
the `language` field and resolve blocks to line ranges. Detect language from the command's argv0 (a small
command→language map), set on block open, hard-region reparse on block close. **Deferred (Phase 4);
recorded so it isn't re-litigated.**

---

## 4. Boundary detection — where does a block start/end?

Two sources, layered. The baseline works with **no shell cooperation**; the upgrade gives precision.

### 4.1 Baseline: our own `CommitLine` (Phase 1, always on)

In the default local-edit path **we** commit the command, so we know exactly when it starts and what its
text is. `TerminalController::CommitLine` already has `cmdLine` in hand right before
`shell.SendCmd`. Add (under `screenLock`):

- on commit of a non-empty, non-plugin command → `screen.BeginCommandBlock(cmdLine, kCommitLine)`.
- the **previous** open block is closed by `BeginCommandBlock` (it sets the prior block's `endAbsRow` to
  the current `absBottom` — i.e. the prompt row that just appeared). Exit code unknown in this mode
  (nullopt) unless we opt into a probe.

Edge cases and how the baseline handles them (gracefully, per §2.1 point 3):
- **Tab-completion / shell-owned line** (`doesShellOwnLineEditing`): the command is still ultimately
  committed through us (`ForwardActionToShell` on commit, or `CommitLine`), so the boundary still opens;
  worst case the command text is read back from the grid via the existing `SyncInputLineFromGrid` path.
- **Plugin / built-in commands** (`ParseAndExecuteWithCmdPrefix` true): these don't run in the shell, so
  there is no pty stream and no shell prompt to bracket them — but they **do** produce terminal output,
  via a different path (§4.3), and that output is just as worth grouping (a `search` block you can jump to
  and open as a document is exactly the kind of thing this feature is for). So we **must not structurally
  exclude them.** Because the row data is retained regardless (Option B, §2.1), the only open question is
  *boundary fidelity*, not data loss: a plugin command's block-close is less crisp than a shell prompt
  (there's no "next prompt" event — the natural close is "when the `WriteLine` burst settles" or the next
  committed line). v1 may leave plugin-command blocks coarser (open on commit, close on the next commit)
  and refine later; the model is ready for it either way. See §4.3.
- **Command that enters alt-screen** (`vim foo`): block opens at commit; alt-screen rows never enter
  scrollback (§10); block closes at the next prompt. Result: a block named `vim foo` with little/no
  captured output. Correct and intended.
- **Output with no command we committed** (shell prints a banner at startup, a background job writes
  later): rows land in scrollback with no owning block — fine. They render normally; jump-nav just skips
  over un-blocked regions.

### 4.2 Upgrade: OSC 133 semantic prompts (Phase 3, optional, opt-in)

The robust industry mechanism is **OSC 133** (FinalTerm/iTerm2 shell integration): the shell's prompt
emits marker escapes —
`OSC 133;A` (prompt start), `;B` (command start), `;C` (output start), `;D;<exit>` (command end). We
**own the shell bootstrap** (`Config terminal.bootstrap`), so we can inject PS1/PROMPT_COMMAND hooks that
emit these for bash/zsh. With OSC 133 we get **exact** boundaries (independent of our line editor) plus
**exit codes** and (via `OSC 7`) **cwd** — which makes the build-parse / open-as-document features far
better (e.g. "open the output of the last *failed* command").

Implementation seam:
- `VTermParser` gains OSC 133 / OSC 7 recognition (it already has `ParseOSC` / `OSC_ParseStringToBel`),
  emitting new `kAnsiCmd` values (`kPromptStart`, `kCommandStart`, `kOutputStart`, `kCommandEnd` with the
  exit param; `kSetCwd`).
- `TerminalController::HandleAnsiCmd` maps `kOutputStart` → `screen.BeginCommandBlock(...)`, `kCommandEnd`
  → `screen.EndOpenBlock(exit)`, `kSetCwd` → `screen.SetOpenBlockCwd(...)`. The command text comes from
  the row span between `;B` and `;C`.

OSC 133 **upgrades** fidelity; it does not change the model. When present it supersedes the `CommitLine`
heuristic (block `Source` records which fired) so the two don't double-open — `BeginCommandBlock` is
idempotent within one prompt cycle (ignore a second open before any output if the abs row hasn't moved).

### 4.3 The other output path: `IOutputConsole::WriteLine` (built-ins / search)

Not all terminal output comes from the pty. The editor injects output directly: `RuntimeConfig` holds an
`IOutputConsole*` and `TerminalController` registers itself as it (`SetOutputConsole(this)`). Anything in
the app calling `OutputConsole()->WriteLine(...)` — **search results today** route here — lands in the
terminal grid through `TerminalController::WriteLine`, which writes chars to `screen` under `screenLock`
(moving to a fresh line first). So this output **already flows into the same grid/scrollback** the pty
output does; it is *already* part of the backlog. The implication for this design:

- The block index sits over the rows, so `WriteLine` output is captured **for free** — it occupies real
  scrollback rows with real abs ids, no special-casing in the storage.
- What's *missing* is the boundary semantics: a `WriteLine` burst has no command line we committed and no
  prompt to close on. To group it (e.g. "a `search` block"), the producer side needs to bracket its
  output — the clean seam is to let an `IOutputConsole` producer **open/close a block around its burst**
  (a `WriteBlock(title, lines)` convenience, or explicit `BeginCommandBlock`/`EndOpenBlock` calls), with
  `Source::kHeuristic`. This is the "additional syncing" the user flagged. **Deliberately deferred** —
  v1 retains the rows (so nothing is lost) and we wire producer-side bracketing when the first consumer
  (search-results-as-a-block) actually needs it. Recorded here so it isn't re-discovered cold.

---

## 5. Rendering & the scroll viewport

### 5.1 Viewport state (controller/view, not model)

Scroll position is a *viewport* concern → lives on `TerminalController` (where key/mouse handling is),
not on `TerminalScreen`:

- `bool followBottom = true` — true = live/pinned to bottom (today's behaviour).
- `uint64_t anchorAbsRow` — when `followBottom == false`, the abs id of the **top visible history row**.

**Anchor to an absolute row, not to an offset-from-bottom.** If we measured "N rows up from the bottom,"
new output streaming in (a running build) would push the content you're reading up and out of the
viewport. Anchoring the top-visible row to an absolute id keeps what you're reading **stationary** while
output appends below — the correct UX for watching a long build while scrolled up.

### 5.2 Render path (shell mode)

Define the virtual history sequence `H = scrollback ++ grid[0..cursorGridRow)`, length
`totalHistory = scrollback.size() + cursorGridRow`, with abs ids per §3.1.

- `followBottom == true` (default): unchanged from today — pin to the bottom, last row composes
  prompt + `inputLine`.
- `followBottom == false`: compute the window-top index from `anchorAbsRow` (clamp into
  `[scrollbackBase, absBottom]`), render `viewHeight` rows of `H` (no input composite — you're not at the
  prompt), and show a subtle "scrolled / N below" affordance (e.g. a marker on the status splitter row,
  or dimmed bottom border). When scrolled past the newest content the view snaps back to `followBottom`.

Alt-screen path: **unchanged and never scrollable** — `IsAltScreen()` short-circuits before any of this
(§10).

### 5.3 Gestures / keybindings

- **Mouse wheel** is the primary gesture. `TerminalView` overrides `OnMouseEvent` (it currently does
  not), handling `kMouseEventKind_Wheel` by moving `anchorAbsRow` by
  `wheelDelta * lines_per_scroll_wheel_notch` — mirroring `WorkspaceView` exactly (same config key,
  `commandview.lines_per_scroll_wheel_notch`). Wheel-up off `followBottom` enters scroll mode; wheeling
  back to the bottom restores `followBottom`.
- **Keyboard**: plain `PageUp`/`PageDown` — **reuse the existing `kUIActionPageUp/Down`**, no new action.
  "Page up" is one intent — *page the focused view up* — and each view implements it for its own content
  (the editor pages the document, the workspace the tree, the terminal its **backlog**). There is no
  competing "page the cursor" in the terminal: the prompt is a single line, so the only thing to page is
  the history. Concretely:
  - **alt-screen / shell-owned** (`vim`/`less`/completion): `TerminalView::OnAction` already routes to
    `ForwardActionToShell`, which maps `kUIActionPageUp → \x1b[5~` (the app pages itself). **Unchanged.**
  - **shell prompt (local edit)**: `controller.OnAction` returns `false` for PageUp today (does nothing);
    add a case there to move `anchorAbsRow` by a page and enter scroll mode. This is the one new handler.
  - `Home`/`End` (or `Ctrl+Home/End`) for top/bottom of the backlog, same routing.

  Why **not** a dedicated `kUIActionScroll*`: real terminals reserve **Shift+PageUp** for emulator
  scrollback and send plain PageUp to the program — but only because they can't tell whether the
  foreground program wants the key. We *can* (alt-screen vs prompt), so we don't need the modified-key
  split; plain PageUp is unambiguous in each mode. Bindings live in `Assets/Resources/*/terminal_keymap.yml`
  — and because the action is decoupled from the key, the keymap can bind **both** `PageUp` **and**
  `Shift+PageUp` to the same `kUIActionPageUp`, so the intuitive key works *and* xterm/iTerm muscle memory
  is preserved, at no extra code cost.
- **Snap-to-bottom triggers** (set `followBottom = true`): committing a line (`CommitLine`), and any
  local text input into `inputLine`. New output **does not** snap (so you can read while a build runs).
  This is the standard, expected terminal behaviour.

### 5.4 Jump-per-block (the "jump the backlog per group")

New `kUIAction`s `kUIActionPrevPrompt` / `kUIActionNextPrompt`. Handler walks `screen.Blocks()` relative
to the current top-visible abs row and sets `anchorAbsRow = block.startAbsRow` (clamping, leaving
`followBottom = false`). This is the iTerm2 ⌘↑/⌘↓ "jump to previous/next command" behaviour. Because
blocks are an index over the same abs-id space the viewport already uses, this is a couple of lines.

---

## 6. Threading / locking (the landmine)

Per CLAUDE.md, **every grid mutation holds `screenLock`**, and the *dimension fields must track the
buffer*. The block index and abs-id counter are part of the model's mutable state, so:

- `scrollbackBase` increment + `scrollback` trim + `blocks` eviction happen **together, atomically, under
  `screenLock`** (they're one consistent operation — see §7). The pty thread already holds the lock when
  it appends to scrollback via `ScrollRegionUp`; trimming is invoked from the same locked region.
- `BeginCommandBlock` / `EndOpenBlock` are called from the controller (UI thread) in `CommitLine` /
  `HandleAnsiCmd`. `HandleAnsiCmd` already runs under `screenLock` (held by `HandleTerminalData`).
  `CommitLine` must take `screenLock` around the `BeginCommandBlock` call (it currently does not lock —
  it only calls `shell.SendCmd`; add a short locked section, mirroring `WriteLine`).
- The viewport state (`followBottom`, `anchorAbsRow`) is read/written **only on the UI thread** (key/
  mouse handlers + `DrawViewContents`), so it needs no lock of its own — but any read of `Blocks()` /
  `AbsRowCount()` for nav happens under `screenLock` since the pty thread mutates them.

This is exactly the existing discipline; we are adding state into the already-locked region, not a new
lock.

---

## 7. Memory / capping — evict whole blocks

Today `scrollback` is uncapped (latent leak). Introduce `terminal.scrollback_lines` (config, default e.g.
10000; `0` = unlimited for power users who accept the cost).

**Eviction is block-granular, not line-granular** (user's call, and the right one): when the line count
exceeds the cap, drop the **oldest whole block** — its lines and its `CommandBlock` together — and repeat
until back under the cap. This guarantees the invariant **"a retained block is complete"**, which the
downstream consumers (open-as-document, parse-build) rely on — you never get half a build log. A
line-granular trim would silently behead the oldest surviving block.

For this to be total, **every scrollback line belongs to some block.** Output with no committed command
(the startup banner, a background job's writes, `IOutputConsole` bursts before any command) goes into an
implicit **loose block** (`Source::kHeuristic`, empty `command`) that closes when the next real block
opens. So eviction is always "drop the oldest block," never "orphan lines."

Procedure (under `screenLock`, atomic):
1. While `scrollbackLines > cap` and `blocks.size() > 1`: take the front block, `DeleteLineAt` its line
   span from the front of the scrollback `TextBuffer`, `scrollbackBase += (its line count)`,
   `blocks.pop_front()`.
2. Never evict the **open** (tail) block — a running command's output stays whole.

Consequences, called out honestly:
- **The cap is approximate** — actual retained lines ≤ `cap + (youngest-kept block's size)`. Fine.
- **Pathological single giant block** (one block alone exceeds the cap, e.g. a 50k-line build with no
  intermediate prompt): block-granular eviction can't help. Guard: a per-block hard line ceiling
  (`terminal.max_block_lines`) above which the *open* block trims its own oldest lines line-granularly
  (the one place we accept an incomplete block — and it's the live one, not a finished result).

Abs ids are 64-bit — no practical wraparound. (Alt-screen's `savedScrollback` snapshot is bounded by
definition; eviction applies to the *active* scrollback only.)

---

## 8. Persistence & restore (session cache)

The user wants the backlog to **survive a restart**. There is already a home for this: the per-root
session cache (`docs/done/session-cache.md`). `SessionManager` is the *sole* owner of session disk I/O,
writes `<root>/.goatedit/session.yml`, and the established pattern is **a plain-data DTO in
`SessionState.h` + a `ToSession`/`FromSession` owned by the subsystem** (each subsystem serialises its own
struct; documents, layout, geometry, tree expand/collapse already do exactly this). Terminal scrollback
slots into that pattern as one more DTO on `RootSession` — **no new file, no new I/O owner.**

### 8.1 Split storage: a `.bin` (text + attributes) file, only block info in `session.yml`

Reusing a `TextBuffer` for scrollback (§3.0) pays off here: the history **saves itself** to a file. So we
**don't put the rows in `session.yml`** (user's explicit ask). Two artifacts under `<root>/.goatedit/`:
- **`terminal_scrollback.bin`** — the scrollback **text *and* per-span attributes**, written by a new
  **`TextBuffer::SaveWithAttributes()`** / read by `LoadWithAttributes()`, capped to the last
  `terminal.persist_scrollback_lines` (default e.g. 2000, smaller than the in-memory cap). Binary, its own
  file, not parsed on every session read.
- **`session.yml`** — gains only a compact **`TerminalSession`** holding the **block index** (metadata)
  plus a pointer to the bin file:

```cpp
// SessionState.h — plain DTO, same style as DocumentSession; NO row text/attribs here
struct TerminalBlockSession {
    uint64_t    id;
    std::string command;     // utf-8
    uint64_t    startLine;   // line index into terminal_scrollback.bin (re-based to the saved slice, §8.2)
    uint64_t    endLine;     // exclusive
    int         exitCode;    // sentinel if unknown
    std::string cwd;
    int         source;
    std::string language;    // optional per-block language id (§3.4), empty = none
};
struct TerminalSession {
    std::string scrollbackFile = "terminal_scrollback.bin";  // relative to .goatedit/
    std::vector<TerminalBlockSession> blocks;
};
// added to RootSession: TerminalSession terminal = {};
```

Decisions baked in here:
- **Preserve the actual colors, don't re-derive them (user's call).** Every shell tool has its own
  coloring; writing a syntax highlighter for each tool's output just to recolor restored scrollback is not
  worth it. So persist the **attributes** alongside the text via `SaveWithAttributes()` — the funky ANSI
  colors come back verbatim on restore, no language needed. This *supersedes* the earlier "text-only,
  recolor via syntax" idea. (Per-block syntax highlighting, §3.4/Phase 4, stays an independent *optional*
  nicety, not a prerequisite for colored restore.) `.bin`, not `.txt`, precisely because it carries more
  than text.
- **New `TextBuffer` capability — `SaveWithAttributes()` / `LoadWithAttributes()`.** Serialise each line
  as `u32 text + its LineAttrib spans (idxOrigString, fg RGBA, bg RGBA, textAttributes)`. Versioned binary
  header; the existing text `Save/Load` stay untouched. This is the bulk of the persistence phase's new
  code and is why it lands **last** (below).
- **Block line indices re-based to the saved slice.** Only the last N lines are saved, so on save the
  block `startLine`/`endLine` are rewritten into `[0, savedLineCount)` and blocks entirely older than the
  slice are dropped. On load, the live model seeds `scrollbackBase = 0` and continues. Keeps the on-disk
  format independent of how long the process ran.

### 8.2 Restore semantics

On `OnFolderOpened` → restore: `TextBuffer::LoadWithAttributes(terminal_scrollback.bin)` seeds the
scrollback buffer (read-only) **with its colors intact**, `FromSession` rebuilds `blocks` from
`TerminalSession`, then the **live shell starts fresh on top** with a new prompt. The restored region is
pure, immutable history — scroll/jump/open-as-document work over it exactly like freshly-produced
scrollback; only the *live grid* is new. A restored block's output is fully resolvable (all its lines are
in the loaded buffer, none in the not-yet-existing live grid).

### 8.3 Constraints / gotchas

- **Save off the main thread is forbidden** — `SessionManager`'s autosave handler is posted to the main
  runloop. The scrollback `TextBuffer` snapshot/`SaveWithAttributes` reads lines that the pty thread
  appends to, so the save path coordinates with `screenLock` (snapshot the slice under the lock, write the
  file outside it); reuse `TextBuffer`'s own save path rather than hand-rolling.
- **Never persist while `isAltScreen`** — snapshot the *shell* scrollback, which by §10 already excludes
  alt-screen content. If a full-screen app is up at save time, persist the saved (pre-alt) scrollback.
- **Two caps, two files.** `terminal.persist_scrollback_lines` (on-disk) is independent of and smaller
  than the in-memory `terminal.scrollback_lines` (§7). `RootSession::kVersion` already guards format
  changes (unknown/newer → start clean), so adding `terminal` is backward-safe. A missing/corrupt
  scrollback file → start with empty scrollback, never crash.
- This is the **last phase** (TS-5) — depends only on the line+block model (Phase 0/1), but is scheduled
  last because the `SaveWithAttributes`/`LoadWithAttributes` binary format is net-new `TextBuffer` code
  and the rest of the feature works without it (it's restart-fidelity, not core function). Per the user:
  "add that once everything works."

---

## 9. Downstream consumers (built ON the block API)

These are the payoff and the reason for the grouping. Each is a thin consumer of one primitive:

```cpp
// TerminalController / TerminalScreen
std::vector<std::u32string> GetBlockOutputText(uint64_t blockId) const;  // resolves scrollback + live grid tail
```

- **Open a block's output as a Document.** New action / JS call: take `GetBlockOutputText(id)`, create a
  Document from it (reuse the `EditorAPI::NewDocument` template — CLAUDE.md "JS API layer pattern"), open
  it in a buffer. Canonical first feature: "send last command output to a new buffer."
- **Parse build output → messages.** A future parser consumes `GetBlockOutputText(lastBuildBlockId)` →
  `(file, line, severity, message)` diagnostics that feed a messages/quickfix view. We spec only the
  **seam** (block → text → parser → list); the parser is its own effort. OSC 133 exit codes make
  "the last *failed* command" selectable, which is what makes this genuinely useful.
- **JS exposure.** New `TerminalAPI` + `TerminalAPIWrapper` (follow the established pattern:
  add to the API, mirror in the wrapper as `Wrapper::Ref`, `dukglue_register_method` in `RegisterModule`,
  update the `.js`): `getBlocks()`, `getBlockOutput(id)`, `openBlockAsDocument(id)`, `getLastBlock()`.
  This makes build-parse and output-extraction **scriptable**, matching the editor's plugin direction.

The key architectural point: **every one of these is a read over the block index.** None of them needs
the storage to *be* groups (Option A) — they need a block to *resolve to* its rows, which Option B gives.

---

## 10. Alt-screen exclusion (explicit — the user's hard requirement)

"Do NOT add full-screen mode to the backlog." This is **already true** of the row data and must stay
true of blocks:

- `SaveScreen()` (enter alt-screen) clears `scrollback` into `savedScrollback` and starts a clean grid;
  `RestoreScreen()` restores them. So alt-screen output never reaches the persistent scrollback — it is
  discarded on exit. Unchanged.
- **No block opens or closes while `isAltScreen`.** `BeginCommandBlock`/`EndOpenBlock` are no-ops (or
  simply never called) in alt-screen. The block for the *command that launched* the full-screen app
  (opened at its commit) stays open across the alt-screen session and closes at the next shell prompt —
  capturing only the shell-level rows, not the app's screen.
- **Scroll is disabled in alt-screen.** The render short-circuits on `IsAltScreen()` before the viewport
  logic; the app owns the screen (and brings its own scrollback, e.g. `less`).

Test asserts this as a property: rows printed while `isAltScreen` never appear in `Blocks()` output and
`scrollback` is unchanged across a save/restore cycle.

---

## 11. Phasing / work items

Sized so each phase ships independently and is separately testable.

**Phase 0 — scrollback storage + viewing (closes #10 on its own). ✅ DONE**
- `TS-0a` ✅ Rename `TerminalHistory` → `TerminalCmdHistory` (§1) — separate, do first so "history" is
  unambiguous.
- `TS-0b` ✅ Scrollback → a `TextBuffer`: lossless `Row→Line` conversion at the scroll-off point (§3.0);
  `SetReadOnly(true)`; no language attached (attribs = ANSI runs). Abs-line spine (`scrollbackBase`,
  `AbsRowCount`, `RowAtAbs`).
- `TS-0c` ✅ Cap + **block-granular eviction** (§7) — needs the loose-block from TS-1a to be total, so the
  cap can land here line-granularly first and upgrade to block-granular once blocks exist (or sequence
  TS-1a before the cap). Shipped line-granular in TS-0c, upgraded to block-granular in TS-1a.
- `TS-0d` ✅ Viewport state on the controller (`followBottom`, `anchorAbsRow`) + snap-to-bottom triggers.
- `TS-0e` ✅ Render: scrolled path in `DrawViewContents` (history via `LineRender`, grid via
  `DrawScreenRow`) + "scrolled" affordance.
- `TS-0f` ✅ Gestures: `TerminalView::OnMouseEvent` wheel (mirrors `WorkspaceView`); reuses
  `kUIActionPageUp/Down` + `NavigateHome`/`NavigateEnd` (Ctrl+Home/End) in the shell-prompt path; bound in
  `terminal_keymap.yml`.
- Tests: Row→Line color round-trips; anchor stays stationary as output appends; snap-to-bottom on commit/
  input; cap holds and abs ids stay valid. All in `test_terminalscreen`/`test_terminalcontroller`,
  verified-green.
- Found-along-the-way: [`open-bugs.md`](open-bugs.md) #11 (raw TAB byte from the pty silently dropped,
  garbling multi-column shell output) — pre-existing, unrelated to this feature, documented not fixed.

**Phase 1 — command blocks via `CommitLine` (the grouping). ✅ DONE**
- `TS-1a` ✅ `CommandBlock` (line-range ref into the scrollback buffer) + `blocks` deque + Begin/End API;
  loose-block for un-commanded output; block-granular eviction in lockstep (§7).
- `TS-1b` ✅ Open/close from `CommitLine` (+ `screenLock` there) and from the tab-completion / shell-owned-
  line commit path (`ForwardActionToShell`'s `kUIActionCommitLine` case — the §4.1 edge case); alt-screen
  guard (no-op inside `BeginCommandBlock`/`EndOpenBlock`, §10). Plugin/built-in commands open a (coarser)
  block too — their output arrives via the `IOutputConsole::WriteLine` path (§4.3), not the pty; v1 closes
  them on the next commit, producer-side bracketing deferred.
- `TS-1c` ✅ `kUIActionPrevPrompt`/`NextPrompt` (Alt+Up/Down in `terminal_keymap.yml`) jump the anchor to
  the previous/next block's `startAbsRow`, clamped at the oldest/newest block.
- Tests: a committed command opens exactly one block; the next command closes the prior; eviction drops
  whole blocks (a retained block is always complete); alt-screen rows never create blocks; nav lands on
  block starts. All in `test_terminalscreen`/`test_terminalcontroller`, verified-green.

**Phase 2 — downstream consumers.**
- `TS-2a` `GetBlockOutputText(id)` (resolve scrollback + live grid tail).
- `TS-2b` "Open last command output as a Document" action.
- `TS-2c` `TerminalAPI` JS surface (getBlocks/getBlockOutput/openBlockAsDocument).
- Tests: output text round-trips for a closed block and the open (live) block; open-as-document produces
  the right line set.

**Phase 3 — OSC 133 shell integration (optional, opt-in fidelity upgrade).**
- `TS-3a` VTermParser OSC 133 / OSC 7 → new `kAnsiCmd`s.
- `TS-3b` Controller maps them to Begin/End/exit/cwd; `Source::kOsc133` supersedes the heuristic.
- `TS-3c` bash/zsh bootstrap snippets emitting the markers (`terminal.bootstrap`), behind a config flag.
- Tests: a scripted OSC 133 A/B/C/D stream produces a block with the right command text + exit code; cwd
  via OSC 7; no double-open when both sources fire.

**Phase 4 — per-block language / highlighting (§3.4).** *(optional, later; depends on Phase 1.)*
- `TS-4a` command→language map (argv0 → `LanguageBase`); set `block.language` on open.
- `TS-4b` **hard-region tokenize**: run a block's language over its line range, writing attribs — new
  `TextBuffer` capability (region reparse with an *explicit* language + a *hard* start boundary, no
  look-back past the block). Non-trivial: the tokenizer runs on a **background `Job`** (§3.4).
- Tests: a `cmake` block's lines get CMake token attribs; an un-languaged block keeps its ANSI attribs;
  hard-region reparse of block N doesn't disturb block N±1 (no state bleed across the boundary).

**Phase 5 — persistence & restore (§8) — LAST (user: "add once everything works").**
- `TS-5a` New `TextBuffer::SaveWithAttributes()` / `LoadWithAttributes()` — versioned binary (text +
  per-span fg/bg/attrs); existing text `Save/Load` untouched. The bulk of the new code.
- `TS-5b` `TerminalSession` DTO (block index + `.bin` file pointer, **no row text**) on `RootSession`;
  `SessionSerializer` to/from YAML; block line indices re-based to the saved slice.
- `TS-5c` Terminal `ToSession`/`FromSession` (snapshot under `screenLock`, cap to
  `terminal.persist_scrollback_lines`, never during alt-screen); on restore `LoadWithAttributes` the
  `.bin` (read-only, colors intact) + rebuild blocks, fresh shell on top.
- Tests: `.bin` round-trip preserves text **and** colors; block index round-trip (command/exit/cwd, re-
  based indices valid); restored block output + color resolves; alt-screen never persisted; missing/
  corrupt `.bin` → empty scrollback, no crash.

---

## 12. Decisions to confirm

1. **Scroll keybindings — settled (§5.3).** Plain `PageUp`/`PageDown` reuse the existing
   `kUIActionPageUp/Down` and page the focused view (terminal = its backlog); no new action, no
   modified-key split (we distinguish alt-screen-forwards-to-app from prompt-scrolls-backlog, so the
   xterm Shift+PageUp convention isn't needed). The only code change is a PageUp/Down handler in the
   shell-prompt path. Left here only as a record of the resolved choice.
2. **Default scrollback cap.** `terminal.scrollback_lines` (in-memory) default — 10000 proposed
   (`0` = unlimited). Separate, smaller `terminal.persist_scrollback_lines` (on-disk, §8) — 2000 proposed.
3. **OSC 133 on by default?** Recommend **off by default** in Phase 3 (opt-in via config), since it
   modifies the user's prompt; the `CommitLine` baseline already delivers blocks without it.
4. **Block visual affordance.** Do we draw a separator/gutter mark at block boundaries in scroll mode, or
   keep it invisible (nav-only) for v1? (Recommend: invisible for v1; add a subtle separator later.)
5. **Color fidelity across restart — settled (§8.1, user's call).** Preserve the actual colors: persist
   the per-span attributes in a binary `.bin` via new `TextBuffer::SaveWithAttributes()`/`LoadWithAttributes()`,
   so restored scrollback comes back with its real ANSI colors — *not* re-derived from syntax (we won't
   write a highlighter per tool). Scheduled **last** (Phase 5). Per-block syntax (§3.4) stays independent
   and optional.
6. **Per-block highlighting storage — settled (§3.4).** Blocks are line-range **references** into one
   shared scrollback `TextBuffer` (not split per-block); per-block language is an additive region-reparse,
   deferred to Phase 4. Recorded as the chosen direction.

## 13. Test plan (module `terminalscreen` + a new `terminalblocks` module)

- **Row→Line conversion**: an ANSI-colored row converts to a `Line` whose attribs reproduce the same
  color runs (lossless); `DrawScreenRow` and `LineRender` of the same content match.
- **Abs-line spine**: ids monotonic; `RowAtAbs` resolves scrollback line / grid row / empty; survive
  eviction.
- **Eviction (block-granular)**: cap enforced; whole blocks dropped (a retained block is always
  complete); `scrollbackBase` bump matches lines removed; open block never evicted.
- **Viewport** (controller-level, no GUI): anchor stays stationary as output appends; clamps at top/
  bottom; snap-to-bottom on commit and on local input.
- **Blocks**: one block per committed command; prior closes on next; alt-screen rows excluded;
  prev/next nav lands on starts; `GetBlockOutputText` round-trips closed + open blocks.
- **`IOutputConsole` output** (§4.3): a `WriteLine` burst lands in scrollback with valid abs ids (rows
  retained); when producer-side bracketing exists, it forms a navigable block.
- **Alt-screen property**: rows printed under `isAltScreen` never enter `scrollback`/`blocks`; the alt-
  screen save/restore leaves persistent scrollback unchanged.
- **Persistence** (Phase 5): `.bin` round-trips text **and** per-span colors (`SaveWithAttributes` →
  `LoadWithAttributes`); `TerminalSession` block index round-trips through YAML (command/exit/cwd, re-based
  indices valid); restored block output + color resolves; nothing persisted while alt-screen active.
- **OSC 133** (Phase 3): scripted A/B/C/D byte stream → correct command/exit/cwd; no double-open.

All assert **properties** (CLAUDE.md), not magic row numbers where avoidable.
