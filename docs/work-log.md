# Work log

Short, reverse-chronological index of completed feature efforts. Each entry summarises *what shipped*
and *the load-bearing decisions*; the linked detail doc carries the full cold-start history. Planned /
not-yet-started efforts are listed at the bottom. For known-wrong code deliberately left unfixed, see
[`open-bugs.md`](open-bugs.md) (the active tracker, not logged here). For work explicitly deferred /
left out of scope from shipped features, see [`deferred.md`](deferred.md).

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

## Terminal scrollback + command blocks — Phases 0–3 + 5 ✅ (Phase 4 deferred)

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
- **Phase 4 — per-block language / hard-region highlighting — DEFERRED** (independent of everything
  above; see [`deferred.md`](deferred.md)). Give a block its own highlighter (e.g. CMake colors over a
  `cmake` run) without disturbing the surrounding ANSI history: TS-4a command→language map (storage already
  on `CommandBlock` + `TerminalBlockSession`), TS-4b hard-region tokenize on a background `Job`, TS-4c
  block-boundary isolation.
- **Found along the way:** [`open-bugs.md`](open-bugs.md) #11 — a raw TAB byte from the pty was silently
  dropped, garbling multi-column shell output (e.g. plain `ls`); pre-existing, unrelated to this feature,
  documented but not fixed.

Detail + phase-by-phase status: [`terminal-scrollback.md`](done/terminal-scrollback.md).

---

## Planned / not started

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
