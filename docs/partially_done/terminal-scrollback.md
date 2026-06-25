# Terminal scrollback + command blocks — design / spec

Status: **Phase 0 (scroll viewport), Phase 1 (command blocks), Phase 1.5a/b (block separator rule +
selected-block highlight), and Phase 2 DONE ✅ — TS-0a..TS-0f, TS-1a..TS-1c, TS-1.5a/b, plus TS-2a
(`GetBlockOutputText`), TS-2b/2c (the dedicated `TerminalAPI`/`TerminalAPIWrapper` JS surface —
`Terminal.GetBlocks/GetBlockOutput/GetLastBlock/GetSelectedBlock/OpenBlockAsDocument`), and the
`Editor.NewDocumentFromText()` seam + the `blocktobuffer` (`b2b`) cmdlet all shipped, in the
verified-green test suite.** TS-1.5c (acting on a block) was redirected onto quick-command + JS and is
now delivered through that JS surface (§5.5.3). The clipboard JS seam ("copy block to paste buffer") is
also shipped (`Editor.CopyToClipboard` + `Terminal.CopyBlockToClipboard` + the `b2c` cmdlet). **Phase 3
(OSC 133 shell integration) DONE ✅ — TS-3a/3b/3c (parser markers + controller→block mapping with the
`useOsc133Boundaries` supersession + opt-in `terminal.shell_integration` bootstrap), in the verified-green
suite.** Remaining: Phases 4-5 (per-block highlighting, persistence) — see §11. Resolves
[`open-bugs.md`](open-bugs.md) #10 ("Missing scrollback feature in terminal UI") for the viewing/scrolling
half; the *grouping* infrastructure (jump-per-command, parse-build-output, open-output-as-document) the
user wants on top of it is built (blocks exist + nav works) but not yet consumed downstream. Written
cold-start so remaining phases can be picked up later without re-deriving the terminal model.

> **▶ DONE this session (Phase 2 JS-consumer slice, §9 + §5.5.3):**
> 1. **TS-2a ✅** — `TerminalScreen::GetBlockOutputText(blockId)` resolves a block's `[startAbsRow,
>    endAbsRow)` to text via `RowAtAbs` (scrollback `Line`s + live grid tail; open block runs to the live
>    row). `TerminalController::GetSelectedBlockText()` joins the selected block with `\n` under
>    `screenLock`. Tests: `test_terminalscreen_block_output_text` (closed + open, straddling both stores),
>    `test_terminalcontroller_selected_block_text`.
> 2. **`Console.GetSelectedBlock()` ✅** — new `IOutputConsole::GetSelectedBlockText()` virtual (default
>    `nullopt`), overridden by `TerminalController`; `ConsoleAPIWrapper::GetSelectedBlock` returns the
>    text or JS `null` when nothing is selected (following bottom / no console).
> 3. **`Editor.NewDocumentFromText(name, text) ✅`** — `EditorAPI` + `EditorAPIWrapper`; splits on `\n`,
>    drops the fresh buffer's placeholder empty line (guarded on `IsEmpty()`), `Reparse()`. Canonical
>    cmdlet shipped as the **`blocktobuffer`** (`b2b`) plugin: `var t = Console.GetSelectedBlock();
>    Editor.NewDocumentFromText("output", t);`. End-to-end test `test_jsengine_newdocumentfromtext`.
> 4. **Selection survives focus loss ✅** — confirmed it holds *by construction* (only `ScrollToBottom`,
>    wired to commit + local edit, clears the viewport; output streaming / focus switch never does).
>    Locked with `test_terminalcontroller_selection_survives_output`.
>
> **▶ DONE this session (TS-2b/2c, the dedicated TerminalAPI):** the implicit terminal API (which lived
> as ad-hoc `Console.GetSelectedBlock` glue poking `RuntimeConfig::OutputConsole()` directly) is now a
> proper TWO-LAYER API matching `EditorAPI`/`EditorAPIWrapper`:
> - **`IOutputConsole` seam extended** (RuntimeConfig) with a plain-data `OutputBlockInfo` +
>   `GetBlocks()`/`GetBlockOutputText(id)`/`GetLastBlock()` (virtual defaults, so `CommandView`/test
>   consoles compile unchanged); `TerminalController` overrides them under `screenLock`.
> - **`TerminalAPI`** (`src/Core/API/`, engine-agnostic logic) — `GetBlocks/GetBlockOutput/GetLastBlock/
>   GetSelectedBlock` + the composite `OpenBlockAsDocument(id)` (block text → `EditorAPI::NewDocumentFromText`).
>   Consumes the terminal ONLY through `IOutputConsole`; registered as a global API object in `Editor`.
> - **`TerminalAPIWrapper`** (`src/Core/JSEngine/Modules/`, thin duktape glue) — the JS `Terminal` global;
>   marshalling only. `Console.GetSelectedBlock` was removed (block is a terminal concept, not console
>   output) and the `b2b` cmdlet now calls `Terminal.GetSelectedBlock()`.
> - Tests: `test_terminalcontroller_block_index_surface` (seam mirrors the model by id; unknown id →
>   nullopt) + `test_jsengine_terminalapi` (real controller as the console → JS enumerates blocks by id →
>   `OpenBlockAsDocument` → new doc content). 290 verified-green.
>
> **▶ ALSO DONE this session (clipboard JS seam):** `ClipBoard::CopyText(u32)` — the literal-text →
> paste-buffer primitive that (unlike `CopyFromExternal`) fires the OnUpdate hook so it reaches the OS
> pasteboard (E.18). Exposed as `Editor.CopyToClipboard(text)` (general primitive, EditorAPI) +
> `Terminal.CopyBlockToClipboard(id)` (composite, TerminalAPI) + the `blocktoclipboard`/`b2c` cmdlet
> (selected-block path, parallel to `b2b`). Tests: `test_clipboard_copytext_notifies` (the OnUpdate
> requirement), `test_jsengine_copytoclipboard` (both JS surfaces → paste-buffer top item).
>
> **▶ DONE this session (Phase 3, OSC 133 shell integration, §4.2 + §11):** `VTermParser` now recognises
> OSC 133 (`A/B/C/D` → `kPromptStart`/`kCommandStart`/`kOutputStart`/`kCommandEnd` with the exit param)
> and OSC 7 (`kSetCwd`, path in the new `CMD::strParam`), via a no-leading-skip payload reader that
> honours BEL **and** ST. `TerminalController::HandleAnsiCmd` maps them onto the block API — `;C` opens a
> `kOsc133` block whose command text is read from the `;B..;C` grid span (`ReadGridText`), `;D` closes it
> with the exit code, OSC 7 sets the open block's cwd. A `useOsc133Boundaries` flag stands the
> `CommitLine`/tab-completion heuristic down once any marker is seen, so the two never double-open. Opt-in
> via `terminal.shell_integration` (default `no`), which appends shell-aware (bash/zsh) prompt-hook
> bootstrap. `HandleTerminalData` made public so tests can script a byte stream without a live shell.
> Tests: `test_vtermparser_osc133`/`_osc7_cwd`, `test_terminalcontroller_osc133_block`/`_osc7_cwd`/
> `_osc133_no_double_open`.
>
> **▶ NEXT SESSION — pick up here.** Remaining beyond Phase 3:
> - **Phase 5** (persistence, §8) — **DONE ✅** — TS-5a (`TextBuffer::SaveWithAttributes`/`LoadWithAttributes`,
>   versioned `GTSB` binary), TS-5b (`TerminalSession` DTO + YAML serializer on `RootSession`), TS-5c
>   (`TerminalScreen::SnapshotScrollbackTail`/`SeedScrollback` seams + `TerminalController::ToSession`/
>   `FromSession`, wired into `Editor::SaveSession` and restored inside `Begin()` before `shell.Begin()`).
>   TS-5d (cmd-history) was already shipped. Clean-exit-only save, open block closed at the saved tail, on by
>   default (`terminal.persist_scrollback`), caps 2000 on-disk / 10000 in-memory, gated on a `.goatedit` dir.
>   See §11 / §12.7–8. **Verification gap:** the seams + `.bin` round-trip are unit-tested green, but the
>   end-to-end save→restart→restore path has NOT been exercised in a live GUI session yet.
> - **Phase 4** (per-block language / hard-region highlighting, §3.4) — **postponed**; independent of Phase 5.
>
> Already in place to build on: the `Terminal` JS surface (`TerminalAPI`/`TerminalAPIWrapper`),
> `TerminalController::SelectedBlockIndex()` / `GetSelectedBlockText()`, the block index
> (`TerminalScreen::Blocks()`, `GetBlockOutputText`, `RowAtAbs`, abs-id spine), and the
> `Terminal`/`Editor`/`Document` JS wrappers. The local `commandview.show_block_markers` is currently
> flipped to `true` for testing (config is in flux; not a committed default decision).

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

### 5.5 Block visual affordances (Phase 1.5)

Once blocks exist and nav works (Phase 1, done), make them *visible* and *actionable*. Two visual
decorations (5.5.1 separator, 5.5.2 highlight) gated by `commandview.show_block_markers` (yes/no, default
**no** for v1), plus 5.5.3 — **acting** on the selected block, which after TS-1.5b testing moved off a
bespoke HUD onto quick-command-mode + the JS surface (folds into Phase 2). The two decorations reuse
rendering primitives the backend already has — no new graphics machinery.

**The primitives already exist (grounded in source).** `SDLDrawContext` (both SDL2 and SDL3) carries
`DrawLine` / `DrawLineWithPixelOffset` (`SDL_RenderLine` + the cell→pixel `CoordsToScreen` transform),
today `protected` and used to render the **underline** text attribute —
`DrawStringWithAttributesAt` draws a sub-cell rule via
`DrawLineWithPixelOffset(x, y, x+len, y, 0, baseline+margin)`. A block separator is *the same call* with
a top-of-cell offset. The highlight is the existing `DrawContext::Overlay` (buffer-coord span +
`isActive`, painted per row by `DrawLineOverlays(y)`) — exactly what `EditorView::DrawSearchResultOverlays`
builds for search hits. So Phase 1.5 exposes one primitive and reuses the overlay; nothing net-new in the
graphics layer.

**5.5.1 Separator rule at block boundaries (permanent, sub-cell, no row cost).**
In the render loop (`TerminalView::DrawViewContents`'s `drawAbsRow`) the abs id of each drawn row is
already known; a boundary is "this abs row == some block's `endAbsRow`" (lookup against `screen.Blocks()`,
O(blocks)/frame). Draw a thin rule *between* this row and the next. Purely additive — **no change to the
§5 viewport math** (anchor/page/snap), because a sub-cell rule consumes no view row.

Expose it as an **intention-revealing** primitive on the `DrawContext` base — `DrawHRule(int y)`
("separator below row y"), **not** a raw "draw pixel line". Reason — and this is the load-bearing design
decision: the planned modern-terminal (cell-grid, SSH-capable) backend **cannot draw a sub-cell line**; an
intention-named seam lets *it* fall back to a box-drawing-char row (`U+2500 ─` via the existing
`FillLine`) or a gutter marker — its choice — without `TerminalView` knowing or caring. Implement in
**both SDL2 and SDL3** (CLAUDE.md keep-backends-in-sync); no-op/fallback elsewhere (NCurses out of build).
Backend-specific caveat, recorded honestly: in a *pixel* backend the rule is sub-cell and free; in a
*cell* backend a full-width rule must occupy a row (or degrade to a gutter glyph) — that backend's
implementer picks the trade-off **then**; it does not block v1.

**5.5.2 Selected-block highlight (on jump-back).**
No new state in the common case: §5.4 nav sets `anchorAbsRow = block.startAbsRow`, so the selected block
is "the block at/containing `anchorAbsRow`" (add an explicit `selectedBlockId` only if free-scroll
selection is wanted later). Highlight it by building one `DrawContext::Overlay` spanning the block's
visible rows (viewY space) — mirroring search-result highlighting. **Caveat:** the grid-row path
(`TerminalView::DrawScreenRow` + `DrawStringAt`) does **not** call `DrawLineOverlays(y)` today (only the
`LineRender` scrollback path does), so the grid tail of a block won't pick up the overlay until that path
honors overlays (or the highlight fill is drawn before the rows). One-line fix, flagged so it isn't a
surprise.

**5.5.3 Acting on the selected block — quick-command + JS, NOT a bespoke HUD (revised after TS-1.5b
GUI testing).**

The original plan here was an in-terminal HUD action bar with hardcoded `S/P/D` hotkeys. Testing TS-1.5b
surfaced a better seam, so **the HUD is demoted to optional** and the action surface moves to the engine
the editor already has:

- **Observation (the enabling behavior):** the block highlight **persists when you press `ESC` to enter
  quick-command-mode** (the global shortcut that parks the caret in the status bar for command entry). It
  persists because nothing in that path snaps the terminal back to the bottom (`SelectedBlockIndex()` is
  non-empty only while scrolled, and entering quick-command-mode neither commits a line nor types into the
  terminal `inputLine`, so `followBottom` stays `false`). That is not a bug to fix — it is *exactly* the
  hook: the selected block stays the **active target** while you type a command at the status bar. The
  highlight is the affordance; **quick-command-mode is the action surface.** No new in-terminal input mode,
  no contextual key-capture, no hardcoded buttons.
  - To make this reliable we must *intentionally preserve* the terminal's selected-block state across the
    focus switch into quick-command-mode (today it survives incidentally) — i.e. don't clear
    `anchorAbsRow`/`followBottom` just because focus left the terminal view.

- **Direction: expose the selected block to JS, reuse existing wiring.** There is already a `Console` JS
  global (`ConsoleAPIWrapper`) that routes to the terminal via `RuntimeConfig::OutputConsole()` (the same
  `IOutputConsole` the terminal registers as). Extend it:
  - `Console.GetSelectedBlock()` → the selected block's output text (over `SelectedBlockIndex()` +
    `GetBlockOutputText`, **TS-2a**). Optionally `Console.GetSelectedBlockInfo()` for command/exit/lineCount.
  - Then a user/plugin cmdlet does the rest with primitives that *mostly* exist:
    `Editor.NewDocument(name)` already creates a buffer (DocumentAPI); **populating it from a string is the
    one small gap** — add e.g. `Editor.NewDocumentFromText(name, text)` or a `Document.AppendText`. "Put it
    on the paste buffer" likewise needs a thin clipboard JS method (no clipboard is exposed to JS today).
  - Canonical first cmdlet: `var t = Console.GetSelectedBlock(); Editor.NewDocumentFromText("output", t);`
    — i.e. the user's "copy-active-block-to-a-buffer", scripted, no bespoke action code. This *is* the
    Phase 2 / §9 JS surface (TS-2c) applied to "the selected block" instead of "the last block".

- **If a HUD is ever drawn, it goes ABOVE the block, not on the footer.** The block's focus/anchor is its
  **first** line (jump-nav lands on `startAbsRow`, the top row), so a status strip belongs above the first
  row, not over the last. (The earlier "footer row" placement was wrong for this reason.) But the leading
  direction is **no persistent in-terminal HUD** — a one-line hint in the **status bar** (the same surface
  quick-command-mode types into) is more consistent with the editor and costs no terminal rows. Treat a
  drawn HUD as a later, optional nicety.

**Decision (settled, supersedes §12.4):** permanent decoration is the **sub-cell separator only** (no row
cost, no viewport-math change). The selected block gets a **highlight** (TS-1.5b, done); acting on it goes
through **quick-command-mode + the JS `Console`/`Editor` surface** (Phase 2), not a hardcoded HUD. Any
future HUD renders **above** the block's first row. This keeps the §5 viewport model (1 abs row = 1 viewY)
intact and the action set open-ended/scriptable rather than three fixed buttons. The cell-grid backend
falls back via the intention-named `DrawHRule` seam (above).

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
  as `u32 text + its LineAttrib spans (idxOrigString, fg RGBA, bg RGBA, textAttributes)`. The existing text
  `Save/Load` stay untouched. This is the bulk of the persistence phase's new code and is why it lands
  **last** (below).
  - **File header (fixed, the first bytes of the `.bin`).** A small metadata header precedes the line
    records: `magic` (4 bytes, `"GTSB"`) · `version` (`uint32`, a plain **sequential** number — starts at
    `1`, bumped by 1 on any incompatible layout change) · `flags` (`uint32`, reserved = 0) · `lineCount`
    (`uint32`). `LoadWithAttributes` validates the magic and rejects an unknown/newer `version` → **start
    with empty scrollback, never crash** (mirrors `RootSession::kVersion`). Native-endian — a same-machine
    cache, like `session.yml`.
  - **Per-line record.** `charCount:uint32`, then `charCount × char32_t` text; `attribCount:uint32`, then
    per span `idxOrigString:int32`, `textAttributes:uint32`, `fg:4×float`, `bg:4×float`, `tokenClass:uint32`.
- **Block line indices re-based to the saved slice.** Only the last N lines are saved, so on save the
  block `startLine`/`endLine` are rewritten into `[0, savedLineCount)` and blocks entirely older than the
  slice are dropped. On load, the live model seeds `scrollbackBase = 0` and continues. Keeps the on-disk
  format independent of how long the process ran.
  - **The open (running) block is closed at the saved tail (decided this session).** Its live-grid portion
    is never persisted (a fresh shell starts on restore, §8.2), so on save the open block is written as a
    **complete closed block** ending at `savedLineCount`, `exitCode = unknown`. It restores as a normal
    navigable block. (`exitCode` sentinel: `INT_MIN` — distinct from a real `-1`, which OSC 133 emits for a
    shell that ran the command but omitted the status, §4.2.)

### 8.2 Restore semantics

On `OnFolderOpened` → restore: `TextBuffer::LoadWithAttributes(terminal_scrollback.bin)` seeds the
scrollback buffer (read-only) **with its colors intact**, `FromSession` rebuilds `blocks` from
`TerminalSession`, then the **live shell starts fresh on top** with a new prompt. The restored region is
pure, immutable history — scroll/jump/open-as-document work over it exactly like freshly-produced
scrollback; only the *live grid* is new. A restored block's output is fully resolvable (all its lines are
in the loaded buffer, none in the not-yet-existing live grid).

**Ordering (load-bearing).** The shell's pty thread starts writing a prompt/banner into scrollback the
instant `shell.Begin()` runs (`TerminalController::Begin`). So restore must seed scrollback **before** that
— `FromSession` runs *inside* `Begin()` ahead of `shell.Begin()` (new `TerminalScreen::SeedScrollback(lines,
blocks)` seam, which sets `scrollbackBase = 0`) — otherwise the fresh prompt would land *above* the restored
history. Restore is gated, like `session.yml`, on a `.goatedit` (kProject) dir; a missing/corrupt `.bin`
yields empty scrollback.

### 8.3 Constraints / gotchas

- **Write cadence: clean exit only (decided this session).** The `.bin` is bulky, so — unlike the
  lightweight `session.yml`, which also autosaves on every meaningful event — terminal scrollback is written
  **only on clean exit** (`Editor::Close` → `SaveSession`), **not** on the debounced autosave. A crash loses
  the *output* backlog (low-value, regenerable); the *command* history (§8.4) is saved eagerly per-commit
  and survives a crash.
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

### 8.4 Command history (`TerminalCmdHistory`) — already persisted, kept as a plain-text file

The *output* backlog (above) is the scrollback half; the *command* history (readline-style ↑/↓ recall —
`TerminalCmdHistory`, capped at `MAX_ENTRIES`) is the other half the user wants to survive a restart — and
it **already does**, independently of this phase:
- **Format — simple text, one UTF-8 command per line (decided).** The simplest of the candidate options
  (binary / YAML / plain text), and what `TerminalCmdHistory::Save`/`Load` already do: newline-separated
  UTF-8, no binary, no YAML. Kept as-is — a tiny human-readable file is the right primitive here; nothing in
  command history needs colors or structure.
- **Location** — the `terminal_history` asset: `<root>/.goatedit/terminal_history` when a project dir
  exists (kProject), else per-user (kUser). Same `.goatedit/` home as the session cache, so it travels with
  the project (resolved in `TerminalController::Begin`).
- **Cadence** — saved **eagerly, on every commit** (`CommitLine` → `cmdHistory.Save`), loaded on
  `TerminalController::Begin`. Eager is correct here (the file is tiny, and command history is worth keeping
  across a crash — unlike the bulky scrollback `.bin`, §8.3).
- **Phase 5 scope** — no new mechanism; recorded here as the command-history half of terminal persistence
  so the two are documented together. The only Phase-5 touch is keeping it consistent with the scrollback
  `.bin` / block-index story (same `.goatedit/` dir, same project gating).

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
- **JS exposure ✅ (TS-2c).** `TerminalAPI` (`src/Core/API/`) + `TerminalAPIWrapper`
  (`src/Core/JSEngine/Modules/`) ship the JS `Terminal` global: `GetBlocks()`, `GetBlockOutput(id)`,
  `GetLastBlock()`, `GetSelectedBlock()`, `OpenBlockAsDocument(id)`. The split is deliberate (two layers):
  `TerminalAPI` holds all logic + the block→Document composite and is engine-agnostic — it reads the
  terminal only through the extended `IOutputConsole` seam (`OutputBlockInfo` + `GetBlocks`/
  `GetBlockOutputText`/`GetLastBlock`, virtual defaults), never `TerminalController` directly — so a
  non-duktape engine could reuse it. `TerminalAPIWrapper` is pure marshalling. This makes build-parse and
  output-extraction **scriptable**, matching the editor's plugin direction.

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

**Phase 1.5 — block visual affordances (§5.5). Depends on Phase 1. TS-1.5a/b DONE ✅, TS-1.5c TODO.**
- `TS-1.5a` ✅ `commandview.show_block_markers` config (default false; read by `TerminalView`, which owns
  the `commandview` section — not `terminal` as first specced) + `DrawContext::DrawHRule(y)` primitive
  (base no-op; SDL2 + SDL3 draw it via `DrawLineWithPixelOffset` at the row boundary, same machinery as
  the underline attribute) + a sub-cell separator drawn under each CLOSED block's last row in
  `DrawViewContents` (boundary = `(abs+1) ∈ {endAbsRow}`, built once/frame). *(Resolves §12.4.)*
- `TS-1.5b` ✅ Selected-block highlight via `DrawContext::Overlay` — `TerminalController::SelectedBlockIndex()`
  (block containing the anchor; nullopt while `followBottom`, so the highlight only shows when scrolled);
  the overlay is added in the scrolled render branch and painted per-row by `DrawLineOverlays` (now called
  from BOTH the scrollback and the grid-row paths in `drawAbsRow`, so the live tail highlights too).
  Highlight color: new `terminal.selection` theme key (`term_selection`, translucent), with a code
  fallback. Tested via `test_terminalcontroller_selected_block_index` (selected-on-jump, empty on
  follow-bottom, range contains the anchor).
- `TS-1.5c` ✅ ~~contextual HUD action bar~~ **superseded (§5.5.3) and now DELIVERED via quick-command +
  JS, not a bespoke HUD.** `Console.GetSelectedBlock()` (over `SelectedBlockIndex()` + `GetBlockOutputText`
  /TS-2a) feeds the `blocktobuffer`/`b2b` cmdlet `Editor.NewDocumentFromText("output", t)` — the
  populate-doc-from-string gap is closed (a clipboard JS method is still absent). The terminal's
  selected-block state is preserved across the focus switch into quick-command-mode (confirmed by
  construction — only commit/local-edit clear it — and guarded by a test). Any future HUD renders ABOVE
  the block's first row.
- Tests (assert **properties**, not pixels): ✅ selected-block resolution (`SelectedBlockIndex`). Separator/
  overlay *pixels* are GUI-verified, not unit-tested (SDL-bound render). 1.5c tests land with the Phase 2
  JS surface (block text round-trips into a new document).

**Phase 2 — downstream consumers. DONE ✅.**
- `TS-2a` ✅ `GetBlockOutputText(id)` on `TerminalScreen` (resolves scrollback + live grid tail, open
  block runs to the live row) + `TerminalController::GetSelectedBlockText()` (joins the selected block
  under `screenLock`). `Editor.NewDocumentFromText(name, text)` + the `blocktobuffer`/`b2b` cmdlet.
  Selection-survives-focus-loss confirmed by construction + guard test.
- `TS-2b` ✅ "Open last command output as a Document" — `Terminal.GetLastBlock()` +
  `Terminal.OpenBlockAsDocument(id)` (and the *selected*-block `b2b` cmdlet). Block text → Document via
  the `EditorAPI::NewDocumentFromText` composite, which lives in `TerminalAPI` (engine-agnostic), not the
  JS glue.
- `TS-2c` ✅ dedicated **two-layer** `TerminalAPI` (`src/Core/API/`, logic) + `TerminalAPIWrapper`
  (`src/Core/JSEngine/Modules/`, thin glue), registered as the JS `Terminal` global:
  `GetBlocks()`/`GetBlockOutput(id)`/`GetLastBlock()`/`GetSelectedBlock()`/`OpenBlockAsDocument(id)`. The
  API consumes the terminal only through the `IOutputConsole` seam (extended with a plain-data
  `OutputBlockInfo` + `GetBlocks`/`GetBlockOutputText`/`GetLastBlock`, virtual defaults). The interim
  `Console.GetSelectedBlock` glue was removed in favour of `Terminal.GetSelectedBlock`. The clipboard
  seam is also shipped: `ClipBoard::CopyText` + `Editor.CopyToClipboard(text)` +
  `Terminal.CopyBlockToClipboard(id)` + the `b2c` cmdlet.
- Tests: ✅ `test_terminalscreen_block_output_text` (closed + open, both stores),
  `test_terminalcontroller_selected_block_text`, `test_terminalcontroller_selection_survives_output`,
  `test_terminalcontroller_block_index_surface` (by-id seam mirrors the model; unknown id → nullopt),
  `test_jsengine_newdocumentfromtext`, `test_jsengine_terminalapi` (JS `Terminal.*` round-trip:
  controller-as-console → enumerate by id → `OpenBlockAsDocument` → new doc content).

**Phase 3 — OSC 133 shell integration (optional, opt-in fidelity upgrade). ✅ DONE**
- `TS-3a` ✅ VTermParser OSC 133 / OSC 7 → new `kAnsiCmd`s (`kPromptStart`/`kCommandStart`/`kOutputStart`/
  `kCommandEnd`/`kSetCwd`). `CMD` gained a `std::string strParam` (carries the OSC 7 path; everything else
  leaves it empty). New `OSC_ReadPayloadToTerminator()` reads the payload with no leading-char skip and
  honours BEL **and** ST (8-bit `0x9c` or two-byte `ESC \`); `ParseOSC133()` splits A/B/C/D and parses
  `D;<exit>` (`-1` when the shell omits it). OSC 7 strips `file://host` → path.
- `TS-3b` ✅ `TerminalController::HandleAnsiCmd` maps `kOutputStart` → `BeginCommandBlock(cmdText, kOsc133)`
  with `cmdText` recovered from the `;B..;C` grid span (`ReadGridText`), `kCommandEnd` → `EndOpenBlock(exit)`,
  `kSetCwd` → `SetOpenBlockCwd`. A `useOsc133Boundaries` flag (set on the first marker, read under
  `screenLock`) makes the `CommitLine`/tab-completion heuristic **stand down** so OSC 133 and the heuristic
  never double-open. `HandleTerminalData` is now public (the pty-output sink — lets a test feed a scripted
  byte stream without a live shell, same role `RegisterAsOutputConsole` plays for output).
- `TS-3c` ✅ `terminal.shell_integration` config flag (**default `no`**, §12.3); when on, `Begin()` appends
  shell-aware OSC 133 prompt hooks (`BuildShellIntegrationBootstrap` — zsh `preexec`/`precmd` + PS1 A/B,
  bash DEBUG-trap guarded to fire `;C` once per cycle + PROMPT_COMMAND `;D` + PS1 A/B) to the bootstrap.
- Tests: ✅ `test_vtermparser_osc133` (A/B/C/D, `D;0`/`D;130`/bare `D`→-1, BEL **and** ST terminators, no
  leak into stripped text), `test_vtermparser_osc7_cwd` (empty + named host); `test_terminalcontroller_osc133_block`
  (command text from `;B..;C` + exit code, `kOsc133` source, closed at `;D`), `_osc7_cwd` (cwd on the open
  block), `_osc133_no_double_open` (prompt `;A/;B` → CommitLine suppressed → one block). All verified-green.

**Phase 4 — per-block language / highlighting (§3.4).** *(optional, later; depends on Phase 1.)*
- `TS-4a` command→language map (argv0 → `LanguageBase`); set `block.language` on open.
- `TS-4b` **hard-region tokenize**: run a block's language over its line range, writing attribs — new
  `TextBuffer` capability (region reparse with an *explicit* language + a *hard* start boundary, no
  look-back past the block). Non-trivial: the tokenizer runs on a **background `Job`** (§3.4).
- Tests: a `cmake` block's lines get CMake token attribs; an un-languaged block keeps its ANSI attribs;
  hard-region reparse of block N doesn't disturb block N±1 (no state bleed across the boundary).

**Phase 5 — persistence & restore (§8) — DONE ✅ (user: "add once everything works").**
*Decisions locked: write cadence = **clean exit only** (§8.3); open block = **closed at the saved tail**
(§8.1); persistence **on by default**, caps **2000** on-disk / **10000** in-memory (§12.2); cmd history =
**plain text, one cmd/line**, already shipped (§8.4).*
- `TS-5a` ✅ `TextBuffer::SaveWithAttributes()` / `LoadWithAttributes()` — versioned binary: a fixed
  header (`magic "GTSB"` · `version:uint32`, sequential, start `1` · `flags` · `lineCount`) then per-line
  `text + per-span fg/bg/attrs`; bad magic / unknown version → empty, no crash. Existing text `Save/Load`
  untouched. The bulk of the new code.
- `TS-5b` ✅ `TerminalSession`/`TerminalBlockSession` DTOs (block index + `.bin` file pointer, **no row
  text**) on `RootSession`; `SessionSerializer` `TerminalToNode`/`NodeToTerminal`; block line indices
  re-based to the saved slice (`exitCode` sentinel `kExitCodeUnknown == INT_MIN`).
- `TS-5c` ✅ Terminal persistence end-to-end. New `TerminalScreen` seams: `SnapshotScrollbackTail(maxLines)`
  (returns `ScrollbackSnapshot{lines, PersistedBlock[]}` re-based into `[0, lines.size()]`, open block
  clipped+closed at the scrollback boundary, empty while alt-screen) and `SeedScrollback(lines, seedBlocks)`
  (replaces scrollback at base 0, inserts seeded blocks CLOSED + a fresh OPEN loose block for new output).
  `TerminalController::ToSession`/`FromSession` map `PersistedBlock`↔`TerminalBlockSession` (`Source`↔int via
  explicit switch, `exitCode` optional↔sentinel, u32↔utf8, `cwd` path↔string) and own the `.bin` write/read
  (`SaveWithAttributes`/`LoadWithAttributes`) + path resolution (`ResolveScrollbackBinPath` → kProject
  `.goatedit`). Save is driven from `Editor::SaveSession` via `GetTerminalView()->GetController()` (clean-exit
  only, idempotent); restore runs **inside `Begin()` before `shell.Begin()`** reading
  `SessionManager::CurrentSession().terminal`. Gated on `terminal.persist_scrollback` (default on) + a project
  dir. **NOT YET verified in a live GUI save→restart→restore.**
- `TS-5d` ✅ Command history (`TerminalCmdHistory`, §8.4) — **already shipped** (plain-text `terminal_history`,
  saved per-commit, loaded on `Begin`); the command-history half, no new code.
- Tests: TS-5a — `.bin` round-trip preserves text **and** colors; header rejects bad magic / newer version →
  empty (`test_textbuffer_saveattribs_*`). TS-5b — block index round-trip + back-compat (`test_session_terminal_*`).
  TS-5c — snapshot/seed round-trip (lines + closed blocks + fresh loose tail), alt-screen → empty snapshot,
  on-disk cap clips+re-bases to the tail, open block closed-at-tail (`test_terminalscreen_snapshot_*`). The
  controller glue + Editor wiring lean on these seam tests (no asset-loader/session infra in unit tests).

---

## 12. Decisions to confirm

1. **Scroll keybindings — settled (§5.3).** Plain `PageUp`/`PageDown` reuse the existing
   `kUIActionPageUp/Down` and page the focused view (terminal = its backlog); no new action, no
   modified-key split (we distinguish alt-screen-forwards-to-app from prompt-scrolls-backlog, so the
   xterm Shift+PageUp convention isn't needed). The only code change is a PageUp/Down handler in the
   shell-prompt path. Left here only as a record of the resolved choice.
2. **Default scrollback cap — settled.** `terminal.scrollback_lines` (in-memory) = **10000** (`0` =
   unlimited). Separate, smaller `terminal.persist_scrollback_lines` (on-disk, §8) = **2000**. Terminal
   persistence is **on by default** (gated only on a `.goatedit` dir, like `session.yml`) — see §12.7.
3. **OSC 133 on by default? — settled (Phase 3 shipped).** **Off by default**, opt-in via
   `terminal.shell_integration` (default `no`), since it rewrites the user's PS1; the `CommitLine`
   baseline already delivers blocks without it. When on, the `BuildShellIntegrationBootstrap` snippet is
   appended to the shell bootstrap and `useOsc133Boundaries` makes the heuristic stand down so the two
   never double-open.
4. **Block visual affordance — settled (§5.5, Phase 1.5).** Permanent: a sub-cell separator rule at block
   boundaries (`commandview.show_block_markers`), no row cost, no viewport-math change. Selected block (on
   jump-back): an `Overlay` highlight (TS-1.5b, done) — never a permanent inter-block row, so the §5
   viewport model (1 abs row = 1 viewY) stays intact. **Acting** on the block is via quick-command-mode +
   the JS `Console`/`Editor` surface, NOT a hardcoded HUD (§5.5.3, revised after testing); any future HUD
   renders **above** the block's first row. The future cell-grid/SSH backend falls back to a box-drawing-char
   rule via the intention-named `DrawHRule` seam.
5. **Color fidelity across restart — settled (§8.1, user's call).** Preserve the actual colors: persist
   the per-span attributes in a binary `.bin` via new `TextBuffer::SaveWithAttributes()`/`LoadWithAttributes()`,
   so restored scrollback comes back with its real ANSI colors — *not* re-derived from syntax (we won't
   write a highlighter per tool). Scheduled **last** (Phase 5). Per-block syntax (§3.4) stays independent
   and optional.
6. **Per-block highlighting storage — settled (§3.4).** Blocks are line-range **references** into one
   shared scrollback `TextBuffer` (not split per-block); per-block language is an additive region-reparse,
   deferred to Phase 4. Recorded as the chosen direction.
7. **Phase 5 persistence cadence + open-block — settled this session.** The bulky scrollback `.bin` is
   written **only on clean exit** (not on the debounced autosave, §8.3); the running/open block is persisted
   **closed at the saved tail** (its live-grid tail is dropped, exit unknown, §8.1). Terminal persistence is
   **on by default** (gated only on a `.goatedit` dir, like `session.yml`).
8. **Command history persistence — settled (§8.4).** Kept as a **plain-text file, one UTF-8 command per
   line** (`terminal_history`, already shipped — saved per-commit, loaded on `Begin`), not binary/YAML. The
   scrollback `.bin` carries a small **versioned header** (`magic` / `version` (sequential) / `flags` /
   `lineCount`); a bad magic or newer version restores empty, never crashes.

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
