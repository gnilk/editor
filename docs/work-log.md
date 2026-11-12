# Work log

Short, reverse-chronological index of completed feature efforts. Each entry summarises *what shipped*
and *the load-bearing decisions*; the linked detail doc carries the full cold-start history. Planned /
not-yet-started efforts are listed at the bottom. For known-wrong code deliberately left unfixed, see
[`open-bugs.md`](open-bugs.md) (the active tracker, not logged here). For work explicitly deferred /
left out of scope from shipped features, see [`deferred.md`](deferred.md).

---

## Cmd+click / Ctrl+click goes to definition (2026-11-12) ✅

A single click with `CopyPasteModifier` held (`Cmd` on macOS, `Ctrl` on Linux) moves the caret to the name and posts
`GotoToggle`, as `Cmd+B` does. **Load-bearing decisions:** the modifier comes from the keymap (`KeyMapping::ModifierMask`,
the `modifiers:` table kept after the build, a child's own winning), not a hard-coded key; the action is posted through
`Runloop::DispatchAction`, not called, so the jump lands on the navigation stack; only `clicks == 1`. A keymap `mouse:`
section is planned to make the binding itself data (usability-round §13). Detail: [`code-index.md`](code-index.md) §5.6.

## Folder double-click in the workspace tree (2026-11-12) ✅

A double-click on a folder did nothing (`OpenSelectedItem` returned early on a folder). **Shipped:** a double-click or
Enter on a folder expands or collapses it (`TreeView::ToggleExpanded`); an unscanned folder fetches its children on
the way, as the arrow key does. **Load-bearing decisions:** Enter toggles too - it shares the one open path with the
double-click; act on `clicks == 2` only, since SDL counts on and a triple-click would toggle back. Detail:
[`usability-round.md`](usability-round.md) §10.

## YAML language support (2026-11-11) ✅

`.yml`/`.yaml` highlighting and editing. **Shipped:** `YAMLLanguage` - comments and quoted strings in the
tokenizer; keys, list markers, document markers, typed scalars (`true`, `8080` - only as the whole value),
anchors/aliases/tags and one-line flow collections in a post-pass; block scalars (`run: |`) through two
general tokenizer additions (`kPushStateAtEOL`, indent-scoped states); Enter indents after `key:` and lines
up with an item's key after `- name: x`. Verified by the user in the app (ANSI backend). **Load-bearing
decisions:** a new class `kConfigKey` (not `kKey` - too generic; placed before the semantic block, which
`[kUserType, kLastTokenClass)` defines), teal like C++ class names (red read as errors); a block scalar's scope
is the key's column, not the line's indent (`- run: |` must end at its sibling `name:`); the indent-scope pop
runs before the depth stamp, in both `ParseLine` and `TokenizeText`; reformat leaves YAML alone (`reindent: no`)
- nothing closes a `key:`, so the opener walk would staircase the file. Detail: [`support-yaml.md`](support-yaml.md).

---

## Install layout and asset lookup (2026-11-09)

The Linux build only started from inside its build dir: system assets were looked up in `./resources/`
(the cwd) or an installed `/usr/share/goatedit`. **Shipped:** `InstallPaths` finds exactly one system asset
dir relative to the executable (build tree, macOS bundle, any install prefix, `$XDG_DATA_DIRS` last;
`--asset-dir` overrides); the user layer is `~/.config/goatedit` on both platforms, `~/.goatedit` read-only (not `~/.local/share/goatedit`: a `~/.local` install puts its system assets there);
log and user-scope terminal history in `~/.local/state/goatedit/`; `scripts/install.sh --user|--system`
with a manifest-driven `--uninstall`. **Load-bearing decisions:** never the cwd for assets (a stray
`resources/` could take over); a dev tree and an install are never mixed (one system dir); within a location
type the loader keeps registration order (the old hit-count reordering let a legacy dir overtake the
preferred one). Also: the terminal no longer hangs startup when the shell can't start, and the default
shell is `$SHELL`. Detail: [`install-layout.md`](install-layout.md).

---

## Code index - structural search over the code base (in progress)

The base for "search everywhere": a lexer-driven index of symbols and their relationships, not plain text
search. **Shipped:** step 0 (`TokenizeText` fast path, `gedit-bench`, tokenizer 3-4x faster), step 1 (C/C++
structure pass: symbols, inherits/holds/includes), step 1b (semantic highlighting from the file's own
declarations, kept per `Line`), step 1c (VS Code / Sublime theme import, `--import-theme`,
`.theme --import`), step 1d (library names), step 2b (smart go-to on `Cmd+B`, navigation stack on `Cmd+[` / `Cmd+]`,
kept across a restart), step 2 (in-memory index, background indexer, Go to symbol on Cmd+T / Ctrl+T, open files fed
from their buffer, disk changes through `FolderMonitor`, `ExclusionRules` with real `.gitignore`, cross-file colouring).
**Load-bearing decisions:** no compiler or language server; anything per-buffer rides
the highlighter's line/region machinery, never a whole-buffer copy or re-lex per edit; themes are converted
once, not read at runtime (D13); library names are shipped data, not a toolchain scan (D14); a name with several
meanings in the workspace stays plain, and without locals in the index, context decides the rest (§11.6); a library
never takes a name the project declares, and a bare C++ name needs a `using` in the file (§10.2). Step 1d shipped too: hand-made `c_std` and
`cpp_std` lists, `std::` by syntax, the `using` tiers, project lists in `.goatedit/libraries/` and lists that apply by header
(§11.8). Step 3 (2026-11-06): edges in the index, open files included; the relationship view (`Cmd+Shift+T`) with the code of each
relation in colour; go to implementation and base (`Cmd+Shift+B`, `Cmd+Shift+U`) (§11.10). The longer aim, keeping the architecture
in view while working with an agent, is in §10.3; a context menu at the caret (`Cmd+Return`) is an idea in §5.7.
**Paused 2026-11-08:** step 4 (modules) is next, once its design is thought through. Detail: [`code-index.md`](code-index.md).

## Project config — `<root>/.goatedit/config.yml` merged at startup ✅ (on `feat/project-config`)

Config now merges **system → user → project** (last wins). The project root is resolved *before* the
config load by `Editor::ResolveProjectDir(args, cwd, home)`: the LAST absolute folder argument, else the
cwd (after the outside-`$HOME` relocation, which moved ahead of the config load). Same guard as
`EstablishProjectDir` — strict subdir of `$HOME` only, and `.goatedit/` must already exist (resolving
never creates it; a folder open may still auto-create it later, but its config can only apply next
launch). Because the merge happens with the rest of the config, logging/theme/languages/views all see it.
- **Denied keys** (a cloned repo may carry `.goatedit/config.yml`, so opening a folder must not run its
  commands): `main.bootstrap_scripts`, `main.plugin_directory`, `terminal.shell`, `terminal.init`,
  `terminal.bootstrap`, `logging.logfile` — stripped by `ProjectConfigPolicy::StripDeniedKeys` (a denied
  section given as a non-map is dropped whole); a non-map document is ignored. (Now in
  `ProjectConfigPolicy` - see the `.cfg` entry above.)
- `--skip-user-config` skips the project config too.
- Sequences still merge **by index** (`YAMLMerge`) — a project can override/extend a list, not shorten it.
- Behaviour change: a folder argument outside `$HOME` no longer leaves the *launch dir's* `.goatedit`
  registered as `kProject` (it was, since the init-time registration used the launch cwd).

## `.cfg` — read/write config from the command line ✅ (on `feat/project-config`)

`.cfg [--user | --project] <action> [args]` (plugin `config`, short `cfg`; `Scripts/config.js` →
global JS `Config` (`ConfigAPIWrapper`) → `ConfigAPI`). Actions: `list [prefix]` (running config with the
level each value comes from, or the `--user`/`--project` file as-is), `get <key>`, `set <key> <value>`,
`edit` (opens the level's file in a buffer; creates it with a header comment if missing).
- **No `--global`:** the system config is never written from here (changing the default for all users
  is a deliberate, by-hand act).
- **Nothing is kept per level.** Each call loads the user/project files fresh from disk into temporaries
  (a hand-edit since startup is never clobbered). `set` writes the target file, then puts the EFFECTIVE
  value (project > user > system) into the running config - a user value the project overrides warns
  and leaves the running value alone. `--skip-user-config` instances only get the file written.
- **Comments are sacred** (they're used to toggle logging): `ConfigFilePatcher` edits the TEXT in place
  - replaces the one value (keeping a trailing `# comment`) or inserts missing keys at the end of the
  deepest existing section; every other line byte-identical. **No fallback:** the result is re-parsed
  and must equal "original + this one value", otherwise nothing is written and the user is pointed at
  `.cfg --<level> edit`. Refuses flow style, list/map targets, tabs, CRLF, unparseable files.
- Writes are atomic (tmp → rename) and go **through** a symlinked config (dotfiles) to its target.
- The project deny-list moved out of `Config` into `ProjectConfigPolicy` (used by the startup merge and
  by `set --project`).
- Live effect is limited to settings read after the set; startup-read settings (theme, languages,
  keymaps, a view's construction-time options) need a restart - `set` says so.

## Soft-wrap (word/char line wrap) ✅

Long lines continue on the following screen rows instead of clipping at the view edge. Off by default;
`editorview.wrap: none | char | word` in config. Deferred tail: [`deferred.md`](deferred.md) (Soft-wrap).

- **`WrapProjection` (`src/Core/WrapProjection.{h,cpp}`)** — pure, stateless, sibling of `HexProjection`
  (deliberately NOT "layout", to stay clear of the UI layout classes). `WrapLine` splits a line into row
  segments; `ProjectRows` gives the visible rows; `TextToScreen` / `TextToScreenClamped` / `ScreenToText`
  map between text (line, char) and screen (row, col). Tab stops stay LINE-relative across a wrap
  (`segment.visualStart` feeds `LineRender::ExpandTabs`'s `startCol`); every segment holds ≥1 char; word
  mode lets whitespace on the break hang off the row and hard-breaks words wider than the view.
- **Gutter and editor share ONE row list, via the Document — the views never link to each other.** The rows
  are per-view state: `DocumentViewState::ProjectVisibleRows` (+ `wrapMode`/`wrapWidth`, pushed in by
  `EditorView` from config and its content width on every `(Re)InitView`). Both views read
  `Document::GetVisibleRows()`. No cache — only the visible lines are wrapped, and `TextBuffer` has no
  revision counter to invalidate on.
- **Model stays logical; the view translates.** Cursor / selection / search stay in text coordinates.
  Caret, selection + search overlays and mouse clicks map through the rows. Overlay ends off-view clamp to
  row -1 / row N — the existing "rows strictly between start and end are fully covered" overlay rule (all
  three backends) then paints wrapped spans with no backend change. `LineRender::DrawLines` became
  `DrawRows` (one code path: without wrap each row is a whole line).
- **Scroll correction at one chokepoint.** Navigation still scrolls in LINES (`VerticalNavigationViewModel`);
  `DocumentViewState::ScrollActiveRowIntoView` advances `viewTopLine` until the caret's row fits, keeping
  `viewBottomLine = top + height` and `position.y = idxActiveLine - top` so the line-based nav stays
  consistent. Called from `Document::GetVisibleRows()` — every consumer reads rows there, so every caret
  move (keys, typing, goto, wheel, search from another view) is covered. No-op without wrap.
- Tests: `wrapprojection` module (tiling property, break rules, tabs, mapping round-trip, scroll
  correction); `test_document_wrap_caret_stays_visible` drives real navigation with wrap on (fails without
  the correction).

## ANSI/TTY graphics backend — `gansi` ✅ (merged to `main`, `e9fa64f`)

A modern-terminal rendering backend so the editor runs in a TTY / over SSH, selectable alongside SDL
(`--backend ansi` or `main.backend: ansi`; a plain terminal session now auto-selects it). NCurses was
removed entirely. Detail + full phased plan: [`done/ansi-graphics-backend.md`](done/ansi-graphics-backend.md).

- **Standalone library `gansi` (`gnilk::ansi`), NOT editor code** — its own folder
  (`src/ext/gansi/`), own `CMakeLists.txt` (static target `gansi`, alias `gnilk::ansi`,
  `add_subdirectory`'d by the editor — never source-listed), own trun tests (`gansitests`), zero editor
  dependencies. Think "our SDL": a cell grid + ANSI encoder + input parser + a single platform seam.
- **Cell model + damage-tracked encoder** (`CellGrid`, `AnsiEncoder`): double-buffered grid; the diff
  emits only changed cells, one CUP per contiguous run, an SGR only when the truecolor pen changes
  (full repaint only on resize). Byte-for-byte unit-tested.
- **Input parser** (`InputParser`): split-read-tolerant byte→`Event` state machine — UTF-8, C0
  controls, CSI/SS3 special keys with xterm modifier params, **Kitty keyboard (`CSI u`)** (fixes
  Shift-Arrow + Alt-as-nav, the SDL/macOS pain), SGR mouse (1006), bracketed paste, focus.
- **Platform seam** = `ITerminalIO`; `PosixTerminalIO` is the only termios/poll/SIGWINCH file (macOS +
  Linux share it; Windows would be the sibling impl). `MockTerminalIO` (public header) drives everything
  headlessly — the whole library + backend are unit-tested with no real TTY.
- **Backend `gedit::Gansi`** mirrors the SDL layout: `GansiScreen`/`GansiWindow`/`GansiDrawContext`/
  `GansiKeyboardDriver`. One shared back-buffer; windows are clip rects; `DrawContext` writes
  `origin+local` cells (no pixels/scaling). Inverse swaps fg/bg, underline/bold→SGR, `DrawHRule`→box
  row, overlays→invert covered cells. Real terminal hardware cursor; OSC 52 clipboard out + bracketed
  paste in (honours the **E.18** external round-trip). `CopyToTexture`/`ClearWithTexture` = grid
  snapshot/restore.
- **Load-bearing decisions:** library stays under `gnilk::` (the signal it's not editor code); coexists
  with SDL (a third backend, *not* the SDL2-XOR-SDL3 split) under `GEDIT_BUILD_ANSI`; no
  `WireScreenGeometry` (a TTY owns its own size). TDD throughout — each unit's tests written first
  against a stub (RED) then implemented (GREEN); ~90 tests across `gansitests` + `test_gansibackend`,
  plus a real-pty smoke confirming the init/teardown escape sequences.
- **Terminal capability detection** (`gansi/Capabilities.{h,cpp}`): `Detect(TERM/TERM_PROGRAM/COLORTERM)`
  → feature profile; `Terminal::Open/Close/Flush/SetClipboard` gate every emitted sequence on it, and
  `AnsiEncoder` degrades truecolor→xterm-256 (`Rgb256`, cube + grayscale-ramp nearest) when 24-bit isn't
  advertised. Fixes the macOS **Terminal.app** stray-glyph: it was the Kitty-keyboard enable
  (`CSI > 1 u`) printed verbatim by a terminal that can't parse it. Apple_Terminal now gets
  Kitty/focus/SGR-mouse/OSC 52 gated **off** + 256-color; capable terminals (Ghostty/kitty/foot) keep
  full fidelity. Env-heuristic for now (runtime DA/XTVERSION query is a possible later refinement).
  Verified end-to-end via the pty harness (Apple_Terminal vs ghostty startup streams).
- **Post-ship hardening on real terminals (macOS Ghostty/Terminal.app/iTerm + Linux):** bracketed paste
  now *inserts* — the terminal handles Cmd+V / Ctrl+Shift+V itself and delivers a paste *event* (not a
  keypress), so it's routed through a new shared `Runloop::DispatchAction` (the action peer of
  `ProcessKeyPress`/`ProcessMouseEvent`, one dispatch chokepoint) as a normal `PasteFromClipboard`.
  Keyboard-modifier-reporting limits (iTerm pre-3.5 drops Shift+Arrow, Terminal.app composes Option to a
  glyph) are terminal-side and unrecoverable app-side — documented in §9, with the low-level OS
  interception "fix" (`CGEventTap`/IOKit) recorded as a rejected dead end. Kitty CSI-u functional-key
  decoding deferred ([`deferred.md`](deferred.md)). Verified on macOS+Ghostty and Linux.

---

## Mouse double-click — open a workspace file ✅ (merged to `main`, `7d4bcca`)

Double-click a file row in `WorkspaceView` to open it + focus the editor (like Enter), via a pure
`MouseClickTracker` (click-count on `MouseEvent.clicks`, `mouse.dbl_click_speed` config; no backend or
`Timer` edits — detection sits at the `Runloop::ProcessMouseEvent` chokepoint, runloop-thread, lockless).
Fixed along the way: `RootView::DispatchMouse` focus-follows-click now fires on press/wheel, not the
trailing release (which was clobbering the editor focus the double-click just set). Detail:
[`mouse-dblclick.md`](done/mouse-dblclick.md).

---

## UI refactor — generic toolkit vs editor-specific UI ✅ (merged to `main`, `d1622a7`)

Split the monolithic `src/Core/Views` + controllers into a **generic UI toolkit** (`src/Core/UI/`) and
**editor-specific UI** (`src/Core/Editor/`). `src/Core/Views/` and `src/Core/Controllers/` no longer
exist. Goal: a clean one-way boundary (toolkit never depends on app services) — both for overview and to
unblock the planned graphics-backend refactor.

- **Folder split** (AI-1): generic views/`BaseController`/graphics *contract* → `UI/`; editor
  views/controllers/`LineRender` → `Editor/`. Shared primitives (`Rect`/`Point`/`ColorRGBA`/…) stayed in
  `src/Core/` root — confirmed shared with the document model, not UI-exclusive.
- **Include-discipline CI gate** (AI-2): `scripts/check-ui-boundary.sh` fails if `src/Core/UI/` includes
  any app header (`Editor.h`/`RuntimeConfig.h`/`Document.h`/`Core/Session|Config|Plugins/*`). Now
  **blocking** in CI; reports clean.
- **Three chokepoints severed by injection, not direct calls:** `UIHost` replaces the `RuntimeConfig`
  service-locator (AI-3); `ILayoutSink` replaces `SessionManager`/`LayoutSession` in `ViewBase` (AI-5);
  theme colors + quick-command policy injected via `UIHost` + a `std::function` hook instead of
  `Editor::Instance().GetTheme()`/state reads (AI-6). Each glued in the init layer (`Editor::Initialize`
  / `SetupSDL*`), mirroring `WireScreenGeometry`.
- **`kAction` split** (AI-4): toolkit-owned `kUIAction` (`src/Core/UI/Input/UIAction.h`) carries the
  shared nav + view-management actions; `kAction` keeps editor actions. `EditorAction`/`ActionItem`
  carry both fields side by side. Follow-up: enumerators renamed to the `kUIAction<Value>` prefix
  (house convention — members repeat the enum's own type name).
- **AI-7 (physical `goatui` library): DROPPED** — the in-tree boundary serves both drivers; a shipped
  library isn't worth its carrying cost without a real second consumer. CMake `ui_src`/`appui_src`
  grouping deferred indefinitely (cosmetic).

Detail + decision log: [`ui-refactor.md`](done/ui-refactor.md).

---

## Markdown syntax highlighting — v1 ✅ (in the verified-green set)

`.md`/`.markdown` routes to `MarkdownLanguage`. Push/pop tokenizer states for fenced code (persists
across lines), code spans, strong/emphasis, links; line-anchored block syntax (ATX headings, blockquote,
ordered/unordered list markers, thematic breaks) via a new injected `LangLineTokenizer::PostLineCallback`
→ `LanguageBase::OnPostProcessParsedLine`. Goal was *decent* highlighting, not a CommonMark parser
(spec-exact emphasis / setext / reference links are explicit non-goals). Module `markdown` green.

Remaining (optional/aesthetic): a GUI color pass to retune the `md_*` placeholder colors; the §6 lexer
work is optional. Detail: [`support-markdown.md`](done/support-markdown.md).

---

## Session cache — Phase 1 ✅ (merged; Step 2 deferred)

Per-root session persistence: open documents, layout (splitters + focused top-view + tree
expand/collapse), window geometry, and debounced autosave all round-trip (GUI-verified). The old
*global* window-geometry file (`WindowLocation`/`gedit_lastwinloc.yml`) was removed — geometry is now
per-root session state and **no rendering backend does file I/O**. `SessionManager` (singleton) is the
sole owner of session disk I/O; a session belongs to an open **folder**, never a single file.

**Step 2 — live registry + cold-start restore of multiple instances (§3.5) — is DEFERRED** (decided
2026-06-16): registry/restore/paths files don't exist yet; revisit when the cold-start work is picked
up. Other deferred items: doc paths stored absolute (→ relativise), `LoadDocument`↔`ReopenDocument`
consolidation, gated undo persistence. Detail + phasing: [`session-cache.md`](done/session-cache.md).

---

## Folder scanner — extract the filesystem walk out of Workspace ✅ (on `fix/folder-scanner`)

Lifted the recursive FS walk out of `Workspace` into a self-contained, testable `FolderScanner`
(`src/Core/FileSystem/`) that emits plain-data discovery events through callbacks (`onEnterDir` veto /
`onFile` / `onLeaveDir(fullyScanned)` + depth) — no `Node`/`Document`/`Workspace`/singleton knowledge,
so it unit-tests over a temp dir (callbacks, NOT templated on the node type). Resolved
[`open-bugs.md`](open-bugs.md) #4 end to end. FS-6 lazy expansion also shipped.

- **FS-1 / FS-4**: pure scanner leaf + `maxDepth` bound + `followSymlinks=false` (the crash side of #4 —
  a symlink cycle is emitted once and never descended, so the ENAMETOOLONG → `terminate` abort is gone).
- **FS-2**: shared `FsFilter` (glob/name) on `Glob.h`; the exclude list relocated off the disabled
  `foldermonitor` config to the scan-owned `workspace.exclude` (share the data, not the subsystem) — the
  build-dir side of #4. Scan-time exclude stays distinct from the view-time dotfile hide.
- **FS-3**: `Workspace::ReadFolderToNode` reimplemented as a scanner adapter (parent-`Node` stack →
  `ApplyFsEntry`, still the single mutator); recursion deleted. Scan stays **synchronous/main-thread** —
  the background-producer model is gated on the un-synchronised `Runloop::SwapQueues`
  ([`open-bugs.md`](open-bugs.md) #6) and deliberately unused here.
- **FS-6 (lazy expansion)**: three-tier mirror — `onLeaveDir` carries `fullyScanned`; `Node::isScanned`
  records it; `FillTreeView` translates `isScanned → hasUnfetchedChildren` (the single model↔view point);
  `TreeView` generic hook `cbFetchChildrenForNode` fires on expand; `WorkspaceView` wires it to
  `Workspace::ScanNode` (maxDepth=1 shallow re-scan + targeted mirror). Config `max_scan_depth: 1` so
  only the top level is scanned eagerly; `Node::ClearChildren` + `ScanNode` are idempotent.
- **FS-7**: `test_folderscanner` (11 cases) + `test_workspace` (5 FS-6 cases) + `test_treeview` (5 cases);
  all in the verified-green set (256 tests).

**Deferred (NOT this branch):** FS-5 (monitor reuse — scan a newly-created dir; blocked on the disabled
monitor + #6). Plan + work items: [`folder-scanner.md`](done/folder-scanner.md).

---

## Terminal scrollback + command blocks — Phases 0–3 + 5 ✅ (Phase 4 deferred) — blocks REMOVED 2026-11-10

> **2026-11-10, owner's call: command blocks are gone** (branch `feat/gvt`). Never used in practice, and the
> selected-block highlight jumping between short blocks made wheel-scrolling flicker. What is left is plain
> scrollback (line-granular trim and persistence, `session.yml` keeps only the `.bin` name) plus one marker -
> where the last command's output starts (`TerminalScreen::MarkOutputStart`, set at commit or OSC 133;C) - for
> `Terminal.GetLastOutput / OpenLastOutputAsDocument / CopyLastOutputToClipboard` (a stack trace to copy). Removed:
> `CommandBlock`, block eviction/persistence, the highlight, end-of-block rules, Alt+Up/Down prompt jumps, OSC 7
> cwd, the block JS API and the `blocktobuffer`/`blocktoclipboard` plugins. The text below is history.

Resolved [`open-bugs.md`](open-bugs.md) #10 ("missing scrollback") and built the *grouping* + downstream
+ persistence infrastructure on top of it. The scrollback buffer already existed; the bug was that the
view always pinned to the bottom. Manually verified (history + session restore round-trip, cmd history
preserved, JS bindings in place) and closed; only Phase 4 (per-block highlighting) is deferred.

- **Phase 0 (TS-0a..TS-0f) — the scroll viewport.** Scrollback became a `TextBuffer` (`Row→Line` is
  lossless — `Line::LineAttrib` already carries per-span `ColorRGBA`, so ANSI color survives and the
  history renders through the existing `LineRender`, no bespoke path). Abs-row spine
  (`scrollbackBase`/`AbsRowCount`/`RowAtAbs`) so positions survive eviction. Viewport state
  (`followBottom`/`anchorAbsRow`) lives on the controller, anchored to an absolute row (not an offset from
  the bottom) so a streaming build doesn't yank a scrolled-up reader back down. Wheel + PageUp/Down +
  Ctrl+Home/End gestures. `TerminalHistory`→`TerminalCmdHistory` rename cleared the name collision first.
- **Phase 1 (TS-1a..TS-1c) + 1.5a/b — command blocks (the grouping).** `CommandBlock` is a line-range
  *reference* into the shared scrollback `TextBuffer` (not a copy), tracked in a `blocks` deque on
  `TerminalScreen`. Opened/closed from `CommitLine` (+ the tab-completion/shell-owned-line commit path)
  under `screenLock`; alt-screen rows never create blocks. An implicit leading "loose" block covers output
  with no committed command, so eviction is always whole-block (a retained block is always complete) —
  never orphans a partial block. Alt+Up/Down jump the viewport to the prev/next block start; block
  separator rule + selected-block highlight (1.5a/b).
- **Phase 2 (TS-2a..TS-2c) — downstream consumers.** `GetBlockOutputText` resolves a block's `[start,end)`
  across scrollback + the live grid; a dedicated two-layer `TerminalAPI`/`TerminalAPIWrapper` JS surface
  (`Terminal.GetBlocks/GetBlockOutput/GetLastBlock/GetSelectedBlock/OpenBlockAsDocument`) replaced the
  implicit one; the `Editor.NewDocumentFromText()` seam + the `blocktobuffer` (`b2b`) cmdlet open a block's
  output as a buffer; a clipboard JS seam (`Editor.CopyToClipboard` + `Terminal.CopyBlockToClipboard` + the
  `b2c` cmdlet) copies a block to the paste buffer. (1.5c "act on a block" was redirected onto
  quick-command + JS and delivered here.)
- **Phase 3 (TS-3a..TS-3c) — OSC 133 shell integration.** `VTermParser` recognises the semantic-prompt
  markers; the controller maps `;A/;B/;C/;D` into precise command blocks + exit codes, with
  `useOsc133Boundaries` superseding the CommitLine heuristic once markers arrive (no double-open). Opt-in
  via the `terminal.shell_integration` bootstrap (off by default — it rewrites PS1; bash DEBUG trap / zsh
  preexec+precmd hooks).
- **Phase 5 (TS-5a..TS-5d) — persistence & restore.** Scrollback survives across sessions, **clean-exit
  only**, gated on a project `.goatedit` dir (`terminal.persist_scrollback`, on by default). The bulky
  history goes to `<root>/.goatedit/terminal_scrollback.bin` — a **versioned `GTSB` binary** keeping line
  text + per-span ANSI colors (`TextBuffer::Save/LoadWithAttributes`, TS-5a); the block index (no row
  text) rides in `session.yml` as `TerminalSession`/`TerminalBlockSession` (TS-5b). `TerminalScreen` grew
  `SnapshotScrollbackTail`/`SeedScrollback` seams (TS-5c) — the snapshot re-bases intersecting blocks into
  `[0,N)`, **closes the open block at the saved tail** (live-grid tail dropped, exit unknown), and is empty
  while alt-screen; the seed replaces scrollback at base 0 and appends a fresh open loose block. The
  controller's `ToSession`/`FromSession` own the `.bin` I/O + kProject path (snapshot under `screenLock`,
  write outside); save is driven from `Editor::SaveSession`, restore runs **inside `Begin()` before
  `shell.Begin()`**. Caps 2000 on-disk / 10000 in-memory. Command history (`TerminalCmdHistory`, TS-5d)
  was already plain-text shipped.
- **Phase 4 — per-block syntax highlighting — EXTRACTED** to its own doc,
  [`syntax-blocks.md`](syntax-blocks.md) (optional, independent, not started). The only seam the shipped
  work had to preserve — the `language` field on `CommandBlock` + the persisted `TerminalBlockSession` —
  is in place. Listed under Planned / not started below.
- **Found along the way:** [`open-bugs.md`](open-bugs.md) #11 — a raw TAB byte from the pty was silently
  dropped, garbling multi-column shell output (e.g. plain `ls`); pre-existing, unrelated to this feature,
  documented but not fixed.

Detail + phase-by-phase status: [`terminal-scrollback.md`](done/terminal-scrollback.md).

---

## Alpha normalization — `ColorRGBA::a` on a single 0..1 convention ✅ (2026-06-28)

`ColorRGBA::a` is a 0..1 fraction everywhere; the lone violation was the theme's `alpha(224)` — a stray
0..255 magnitude (hand edit) that leaked into the `selection` color, overflowing the Gansi overlay blend
(the green-selection bug 10) and surviving in SDL only by an accidental Uint8 wrap. **Load-bearing
decision:** the Sublime spec says `alpha()` is 0..1 and the theme already authored every *other* alpha
0..1, so the *value* was the defect, not the reader — fix was to correct `alpha(224)` → `alpha(0.12)`
(both `colors.json`), make `ExecuteAlpha` a faithful 0..1 boundary (store verbatim + clamp + warn), and
drop both Gansi stopgaps. SDL/JS/serialization unchanged (alpha ≤ 1 ⇒ no wrap). Pinned by
`test_theme_alpha` + the updated `test_gansibackend_overlay`. Detail: [`alpha-normalization.md`](alpha-normalization.md).

## Planned / not started


- **System font fallback** (2026-11-11) - after the theme font and `sdl.font_fallback`, ask the system (CoreText /
  fontconfig) for a font with the character, behind `sdl.system_font_fallback` (default on). Prompted by open-bugs #26
  (Claude Code's `/context` grid). Detail: [`system-font-fallback.md`](system-font-fallback.md).
- **Usability round** (2026-11-08) - in this order: the change set (`git status`) marked in the workspace tree; the header's open files cut off with `...` and listed on `Cmd+E`;
  splitters dragged with the mouse (moves from SDL, mouse capture in `RootView`); workspace search: type-to-jump in the
  tree, open a file by name on `Cmd+P`, plain text search on `Cmd+Shift+F`. Noted, not scheduled: key bindings for script
  commands, so `ReformatLine` can become a quick command. Detail: [`usability-round.md`](usability-round.md).

- **Terminal emulation library `gvt`** (2026-11-09) — rewrite `VTermParser` as a Williams streaming state
  machine and move parser + screen model into a standalone `src/ext/gvt` library (like `gansi`), driven through
  `IParserSink` / `ITerminalHost` interfaces; libvterm as a test-only oracle, libghostty as reading reference
  (no Zig dependency). VT-0..VT-7 done on `feat/gvt`: parser, screen model, SGR, replies/OSC, wide chars (all six fixtures match
  libvterm), and the editor switch-over - `TerminalScreen` runs on `gvt::Terminal`, `VTermParser` is gone.
  Work packages VT-0..VT-10: [`terminal-vt.md`](terminal-vt.md). Gap analysis it builds on
  (the claude capture, TE items): [`terminal-enhancements.md`](terminal-enhancements.md).
- **Per-block syntax highlighting** — extracted from the terminal-scrollback effort (was its "Phase 4").
  Highlight a command block's output with the matching language (e.g. CMake colors over a `cmake` run) via
  a hard-region tokenize over the block's line range — a fixed start boundary on a background `Job`, no
  state bleed across blocks. Independent of the shipped terminal work; the `language` seam is already in
  place. Work packages TS-4a/4b/4c + design: [`syntax-blocks.md`](syntax-blocks.md).
- **Folder monitor** — *disabled* live FS watcher (`foldermonitor.enabled: no`). Platform analysis of
  the two backends (macOS FSEvents = OS-recursive subtree watch; Linux inotify = per-dir, non-recursive,
  watch-capped, crippled by a leftover `IN_ONESHOT`), the fundamental asymmetry that blocked a clean
  abstraction, a defect checklist, and a re-enable plan (FM-n) that converges on "rescan the affected
  subtree via the FolderScanner" instead of trusting fragile event flags. Distinct work area from the
  scanner. Detail: [`folder-monitor.md`](partially_done/folder-monitor.md).
- **CMake cleanup** — FetchContent for deps (known-working SHAs pinned), group the flat `editorsrc` into
  named source-list variables (one target, *not* separate libs), per-compiler flags, hoist deps/flags/
  packaging into `cmake/*.cmake`, and `.deb` + AppImage via CI → GitHub Releases. Plan + grounded
  inventory (incl. a broken `.deb` install rule found): [`cmake-cleanup.md`](done/cmake-cleanup.md).
