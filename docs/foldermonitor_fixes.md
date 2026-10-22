# Folder monitor — fixes & proposed architecture

> Written 2026-10-22 from a cold read of `src/Core/FolderMonitor.{h,cpp}`,
> `src/Core/macOS/MacOSFolderMonitor.{h,cpp}`, `src/Core/Linux/LinuxFolderMonitor.{h,cpp}` and the consumer
> seam in `src/Core/Workspace.{h,cpp}` / `WorkspaceView.cpp`.
>
> **Builds on [`partially_done/folder-monitor.md`](partially_done/folder-monitor.md)**, which has the
> platform analysis (FSEvents vs inotify) and an 11-item defect table (FM-d1..d11). Those are not
> repeated here. §1 lists what that doc *missed* (several are on the consumer side and would break the
> feature even with perfect OS backends). §2 is the proposed architecture, §3 the implementation steps.
> The monitor is still `foldermonitor.enabled: no` (system and user config).

**Goal (from the request):** when the disk changes, reload the affected part of the workspace tree, for
every folder in the tree. Honour `workspace.exclude`, so build folders and similar never generate work.

---

## §1 — Additional defects (not in folder-monitor.md)

### Consumer side (Workspace / ProjectRoot) — these break it regardless of backend

| # | Defect | Where | Effect |
|---|---|---|---|
| C1 | **Exclusions are never applied to events.** `OnFsCreated` adds a node for any path whose parent is in the tree. | `Workspace.h` `OnFsCreated` | Running `rm -rf cmake-build-debug && cmake ...` (or `mkdir bld`) puts the excluded folder **into** the tree (its parent, the root, is in the tree). Files written deeper inside excluded dirs get "Unable to find node" error spam, one per event. |
| C2 | **Null deref on removal of an unknown path.** `OnFsRemoved` passes `FindNodeWithPath()` (possibly `nullptr`) to `cbDeleteNode`, which calls `node->GetDisplayName()` *before* `RemoveNode`'s null check. | `Workspace.cpp` `GetOrAddProjectRoot` `cbDeleteNode` | Crash on deleting any file not in the tree. Examples: a file in an excluded dir, under an unscanned frontier folder, or a temp file the OS reports late. |
| C3 | **Path identity mismatch.** The root path is `fs::absolute(folder)`, not canonical, so `goatedit .` gives `/…/editor/.`. Its `lexically_normal()` is `/…/editor/` with a **trailing slash**. FSEvents reports *canonical real paths* (symlinks resolved, e.g. `/tmp` → `/private/tmp`). | `Workspace::OpenFolder`, `Node::FindNodeWithPath` | An event for a direct child of the root has parent `/…/editor`, which ≠ `/…/editor/`, so the parent is never found and nothing is added. The same happens for any root reached through a symlink. **Probably part of the "it never worked".** |
| C4 | **Flags are trusted.** `OnMonitorEvent` picks create/remove from the flags. FSEvents flags are historical and coalesced: a later event for the same path still carries `Created`. A rename arrives as `Renamed` on both old and new path with no `Created`/`Removed`, so it goes to `OnFsChanged`, which is a no-op. `Created\|Removed` in one batch means "removed" even if the file exists now. | `Workspace.h` `OnMonitorEvent` | Renames and moves never show. Atomic-save editors (write temp + rename) and `git checkout` / `git switch` leave the tree stale or wrong. |
| C5 | **External delete of an open file closes it and silently drops unsaved edits.** `RemoveNode` calls `Editor::CloseDocument` when the node holds the *active* document. `CloseDocument` then calls `RemoveNodeIfNotOnDisk` → `RemoveNode` again; it doesn't loop, because `RemoveOpenDocument` has already cleared the active document, but the node is removed twice and the view notified twice. A *non-active* open document keeps its entry in `openDocuments` while its node leaves the tree. | `Workspace::RemoveNode`, `Editor::CloseDocument` | Data loss, plus an orphaned open-documents entry. `git stash` while editing would close your buffer. |
| C6 | **Lazy tree not respected.** Events under a frontier folder (`isScanned == false`) add a child to a partly-known folder. The lazy scan's `ClearChildren` then wipes it on expand. That is harmless for plain nodes, but `ScanNode`'s `ClearChildren` **also drops nodes that own an open `Document`** — the same hazard as C5. | `Workspace::ScanNode` | Expanding or rescanning a folder that holds an open document detaches it from the tree. (Already latent today — expanding is rare after a doc opens, but a monitor would call rescans constantly.) |
| C7 | **One full `PopulateTree()` per event.** `cbCreateNode` / `RemoveNode` each call `NotifyChangeHandler`, and the change handler rebuilds the whole view tree. | `Workspace.cpp` | A `git checkout` touching 300 files gives 300 rebuilds. |
| C8 | **The monitor is started from the view.** `WorkspaceView::PopulateTree` calls `projectRoot->StartFolderMonitor()` on every repopulate. Starting a filesystem service from a view rebuild is the wrong layer (it's idempotent, so not a bug today). | `WorkspaceView.cpp:223` | Monitoring depends on a view existing and being populated. Not testable without a view. |

### Base / lifetime

| # | Defect | Where |
|---|---|---|
| B1 | `DispatchEvent` posts a lambda capturing raw `this` (the MonitorPoint) and calls `handler` with no lock. The ProjectRoot callback likewise captures raw `this`. If either dies with a message queued, that's a use-after-free (same class as the autosave timer note). It also calls `GetRootView()` without `HasRootView()`. | `FolderMonitor.cpp:22`, `Workspace.h` `StartFolderMonitor` |
| B2 | `FolderMonitor::monitorPoints` holds a strong `Ref` to every point **forever**. Nothing removes one, so a ProjectRoot can never stop or free its monitor. | `FolderMonitor.h` `AddMonitor` |
| B3 | `EventDelegate`'s first parameter is named `globPattern` but carries a full path (cosmetic, misleading). `isRunning` is a plain bool read across threads. | `FolderMonitor.h` |
| B4 | macOS: `Start` sets `isRunning = true` even when `FSEventStreamStart` fails. `fsEventStream` isn't nulled on `Stop`. The exclusion `CFString`s leak (the array is created with `nullptr` callbacks). The `Error` log call has no `%s`. | `MacOSFolderMonitor.cpp` |
| B5 | macOS: the OS exclusion paths are `root/<pattern>`. That is wrong in two ways: a **glob** (`cmake-build-*`) is passed literally, and a **nested** match (`sub/bld`) is never excluded. Past 8 entries the call fails, and the return value is ignored, so then *nothing* is excluded. | `MacOSFolderMonitor.cpp:100-111` |
| B6 | macOS: `kFSEventStreamEventFlagMustScanSubDirs`, `UserDropped`/`KernelDropped`, and `RootChanged` are ignored. When the kernel drops events, the tree silently drifts. | trampoline |
| B7 | Linux: excluded dirs are skipped but still **walked** (`recursive_directory_iterator` without `disable_recursion_pending()`), so the whole of `cmake-build-debug/_deps` is traversed anyway. The iterator also throws on a permission error, and that terminates the thread. It reads the old `foldermonitor.exclude` key (moved to `workspace.exclude`), so in practice **no exclusion at all**. | `LinuxFolderMonitor.cpp:150` |
| B8 | Linux: it watches the *entire disk tree* eagerly, while the workspace tree is lazy. Watches are spent on folders the tree doesn't even contain (inotify watch-limit hazard). `IN_IGNORED` (a watch removed when its dir is deleted) isn't handled, so `watchers` keeps stale `wd`s that the kernel will re-use. It includes `Core/Editor.h` (layer inversion — see CLAUDE.md). | `LinuxFolderMonitor.cpp` |

---

## §2 — Proposed architecture

The core idea from folder-monitor.md §4 holds. This section makes it concrete.

> **The OS layer is a *dirty-directory signal*, not a change description.** No flags are decoded and
> nothing is inferred from them. Any event at path `P` means "directory `P.parent_path()` (and `P` if it
> is a dir) may differ from the tree". The main thread then **reconciles** those directories against
> disk, non-destructively.

This makes renames, moves, atomic saves, coalescing, dropped events and `git checkout` all the same
case. It also makes macOS and Linux identical above a ~100-line OS shim.

```
 OS thread / queue                         │  main thread (runloop)
                                           │
 FSEvents / inotify ──► MonitorPoint       │
   raw path            ├─ canonicalize     │
                       ├─ FsFilter (any    │
                       │  component vs     │
                       │  root) → drop     │
                       ├─ dirty-set insert │
                       └─ if first since   │
                          flush: post ─────┼──► ProjectRoot::FlushDirty()   (debounced ~150 ms)
                          (weak_ptr)       │      for dir in dirty (shallowest first):
                                           │        node = FindNode(dir)
                                           │        skip if !node or !node->IsScanned()
                                           │        Workspace::ReconcileFolder(node)
                                           │      one NotifyChangeHandler()  → one PopulateTree
```

### 2.1 Layers and ownership

- **`FolderMonitor` (base, `Core/`)** — OS-agnostic. It owns the filter, the canonical root, the dirty
  set (mutex) and the coalescing/post logic. The platform subclass only implements `Start/Stop` and
  calls a protected `OnRawEvent(path)` from whatever thread it lives on. The
  `kChangeFlags` → consumer path goes away. Flags may stay for logging only.
- **Delivery via an injected poster, not `RuntimeConfig::GetRootView()`.** The point gets a
  `std::function<void(std::function<void()>)> postToMain` (set by the Workspace/Editor glue). This
  follows the "lower layer never depends on a higher-level service" pattern. Tests can inject a
  synchronous poster, which makes the whole pipeline unit-testable without SDL.
- **Lifetime:** each `ProjectRoot` owns its `MonitorPoint::Ref` (drop the `monitorPoints` registry, B2).
  Posted closures capture a `weak_ptr` and bail if it's gone (B1). `ProjectRoot` must then be held by
  `shared_ptr` when the callback is registered (it already is); use `enable_shared_from_this` and
  capture `weak_from_this()`.
- **Started by the model, not the view (C8):** `Workspace::OpenFolder` starts the root's monitor when
  `foldermonitor.enabled`. `WorkspaceView` only reacts to `NotifyChangeHandler`, as it does for every
  other tree change.
- **Default root** (CWD bucket for loose files) is never monitored — unchanged.

### 2.2 Reconcile instead of mutate — `Workspace::ReconcileFolder(node)`

A **non-destructive** sibling of `ScanNode`:

1. List `node`'s directory (one level, `FsFilter` applied — reuse `FolderScanner` with `maxDepth = 1`,
   collecting names).
2. **Add** entries on disk but not in the tree, via `ApplyFsEntry` (still THE single mutator). A new dir
   arrives `isScanned = false`, a frontier, so lazy expansion populates it.
   - This also closes the Linux new-dir race (FM-d5): nothing inside a new dir needs to be caught live.
   - Exception: if the new dir is the *active folder* or an ancestor of an open document's path, scan
     it immediately.
3. **Remove** tree children no longer on disk. **Unless** the node (or any descendant) owns an *open*
   `Document`: keep it and flag it `kMetaKey_MissingOnDisk`. The view can render a marker; saving
   re-creates the file and clears the flag. **Never close a document because the disk changed (C5).**
4. **Keep** existing nodes untouched. Their `Document`, expand state and `isScanned` all survive.

**Depth: a dirty folder is reconciled ONE level deep.** Its direct entries are compared against disk.
Subfolders already in the tree are kept as they are, scanned or not; they are *not* walked from the
parent. Changes inside a scanned subfolder produce their own events (FSEvents is recursive; Linux has a
watch per folder), so that subfolder gets dirty itself. The one exception is a "lost events" signal
(FSEvents `MustScanSubDirs` / `Dropped`, inotify `IN_Q_OVERFLOW`). Then every **scanned** folder under
the affected root is reconciled, recursively, one level each. Unscanned frontiers are still skipped.

`ScanNode` should then use the same keep-documents rule instead of `ClearChildren` (fixes C6). The
cleanest way is to implement `ScanNode` as "reconcile + mark scanned".

Content changes to an open file (`OnFsChanged`, "reload/dirty on disk") stay **out of scope**. Only
record the hook: a dirty *file* path whose node has an open document is where a future
"changed on disk" flag goes.

### 2.3 Exclusion — one matcher, applied per path component

- **Authoritative filter (user-land, both OSes):** drop an event if **any component** of
  `path.lexically_relative(canonicalRoot)` matches `FsFilter` (the same `workspace.exclude` globs the
  scan uses). The check must be per component, not per filename: `cmake-build-debug/x/y.o` has filename
  `y.o`, but component `cmake-build-debug` matches. Add `FsFilter::IsExcludedRelative(relPath)` with a
  unit test.
- **Hidden dot-files** stay a *view* concern (`workspaceview.hide_dot_files`); the monitor still tracks
  them. `.git` is in `workspace.exclude` by default, so git's internal churn is dropped at the filter.
- **OS-level exclusion is only an optimisation:**
  - **Linux:** never add a watch on an excluded dir (prune with `disable_recursion_pending()`, B7).
    Better still (§3 step 6), watch exactly the *scanned folder nodes* of the tree. The watch set then
    equals what the tree shows, excluded dirs are never in it, and the watch count stays bounded by the
    lazy tree.
  - **macOS:** compute *concrete* exclusion paths by matching the patterns against the root's existing
    top-level entries (handles globs), and cap at 8 (check the return value). Everything else is caught
    by the user-land filter. Optional — FSEvents for a filtered build dir is just wasted wake-ups.
- `.gitignore` (`use_git_ignore`) stays a later, separate item (FM-6).

### 2.4 Path identity (C3)

- Store the root as **`fs::weakly_canonical(folder)`** in `OpenFolder` (no trailing `.`, symlinks
  resolved). Every child path is built from it, so the tree is canonical throughout.
- In `OnRawEvent`, canonicalize the event path's *parent* with `weakly_canonical` (a removed file can't
  be canonicalized, its parent usually can). Then derive the relative path. An event outside the root
  is dropped.
- This also helps `FindNodeWithPath` generally. The `"."`-segment workaround comment in it becomes
  obsolete.
- ⚠ Check the session cache (it stores paths) and `GetWorkspaceNodeForActiveDocument` for anything
  relying on the non-canonical form — a quick grep at implementation time.

### 2.5 OS backends after the change

- **macOS:**
  - Use a **private serial dispatch queue** (`dispatch_queue_create`) instead of the main queue. This
    removes the "is the main GCD queue pumped under SDL3?" question (FM-d6) entirely. The callback then
    only does `OnRawEvent` (thread-safe), and the base posts to the runloop. The runloop picks it up
    within ≤ 250 ms (`SDL_WaitEventTimeout`).
  - Set latency to ~0.3 s.
  - Set the flags to `FileEvents | NoDefer | WatchRoot`. `IgnoreSelf` is optional: our own saves are
    harmless because reconcile is idempotent.
  - On `MustScanSubDirs`, `Dropped` or `RootChanged`, mark the root dirty so the whole loaded tree is
    reconciled.
- **Linux:**
  - **No `IN_ONESHOT`** (FM-d1).
  - Mask: `IN_CREATE|IN_DELETE|IN_MOVED_FROM|IN_MOVED_TO|IN_DELETE_SELF|IN_MOVE_SELF|IN_ONLYDIR`.
    `IN_CLOSE_WRITE` is not needed for the tree, only for the future "changed on disk" hook.
  - Every event → `OnRawEvent(dir/name)`; the move cookie isn't needed (reconcile).
  - On `IN_IGNORED`, drop the `wd`.
  - On `IN_Q_OVERFLOW`, mark the root dirty.
  - Remove the `printf` and the `Editor.h` include.

---

## §3 — Implementation steps (each its own commit, tests first where possible)

| # | Step | Test (reproduce-first where it's a bug) |
|---|---|---|
| 1 ✅ | **Canonical root** in `OpenFolder` (C3). | `test_workspace`: `OpenFolder(".")`-style path with a trailing `.` (and, on macOS, a root under `/tmp`). `FindNodeWithPath(canonical child)` finds the node — fails today. |
| 2 ✅ | **Null-safe removal** (C2): `cbDeleteNode` checks `node`; `RemoveNode` never re-enters `CloseDocument` (C5 interim: a node with an open document is flagged, not removed or closed). | Remove-unknown-path no longer crashes. Removing an open doc's file keeps the doc open and in `openDocuments`. |
| 2b ✅ | **Missing-on-disk marker** (§4.1): status-line prefix first; then a `TreeView` per-item style delegate, plus `kStrikethrough` in SDL2 and SDL3 (or a theme colour). | Unit: a flagged node reports missing, and a save clears the flag. Marker rendering is checked in the GUI. |
| 3 | **`FsFilter::IsExcludedRelative`** (per component). | `test_folderscanner` / new `test_fsfilter` cases: `cmake-build-debug/a/b.o` excluded, `src/bld.cpp` not excluded by the `bld` pattern, glob `cmake-build-*`. |
| 4 | **`Workspace::ReconcileFolder`** + `ScanNode` built on it (C4, C6). | Temp tree: add / remove / rename a file and a dir on disk, then reconcile. The tree matches disk; a node owning a Document survives removal (flagged); a new subdir arrives `isScanned == false`; excluded dirs never appear. **This is the bulk of the correctness and is fully OS-independent.** |
| 5 | **Base rework**: injected `postToMain`, dirty set + coalescing, weak-ptr lifetime, filter in `OnRawEvent`, drop the `monitorPoints` registry and flag decoding (B1–B3, C1, C7). ProjectRoot gets `FlushDirty`; start moves to `OpenFolder` (C8). | A fake `MonitorPoint` subclass, test-driven via `OnRawEvent` with a synchronous poster. N events in one folder → one reconcile + one change notification; events under an excluded dir → zero; an event after the ProjectRoot is gone → no crash. |
| 6 | **macOS backend** per §2.5 (B4–B6, FM-d6..d10). | GUI/manual on the Mac: `touch`, `rm`, `mv a b`, `mkdir x && touch x/y`, `git switch`, and a rebuild in `cmake-build-debug` (must cause no tree activity). Optional integration test, gated on platform, with a real stream + temp dir + polling for ≤ 2 s. |
| 7 | **Linux backend** per §2.5 (FM-d1..d5, B7, B8). Simple version first: walk the tree with the filter (pruned) and watch every non-excluded dir. Later, optionally: watch-set = scanned folder nodes (`Workspace` tells the monitor on scan/remove). | Same manual list on the Linux box; check the watch count (`ls /proc/<pid>/fdinfo` → inotify lines). |
| 8 | Flip `foldermonitor.enabled` default (system config only). Update `folder-monitor.md` §0 and `work-log.md`; add the new test modules to the CLAUDE.md verified-green set. | Full verified-green set. |

**Done (branch `fix/foldermonitor-prep`):**
- **Step 1:** `pathutil::CanonicalPath` is used by `OpenFolder`, the default root, `FindNodeForPath`,
  `EnsurePathInTree` and `NewDocument`. Tests: `test_workspace_canonicalroot`; older workspace tests
  now build their temp roots canonically.
- **Step 2:** `Workspace::RemoveNodeGoneFromDisk` and `Node::kMetaKey_MissingOnDisk`.
  - The monitor's delete callback is null-safe and routes through it.
  - `OnFsCreated` clears the flag when the path comes back.
  - `Node::SaveData` re-creates a missing file (force-writes even an unchanged buffer, creates its
    folders) and clears the flags upwards.
  - Test: `test_workspace_gonefromdisk`.
  - `RemoveNode` itself is unchanged; the monitor path just never reaches its `CloseDocument`, because
    open documents are kept.

- **Step 2b:** the marker.
  - `kTextAttributes::kStrikethrough` is drawn as a line at 3/4 of the baseline, in SDL3 and SDL2. The
    SDL2 change is the same as the SDL3 one but wasn't compiled on the Mac.
  - The gansi backend maps it to `Dim`, since gansi has no SGR 9.
  - `TreeView::SetStyleDelegate` adds per-item attributes; `WorkspaceView` strikes flagged nodes.
  - `EditorView`'s status line shows `[deleted] `.
  - Verified in the GUI only; there is no draw-capture test infrastructure for the SDL path.

Steps 1–5 are platform-independent and unit-testable. They give a working feature on the Mac as soon as
step 6 is done. Step 7 only needs the Linux box.

---

## §4 — Decisions (2026-10-22)

Context, confirmed by the author: the workspace tree was refactored recently for lazy traversal
(unscanned/frontier directories, FS-6). The monitor predates that refactor, so a large part of the breakage
is the mismatch between the two. The design above (reconcile only *scanned* folders, new dirs arrive
as frontiers) is meant to close exactly that gap.

1. **External delete of an open document → keep the buffer, mark it.** The node stays in the tree with
   `kMetaKey_MissingOnDisk`, the document stays open, and saving re-creates the file and clears the flag.
   Marker, using what exists today:
   - **Status line:** `EditorView` already prefixes `* ` (modified) and `R/O ` in the status center
     (`EditorView.cpp:290`). Add a `[deleted] ` (or similar) prefix the same way. This is cheap and
     needs no new rendering.
   - **Workspace tree:** `TreeView` draws every item with fixed attributes (`kNormal` / `kInverted`)
     and only has a to-string delegate. Add an optional per-item style delegate
     (`T → {kTextAttributes, color key}`), so `WorkspaceView` can colour or strike a missing node.
   - **Strikethrough:** not supported today. `kTextAttributes` has `kUnderline` (16), drawn in
     `SDLDrawContext` as a line at `baseline + margin`. A `kStrikethrough = 32` is the same mechanism
     at about half the glyph height. That's a small change, but it has to go into **both** SDL2 and SDL3
     draw contexts. A theme colour (e.g. `ui.missing_on_disk`) is the fallback if strikethrough looks
     wrong.
2. **Linux watch scope → every non-excluded dir under the root** (pruned walk). Watching only the
   scanned folders is a later optimisation, if the watch count ever matters.
3. **New directory → lazy frontier** (`isScanned == false`), populated on expand like any other.
4. **Changed-on-disk for open files → deferred.** Leave the `OnFsChanged` hook as the extension point.
   It likely wants a "file changed on disk — reload?" prompt, so it depends on the half-done modal
   dialogs work.
