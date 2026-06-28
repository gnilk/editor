## 4. FIXED — `Workspace::ReadFolderToNode` recursively scanned ALL subdirectories — hung / crashed on build dirs

**Where:** `src/Core/Workspace.cpp`, `Workspace::ReadFolderToNode` and `Workspace::Node` (header
`src/Core/Workspace.h`).

> **FIXED (2026-06-17, branch `fix/folder-scanner`).** Resolved by the FolderScanner extraction
> ([`folder-scanner.md`](done/folder-scanner.md)), exactly as planned — not a bolt-on in `Workspace`:
> - *Build dirs ingested* → **FS-2** scan-time exclude (`FsFilter` on `Glob.h`; the list relocated off
    >   the disabled monitor to the scan-owned `workspace.exclude`). Opening a project root now prunes
    >   `cmake-build-debug` etc. before they enter the node tree.
> - *No depth limit / symlink-cycle crash* → **FS-4** `Options.maxDepth` (operational
    >   `workspace.max_scan_depth`, default 64) + `followSymlinks=false` — a cyclic link is emitted once and
    >   never descended, so the ENAMETOOLONG → `std::terminate` abort can't recur.
> - `ReadFolderToNode` is now a thin adapter that drives the scanner via a parent-`Node` stack (**FS-3**);
    >   the unbounded recursion is deleted. `ApplyFsEntry` stays the single fs→tree mutator.
>
> Regression: `test_workspace_openfolder_excludes_builddir` + the `test_folderscanner` module
> (exclude-prune / maxdepth / symlink-cycle). Commits `276a966`, `9603bcb`, `6410daa`.
> **No residual of the original symptom.** The originally-proposed `Node::isRead` was premature; lazy
> expansion for genuinely huge *legitimate* trees is tracked as the deferred **FS-6** (its own effort),
> NOT part of this bug. The analysis below is retained as historical cold-start context.
>
> real-app verified — goatedit cmake-build-debug no longer crashes
>
**What's wrong:** the recursive scan has no depth limit and no per-directory skip. Opening the project
root (`.`) with a `cmake-build-debug/` (or `_deps/`) subtree present causes the scan to descend into
thousands of generated build artefacts, FetchContent downloads, and compiler cache files. On startup
this makes the editor hang or crash before the first frame renders — there is no point caching the
complete build directory into the node tree.

A second, harder failure of the same missing guard: a **symlink cycle** in the scanned tree (e.g.
`~/ncs/.../connectedhomeip/config/beken/third_party/connectedhomeip/...` repeating) makes the recursion
build an ever-longer path until `std::filesystem::is_directory` throws `ENAMETOOLONG` — uncaught →
`std::terminate` → abort. So the fix needs **cycle detection** (e.g. `std::filesystem::canonical` +
visited-set, or `directory_options::follow_directory_symlink` left OFF), not only a depth bound.

**Discovered:** 2026-06-17 — after CMake's `FetchContent` started writing deps under `_deps/` inside
the project tree (`cmake-build-debug/_deps/`). The build directory is now a multi-thousand-file
subtree that the startup scan tries to ingest in full.

**Workaround (today):** open a specific subdirectory instead of `.` to avoid descending into build
dirs. Not suitable as a permanent state.

### How the model actually works (read before "fixing" with depth/flags)

Two phases, both **eager** — the cost is paid up front, before the first frame (matching the symptom):

1. **Scan** — `OpenFolder` → `ReadFolderToNode` recurses the *entire* subtree into the `Node` tree
   unconditionally. `ApplyFsEntry` (`Workspace.cpp:410`) is the single fs→tree mutator and is documented
   as the shared seam for both the scan and the (future) folder monitor.
2. **View build** — `WorkspaceView::PopulateTree` → `FillTreeView` walks the *whole* already-built
   `Node` tree and materialises a `TreeView` node for every entry. The `hide_dot_files` `"."` filter
   (`excludePrefixList`) is applied **here, at view-build time** — dotfiles are still scanned into the
   `Node` tree, just not displayed.

**Expansion in the WorkspaceView is purely visual.** `isExpanded` on a `TreeNodeRef` toggles visibility
of children that *already exist*; there is **no callback from expand back into the Workspace to scan on
demand**. So lazy expansion is NOT a property of the current model.

### Assessment of the originally-proposed guards

- **`maxDepth` — necessary but not sufficient.** The build-dir problem is *breadth*, not depth:
  `cmake-build-debug/_deps/<dep>-src/...` and `CMakeFiles/` hold thousands of files well within 8
  levels, so a depth cap still ingests most of a build tree. Its real value is as a hard safety bound
  against **symlink-cycle infinite recursion** — `ReadFolderToNode` recurses on `fs::is_directory(entry)`,
  which *follows symlinks*, so a cyclic link gives unbounded recursion → stack overflow (the "crash").
  Keep it for that, not as the fix.
- **`isRead` — premature; doesn't fit the current model.** As originally specified (set true on entry,
  skip if true) it only prevents redundant *full* re-scans. The "hook for lazy expansion later" is
  overstated: lazy expansion needs a *shallow* (one-level) scan mode in `ReadFolderToNode` (today it
  always recurses fully) **and** an on-expand callback from `WorkspaceView` into `Workspace` (today
  expand never calls back). `isRead` is a fine *seed* for that future but is inert until both exist —
  do NOT add it now as a no-op.

### Proper fix: exclude at scan time (fits the model; half-built already)

The offenders are build/generated directories. Exclude them **at scan time**, in/around `ApplyFsEntry`
— the infrastructure already exists:

- `config.yml` already lists the exact offenders under `foldermonitor.exclude:`
  (`.git, .idea, bld, cmake-build-debug, cmake-build-release`) + `use_git_ignore`.
- `ApplyFsEntry` is THE single fs→tree mutator shared by scan and the future monitor — an exclusion
  check there fixes both at once (the established "single chokepoint" pattern).
- `ReadFolderToNode` already does `if (node == nullptr) continue;` *before* the `is_directory`
  recursion check. So if `ApplyFsEntry` returns `nullptr` for an excluded dir, both ingestion AND
  descent are skipped for free — no new branch in the recursion.

**Wrinkle — the FolderMonitor is currently disabled and being reworked.** The scan must NOT depend on
that subsystem. The exclude *list* lives under the `foldermonitor` config section and is read via
`FolderMonitor::GetExclusionPaths()` (`FolderMonitor.cpp:40`). Share the **data, not the monitor**:
lift the exclude list to a scan-owned key (e.g. `workspaceview.exclude` / `workspace.exclude`) or read
the raw config sequence directly from the scan; the monitor reads the same key later.

**Scan-time vs view-time exclusion (deliberate choice):** scan-time exclusion means excluded dirs are
*never in memory* (correct for build dirs — never wanted). View-time exclusion (today's dotfile filter)
keeps them in memory but hidden, so a future "show hidden files" toggle works by view-rebuild without a
rescan. Recommendation: exclude build/heavy dirs at scan time; keep the dotfile hide at view time.

**When fixing (this bug):**
- Add the exclusion check in `ApplyFsEntry` (return `nullptr` for an excluded dir → ingestion + descent
  both skipped). Read the exclude list from a scan-owned config key, not from the disabled monitor.
- Add `maxDepth` (signature change on `ReadFolderToNode`; only caller is `OpenFolder` at
  `Workspace.cpp:379`) purely as a symlink-cycle / stack safety bound; default 8–10. Optionally also
  skip symlinked dirs outright. Consider a config value (`workspace.max_scan_depth`) for deep trees.
- `test_workspace`: synthetic temp tree containing a `cmake-build-debug/`-like subtree → assert those
  nodes are absent and node count is bounded; plus a depth-bound test.

**Deferred as its own effort (NOT this bug):** true lazy expansion for genuinely huge *legitimate*
trees (monorepo) — `Node::isRead` + a shallow-scan mode + an on-expand callback from `WorkspaceView`,
designed as one piece. Build output is handled by exclusion, not by lazy expansion.

---

## 5. FIXED — Open a non-active file from the project/workspace view in a restored session (where the file has been previously opened) will open a new tab with same file

**Symptom:** open a non-active file from the project/workspace view in a restored session (where the file has been previously opened) will open a new tab with same file name
resulting in a new tab with same file name.

Closing and opening the session again will now have two files with same name. The session file will also contain
two references to the same file.

**Root cause:** `Editor::OpenDocumentOrFolder` calls `Workspace::OpenFolder` (scans the folder into
path-only nodes) BEFORE `Editor::RestoreSession` runs. `Workspace::FromSession` → `ReopenDocument` →
the single-arg `NewDocumentWithFileRef(path)` always parented the new node under `GetDefaultWorkspace()`
— a separate, lazily-created "default" `ProjectRoot`, distinct from the `ProjectRoot` `OpenFolder` just
scanned for the same on-disk directory. So each restored document landed on a brand-new node under that
disconnected "default" tree, while the *displayed* (scanned) node for the same path stayed doc-less.
Selecting that displayed node in `WorkspaceView` → `Editor::OpenDocumentFromWorkspace` →
`EnsureDocumentForNode` then built a **second** `Document` for the same file (the doc-less node had
never been told one already existed) and opened it as a new tab — the duplicate session entries on
save followed from there.

**Fix:** added `Workspace::FindNodeForPath` (searches every `ProjectRoot`'s tree, via
`Node::FindNodeWithPath`, for a node already covering the absolute path) and call it first in the
single-arg `NewDocumentWithFileRef`; if a node already exists (scanned, or already open elsewhere) it's
reused — `EnsureDocumentForNode` attaches/returns its `Document` — instead of creating a duplicate.
`Node::FindNodeWithPath` itself was made to compare `lexically_normal()` forms, since an `OpenFolder(".")`
root (and everything scanned under it) carries a literal `/./` that a freshly-`absolute()`'d query path
doesn't. Also guarded `Editor::LoadDocument` to activate-not-reload when `NewDocumentWithFileRef` returns
an already-open document (mirrors the existing early-return in `OpenDocumentFromWorkspace`), so the
direct "open file" path can't clobber an open buffer's unsaved edits via the same mechanism.

**Regression test:** `test_session_workspace_reopen_reuses_scanned_node` (`utests/test_session.cpp`) —
`OpenFolder(".")`, then `ReopenDocument` a session entry for a file already scanned into the tree;
asserts no second `ProjectRoot` is created and the scanned node ends up holding the restored `Document`.
Verified failing on pre-fix code (extra root committed) before the fix, per the repo's
reproduce-before-fix discipline.

---
## 7. 'CWD to HOME when starting outside'

**WHERE:** `Editor::Initialize` (`src/Core/Editor.cpp`) — the `if (!IsWithinTree(cwd, pathHome))
{ current_path(pathHome); }` block, which runs *before* the positional file/folder args are opened
(`OpenDocumentOrFolder` further down).

**What's wrong:** It screws up quite a lot of things - we shouldn't do this - instead we should just adhere
to the rules of 'don't write session/cache data unless strictly a subfolder of users home directory'.
Concretely it broke `goatedit .` launched from outside `$HOME`: the relative `.` resolved against the
*relocated* cwd (`$HOME`), so the editor opened the whole home tree instead of the launch dir — and then
the workspace scan recursed into a symlink loop there (see bug #4) and aborted.

**Partial fix (2026-06-17):** `ParseArguments` now resolves positional args to **absolute paths at
parse time** (against the launch cwd, before any relocation), so `goatedit .` opens the directory it
was launched from regardless of the later `current_path` change. Verified: launching from `/tmp/...`
with `.` now opens that folder (not `$HOME`) and no longer hits the symlink-loop abort.

**Still open (decision):** whether to drop the cwd→`$HOME` relocation entirely (user's preference —
"we shouldn't do this"). The relocation still runs for no-arg/Finder launches; removing it needs a
look at every remaining relative-path operation (default workspace, save) + a macOS Finder test, so
it's deferred rather than done here.

**Why it (mostly) hasn't bitten yet:** We don't really test it...

## 8. FIXED — 'Reopening a session which has open files above the max_scan_depth threshold'
**Where:** Session manager and the folder scanner...
**What's Wrong:** When reopning a session with a file above the 'max_scan_depth' threshold
the folder is (in the WorkspaceView) has '-' indicated but since the folder is not scanned it can't
be mapped and ends up under the 'default' at the bottom..
**Reproduce:** Clear out any session, set 'max_scan_depth=1', start './goatedit .' then open a file in any
subfolder. Quit the editor. Restart the editor.

> **FIXED (2026-06-21).** Root cause: `OpenFolder` scans only to `max_scan_depth`, so a restored
> session file below that frontier is absent from the `Node` tree; `ReopenDocument` →
> `NewDocumentWithFileRef` → `FindNodeForPath` then misses and the file was parented under the separate
> `default` root.
>
> Fix — scan ONLY the path to the file into its owning root, respecting the user's `max_scan_depth`
> intent (we do NOT bring in the whole subtree):
> - New `FolderScanner::ScanToReference(root, refPath, opts)` — lists every directory in full (siblings
    >   become un-descended frontier nodes) but descends ONLY into ancestors of `refPath`; `maxDepth` is
    >   ignored (the reference path bounds recursion). The followed folders end up legitimately
    >   `isScanned=true`, so the WorkspaceView treats them as cached and `ScanNode`/`ClearChildren` never
    >   re-fires on them — **no change to existing scan code, no clobber of the restored subtree.**
> - New `Workspace::EnsurePathInTree` walks the already-cached ancestors down to the current frontier
    >   folder, then drives `ScanToReference` from there via the existing `ApplyFsEntry` parent-stack
    >   adapter (so cached levels are never re-listed or demoted). Hooked into the single-arg
    >   `NewDocumentWithFileRef` between the `FindNodeForPath` reuse-check and the default-root fallback, so
    >   it fixes both session restore AND a direct deep-file open.
>
> Regression: `test_session_workspace_reopen_scans_deep_file` (deep file lands under the real root, not
> `default`) + `test_folderscanner_reference` (full-listing/selective-descent/maxDepth-ignored).
> Verified failing on pre-fix code before the fix, per reproduce-before-fix discipline.

---

## 6. FIXED — `Runloop::SwapQueues` swapped the message-queue pointers non-atomically (data race vs cross-thread `PostMessage`)

**Where:** `src/Core/Runloop.cpp`, `Runloop::SwapQueues`; the `incomingQueue`/`processingQueue` pointers
in `Runloop.h`. The underlying `SafeQueue` (`src/Core/SafeQueue.h`) is itself thread-safe; the bug was in
which queue object a producer thread resolves while the main thread swaps.

**What was wrong:** the message pump double-buffers two `SafeQueue`s and, once per frame, swapped the
`incomingQueue`/`processingQueue` **raw pointers** with three plain (non-atomic) assignments on the main
thread. A background producer calling `Runloop::PostMessage` → `incomingQueue->push(...)` dereferenced
`incomingQueue` concurrently with the swap — a torn/stale pointer read could push into the queue the main
thread was simultaneously draining: undefined behaviour and lost/misrouted messages.

**Fix (2026-06-22):** made `incomingQueue`/`processingQueue` `std::atomic<MessageQueue *>`.
`SwapQueues` now does `.load()`/`.store()` on each pointer individually — every read of either pointer
(from `PostMessage` on a producer thread, or from the pump itself) now observes a fully-formed pointer
value, never a torn one. Call sites that dereference the pointer (`PostMessage`, `ProcessMessageQueue`,
`DefaultLoop`) were updated to `queue.load()->method()` since `std::atomic<T*>` has no `operator->`.

**Verified:** `goatedit` and `utests` rebuild clean; full verified-green test set (258 tests) passes
unchanged (`trun -m clipboard,document,vnav,cpplang,jsonlang,cppnumbers,linelayout,dcoverlay,layout,jsengine,workspace,terminalscreen,vtermparser,keymapping,hexprojection,bytestream,hexview,indent,session,markdown,folderscanner,treeview --sequential ./libutests.dylib`).

**Not done (separate, larger task if ever needed):** a stress test hammering `PostMessage` from N
threads concurrently with the pump under TSan/ASan — the original bug doc suggested this; deferred since
the fix removes the only torn-read mechanism and the existing producer (`TextBuffer`'s async
tokenizer/parser) is the only cross-thread caller today.

## 10. 'GansiDrawContext does not respect fg/bg colors when drawing overlays' — FIXED 2026-06-27
**Where:** GansiDrawContext::DrawLineOverlays
**What was wrong:** the cell-grid overlay path did a plain video-invert (`std::swap(cell.fg, cell.bg)`)
and ignored the application-set overlay color. `LineRender::DrawLines` points `fgColor` at the theme
`selection` color right before calling `DrawLineOverlays`, exactly as the SDL backends rely on.
**Fix:** highlight by blending the overlay color (`fgColor`) into each covered cell's BACKGROUND only
— glyph + its fg left intact so text stays readable on a grid with no alpha compositing. Covered by
`test_gansibackend_overlay`. **Update 2026-06-28:** the two stopgaps it used to carry — `if (a > 1) a
/= 255` and `a = 1.0 - a` ("invert for now") — were **removed** when bug 11 was fixed; `fgColor.A()` is
now a true 0..1 opacity used as the blend fraction directly. The test now uses a discriminating 0.25
alpha (a 0.5 mix is invert-symmetric and couldn't catch a regression of the old invert).


## 11. Theme/color alpha is not on a single 0..1 convention — FIXED 2026-06-28

`ColorRGBA::a` is meant to be a 0..1 fraction (and is everywhere except `ExecuteAlpha`, which stored its
raw argument, so the theme's `alpha(224)` leaked a 0..255 magnitude into the `selection` color). That
overflowed the Gansi overlay blend (rendered green — bug 10) and only "worked" in SDL via an accidental
integer wrap. The full diagnosis, touch-point inventory, and fix live in the deep-dive:
[`alpha-normalization.md`](alpha-normalization.md).

**Discovered:** 2026-06-27, while fixing the Gansi overlay color (bug 10) — the green selection traced
straight back to `fgColor.A()` returning 224.

**Root cause (resolved):** the value was the defect, not the reader. Sublime's `alpha()` is a 0..1
fraction (per the color-scheme spec), and the theme already authored every other alpha 0..1
(`hsla(…, 0.7)`, `hsla(…, 0.25)`); `alpha(224)` was a stray 0..255 magnitude (a hand edit). Fix:
(1) corrected the theme value `alpha(224)` → `alpha(0.12)` in both `Assets/Resources/colors.json` and
`Assets/testfiles/colors.json`; (2) made `ExecuteAlpha` a faithful 0..1 import boundary — store verbatim
+ clamp to `[0,1]` (with a warn on out-of-range, which would have caught the original 224);
  (3) removed both Gansi stopgaps. SDL/JS need no change (alpha ≤ 1 now → `AlphaAsInt ≤ 255`, no wrap).
  Pinned by `test_theme_alpha` (alpha stays 0..1, out-of-range clamps) and the updated
  `test_gansibackend_overlay`. The selection's faint look is preserved (~0.12 opacity, vs the old
  accidental ~0.125).

---


## 12. Overlays carry no color/role — every overlay is painted with the `selection` color — FIXED 2026-06-28

**Where:** `LineRender::DrawLines` (`src/Core/Editor/LineRender.cpp`) +
`*DrawContext::DrawLineOverlays` (SDL2 / SDL3 / Gansi) and `DrawContext::Overlay`.
**What was wrong:** an `Overlay` was just a covered region — no color/role of its own.
`LineRender::DrawLines` hard-coded `dc.SetFGColor(contentColors["selection"])` immediately before
`DrawLineOverlays`, so **all** overlays rendered with the theme `selection` tint. Search-result overlays
are not selections, but they inherited the selection color (tuned faint for selection) and read as too
faint.
**Fix:** `DrawContext::Overlay` now carries its OWN `ColorRGBA color` (and its alpha IS the blend
opacity). The **view** resolves theme role→color at creation — `EditorView::DrawSelectionOverlay` uses
`content.selection`, `DrawSearchResultOverlays` uses a new `content.search` (fallback to `selection` for
themes that don't define it), `TerminalView` sets its block-highlight color on the overlay. The three
backends' `DrawLineOverlay(s)` blend `overlay.color` instead of the single app-set `fgColor`, so
selection + search overlays in the same frame keep distinct tints. `LineRender` no longer sets a
selection color (the graphics layer stays theme-free; the view owns the lookup). New theme color
`search` = `color(var(blue5), alpha(0.30))` — a teal at 0.30 (vs selection's faint orange 0.12), so
matches read clearly. Pinned by the updated `test_gansibackend_overlay` (points `fgColor` at a WRONG
color to prove the backend reads `overlay.color`). Surfaced 2026-06-28 while verifying the
alpha-normalization fix (bug 11).
