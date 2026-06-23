# Folder monitor — platform analysis (macOS FSEvents vs Linux inotify)

> **Analysis + planning doc.** Started 2026-06-17. The live folder monitor is currently **disabled**
> (`foldermonitor.enabled: no`) and has been for a long time because it never worked reliably across
> both platforms. This captures, from a cold read of the current code, *what each platform backend
> actually does*, *how they fundamentally differ*, *why it's flaky*, and a sequenced plan for re-enabling
> it. Kept separate from [`folder-scanner.md`](../done/folder-scanner.md) on purpose — scanning (one-shot walk)
> and monitoring (live watch) are distinct work areas that happen to share a matcher and a node-builder.
>
> **Files analysed:** `src/Core/FolderMonitor.{h,cpp}` (base), `src/Core/macOS/MacOSFolderMonitor.{h,cpp}`,
> `src/Core/Linux/LinuxFolderMonitor.{h,cpp}`, plus the consumer seam in `src/Core/Workspace.{h,cpp}`.

---

## §0 — Status / read this first

**Phase: DISABLED — analysis only, no re-enable scheduled.** `config.yml:50` ships
`foldermonitor.enabled: no`. The seam is wired end-to-end (`ProjectRoot::StartFolderMonitor` →
`OnFs*` → `funcCreateNode`/`funcDeleteNode` → `ApplyFsEntry`) but the gate at
`WorkspaceView.cpp:187` keeps it off.

**The one thing to internalise:** the two backends are not "the same idea on two APIs" — they have
**fundamentally different recursion and event models** (§4), and that asymmetry is the root reason a
single clean abstraction never settled. macOS (FSEvents) gives you a recursive subtree watch for free;
Linux (inotify) gives you a per-directory, non-recursive, watch-capped primitive that you must
enumerate, arm, and re-arm yourself.

| Concern | macOS (FSEvents) | Linux (inotify) |
|---|---|---|
| Recursion | OS-recursive (one stream, whole subtree) | **NOT recursive** — one watch per directory, manual enumeration |
| Enumeration cost | none (OS walks) | full recursive walk to add watches (same build-dir hazard as #4) |
| Scale ceiling | effectively none | `fs.inotify.max_user_watches` (per-dir watches) |
| Event delivery | main dispatch queue, batched, **3.0s latency** | dedicated poll thread, immediate |
| Event richness | path + coalesced flags (flags "flawed" for moves) | wd + name + precise mask (but see ONESHOT bug) |
| Exclusion | OS-level `FSEventStreamSetExclusionPaths` (**8-path cap**) + root-name reject | user-land substring filter during enumeration (uncapped) |
| Renames/moves | partial (`kRenamed` flag, ambiguous) | **unhandled** (only `IN_CREATE`/`IN_DELETE`) |

**Relationship to other docs:**
- [`folder-scanner.md`](../done/folder-scanner.md) — the Linux backend's `ScanForDirectories` is itself an
  unbounded recursive walk; it must reuse the same `FsFilter` (exclude) + bound as the node scan. And
  *both* backends need the scanner to populate a subtree when a directory appears (FS-5). See §6.
- [`open-bugs.md`](../open-bugs.md) #4 — the Linux watch-enumeration shares the build-dir hang hazard.

---

## §1 — Shared architecture (the base)

`FolderMonitor` (factory) + `FolderMonitor::MonitorPoint` (one watched root). Platform subclasses
override `CreateMonitorPoint` + `MonitorPoint::Start/Stop`.

- **`kChangeFlags`** (`FolderMonitor.h:29`): `kNone/kCreated/kModified/kRemoved/kRenamed`, a `uint8_t`
  bitset. **`EventDelegate`** = `void(const std::string &path, kChangeFlags)`.
- **`MonitorPoint`** base (`FolderMonitor.h:38`): `pathToMonitor`, `pathsToExclude`, `handler`,
  `dataGuard` mutex, `isRunning`. `SetExcludePaths` refuses while running and is mutex-guarded.
- **`DispatchEvent`** (`FolderMonitor.cpp:22`): the single thread-hop — re-posts the event onto the main
  thread via `RuntimeConfig::Instance().GetRootView().PostMessage(...)`, so the consumer's tree mutation
  always runs on the UI thread regardless of which thread/queue the OS delivered on.
- **`GetExclusionPaths`** (`FolderMonitor.cpp:40`): reads `foldermonitor.exclude` each call; the
  `use_git_ignore` branch is an **unimplemented TODO** (`.gitignore` is never parsed).
- **Platform selection is compile-time** (`RuntimeConfig.h:120`): `#ifdef GEDIT_MACOS` →
  `MacOSFolderMonitor`, `GEDIT_LINUX` → `LinuxFolderMonitor`, stored **by value** (the in-code comment
  notes "these should all be ref's").
- **Consumer seam** (`Workspace.h`): `ProjectRoot::StartFolderMonitor` creates the point with a callback
  → `OnMonitorEvent` decodes flags → `OnFsCreated/OnFsRemoved/OnFsChanged` → the `funcCreateNode` /
  `funcDeleteNode` delegates → `ApplyFsEntry`. `OnFsChanged` is a no-op placeholder.

---

## §2 — macOS backend (FSEvents / CoreServices)

`MacOSFolderMonitorPoint::Start` (`MacOSFolderMonitor.cpp:62`) builds an `FSEventStreamCreate` over a
1-element path array with `kFSEventStreamCreateFlagFileEvents`, latency **3.0s**, attaches it to
`dispatch_get_main_queue()`, and starts it. A C trampoline (`glbFSNotifyTrampoline`, L166) receives a
**batch** of `(path, flags)` and translates `kFSEventStreamEventFlagItem{Created,Modified,Removed,Renamed}`
→ `kChangeFlags`, then calls `OnFSEvent` per entry → `DispatchEvent`.

**Characteristics**
- **Recursive for free** — one stream covers the whole subtree under the root; no per-dir enumeration.
- **Delivered on the main dispatch queue** — so the callback already runs on the main thread; the extra
  `DispatchEvent`→`PostMessage` hop is then redundant *on macOS* but harmless (kept for cross-platform
  parity). **Reliability risk:** this depends on something actually *pumping* the main dispatch queue.
  Under the SDL3 runloop this is the prime suspect for "callbacks never fire" — if SDL's loop doesn't
  service `dispatch_get_main_queue()`, FSEvents is silent. **Verify before re-enabling.**
- **Exclusion is OS-level** via `FSEventStreamSetExclusionPaths` (L110), built by joining each exclude
  name onto the root path. Two issues, both flagged in-code:
  - macOS caps the exclusion list at **8 paths** (FIXME L100); the overflow must be filtered in
    user-land — there's a placeholder TODO in `OnFSEvent` (L143) but **no user-land filter exists yet**.
  - `CreateMonitorPoint` (L35) *rejects the whole monitor* if the **root folder's own name** matches an
    exclude entry — a surprising side effect (open a folder literally named `bld` → no monitoring).

**Defects / cruft observed**
- `SetExcludePaths` is called **twice** in `CreateMonitorPoint` (L42 *and* L44) — duplicate line.
- Change flags are "very flawed" for moves (TODO L142): `mv a b` yields ambiguous create/remove/rename
  — the in-code suggestion is to **rescan the affected folder** rather than trust the flags (this is the
  FS-5 scanner-rooted-at-a-dir reuse).
- 3.0s latency makes the UI feel laggy; events are coalesced (you get "something under X changed", not a
  precise per-file stream).

---

## §3 — Linux backend (inotify)

`LinuxFolderMonitorPoint::Start` (`LinuxFolderMonitor.cpp:35`) opens `inotify_init1(IN_CLOEXEC|IN_NONBLOCK)`
and spawns a dedicated thread `ScanThread` (named `LnxFldMon`). The thread:
1. `ScanForDirectories` — `recursive_directory_iterator` over the root, skipping excluded dirs, calling
   `AddMonitorItem` per directory (L150).
2. `StartWatchers` — `inotify_add_watch` per not-yet-watched dir (L185).
3. `poll()` loop (`GEDIT_DEFAULT_POLL_TMO_MS`) → `read` inotify events → `ProcessEvent` each.

**Characteristics**
- **inotify is NOT recursive** — every subdirectory needs its own watch, so the monitor must walk the
  whole tree to arm watches. This is the **same unbounded-walk hazard as `open-bugs.md` #4**: on a tree
  with a `cmake-build-debug/_deps/` subtree it both pays the full walk and can blow past
  `fs.inotify.max_user_watches`.
- **Exclusion** is read directly from `foldermonitor.exclude` inside `ScanForDirectories` (L155) and
  matched by **substring** (`path.string().find(f) != npos`) — recursive-aware (good) but crude (e.g.
  `bld` matches anywhere in any path component). Note it **ignores the base `pathsToExclude` member**
  that macOS uses — the two backends plumb exclusion completely differently.

**Defects observed (these are likely the core "doesn't work reliably" causes)**
- **`IN_ONESHOT` (L193).** Watches are armed with `IN_ONESHOT | IN_CREATE | IN_DELETE`, so each watch
  **auto-removes after a single event**. Re-arming only happens on `IN_CREATE` (L140 → `StartWatchers`),
  and only for *newly added* items (items keep `wd >= 0` after the oneshot fires, so `StartWatchers`
  skips re-arming them — L189). Net effect: **after the first event in a directory, that directory stops
  being monitored.** This alone makes the Linux monitor effectively one-shot per folder.
- **Renames/moves unhandled.** `ProcessEvent` only acts on `IN_CREATE`/`IN_DELETE` (L129); no
  `IN_MOVED_FROM`/`IN_MOVED_TO` (which arrive as a cookie pair), no `IN_MODIFY`. A rename looks like
  nothing happened.
- **`printf("mask: 0x%x\n", ...)` left in `ProcessEvent` (L127)** — raw stdout debug spew, not the logger.
- **Race on new directories** — a dir created at runtime is watched only after `AddMonitorItem` +
  `StartWatchers`; entries created inside it *before* the watch lands are missed.
- `Stop` notes "FIXME: Clean up watchers" (L68); individual watches aren't removed (the `close(fd)` in
  `ScanThread` reclaims them, but only on thread exit).
- `CreateMonitorPoint` does **not** check `IsEnabled()` (macOS does, L29) — asymmetric gating (the real
  gate is the caller in `WorkspaceView`, so this is latent, not active).

---

## §4 — The platform asymmetry (the crux)

The reason a clean shared abstraction never settled:

1. **Recursion model.** FSEvents: subtree-recursive, one watch. inotify: per-directory, you build and
   maintain the watch set. Any unified API must either (a) hide enumeration behind the interface (cheap
   on macOS, expensive + capped on Linux), or (b) expose "watch this one dir" and let a higher layer do
   recursion (then macOS wastes its native recursion). Today each backend just does its own thing.
2. **Event semantics.** macOS: coalesced, latency-delayed, flags that can't cleanly express a move →
   "rescan the folder" is the honest response. inotify: precise and immediate, moves *are* expressible
   (cookie pairs) — but the current code throws that away and is crippled by `IN_ONESHOT`.
3. **Delivery/threading.** macOS: main dispatch queue (no thread; fragile under SDL's loop). Linux:
   dedicated poll thread. Both converge through `DispatchEvent`→`PostMessage` onto the UI thread — that
   convergence point is the one part that *is* clean and should stay.
4. **Exclusion.** macOS: OS-level, 8-path cap, no user-land fallback, plus a root-name reject. Linux:
   user-land substring during enumeration, uncapped. Neither honors `.gitignore`. These should collapse
   onto **one** matcher (§6).

A defensible target model: **treat the OS layer as a raw event source ("something changed at path P")
and do recursion + filtering + coalescing in a shared, platform-independent layer** — relying on the OS
recursion only on macOS as an optimization, and accepting "rescan the affected directory via the
FolderScanner" as the canonical response to ambiguous/coalesced events (which also sidesteps the
flawed-flags and missed-move problems on both platforms).

---

## §5 — Known defects (checklist for any re-enable)

| # | Platform | Defect | Where |
|---|---|---|---|
| FM-d1 | Linux | `IN_ONESHOT` → monitoring dies after first event per dir; consumed watches never re-armed | `LinuxFolderMonitor.cpp:193,189` |
| FM-d2 | Linux | renames/moves + modifies unhandled (`IN_CREATE`/`IN_DELETE` only) | `:129` |
| FM-d3 | Linux | leftover `printf` debug spew | `:127` |
| FM-d4 | Linux | unbounded recursive watch enumeration (build-dir / watch-cap hazard) | `:150,158` |
| FM-d5 | Linux | race: contents created in a new dir before its watch is armed are missed | `:140` |
| FM-d6 | macOS | depends on the main dispatch queue being pumped under SDL3 (may never fire) | `:98,114` |
| FM-d7 | macOS | 8-path OS exclusion cap with no user-land overflow filter | `:100,143` |
| FM-d8 | macOS | duplicate `SetExcludePaths` call | `:42,44` |
| FM-d9 | macOS | root-name in exclude list silently rejects the whole monitor | `:35` |
| FM-d10 | macOS | coalesced flags can't express moves cleanly (3.0s latency too) | `:142,77` |
| FM-d11 | both | exclusion plumbed two different ways; `.gitignore` (`use_git_ignore`) never implemented | `FolderMonitor.cpp:42` |

---

## §6 — Relationship to the FolderScanner

The monitor and the scanner ([`folder-scanner.md`](../done/folder-scanner.md)) are distinct (live watch vs
one-shot walk) but should **share two things, never the subsystem**:

1. **The exclude matcher (`FsFilter`).** The Linux watch enumeration (FM-d4) and the node scan have the
   identical "don't descend into build dirs" need. One `Glob`-based matcher, fed from one scan-owned
   config key (FS-2), gates both — so a path the scan skips is a path Linux never watches.
2. **The scanner as the "rescan a directory" primitive.** Both backends' honest response to an
   ambiguous/coalesced event (macOS FM-d10, and the Linux new-dir race FM-d5) is *rescan the affected
   subtree* — i.e. run the `FolderScanner` rooted at that directory and diff against the node tree. This
   is exactly FS-5 ("scan rooted at a newly-created directory"). It also makes move/rename handling fall
   out for free (rescan parent → adds/removals reconcile) instead of decoding fragile flag pairs.

What the monitor does **not** borrow from the scanner: the live OS watch primitives (FSEvents stream /
inotify fds) stay platform-specific — only the *post-event reconciliation* and *filtering* are shared.

---

## §7 — Sequenced work items (when re-enabling)

Each: **Goal / Scope / Effort·Risk / Done-when / Depends-on.** All deferred until someone takes the
monitor re-enable on; listed so the analysis isn't lost.

### FM-1 — Decide the target model (spike, not code)
- **Goal:** commit to "OS = raw event source; shared layer does recursion + filter + rescan-on-event".
- **Scope:** confirm the macOS main-dispatch-queue-under-SDL3 question first (FM-d6) — it gates whether
  FSEvents is even viable as-is or needs a dedicated CFRunLoop thread.
- **Effort·Risk:** S · — (investigation). **Done-when:** the threading/delivery model is chosen and
  written here. **Depends-on:** —

> **Threading model is already decided** — see [`folder-scanner.md`](../done/folder-scanner.md) §7: background
> thread is a pure producer that **batch-posts plain-data events** through the existing thread-safe
> message pump; the `Node` tree is mutated only on the main thread (no tree lock). **Hard prerequisite:**
> the un-synchronised `Runloop::SwapQueues` ([`open-bugs.md`](../open-bugs.md) #6) must be fixed before this
> continuous background producer is enabled.

### FM-2 — Linux: fix the watch lifecycle
- **Goal:** monitoring survives past the first event.
- **Scope:** drop `IN_ONESHOT` (or systematically re-arm); handle `IN_MOVED_FROM/TO` + `IN_MODIFY`;
  remove the `printf`; close the new-dir race.
- **Effort·Risk:** M · medium. **Done-when:** create/delete/rename under a watched tree reliably
  produce events over many operations. **Depends-on:** FM-1.

### FM-3 — Shared `FsFilter` + bounded enumeration
- **Goal:** Linux watch enumeration reuses the scanner's exclude matcher + bound (kills FM-d4).
- **Scope:** route `ScanForDirectories` through the shared filter; one config key for both.
- **Effort·Risk:** S · low. **Done-when:** opening a root with a build subtree adds no watches for it.
- **Depends-on:** FolderScanner FS-2.

### FM-4 — macOS: exclusion + flag honesty
- **Goal:** correct exclusion past 8 paths; stop trusting move flags.
- **Scope:** user-land overflow filter (FM-d7); drop the duplicate `SetExcludePaths`; reconsider the
  root-name reject (FM-d9); adopt rescan-on-ambiguous-event.
- **Effort·Risk:** M · medium. **Done-when:** >8 excludes work; a `mv` reconciles correctly via rescan.
- **Depends-on:** FM-1, FolderScanner FS-5.

### FM-5 — Unified rescan-on-event via FolderScanner
- **Goal:** both backends respond to a directory event by rescanning that subtree and diffing the node
  tree (replaces fragile per-flag tree mutation).
- **Scope:** wire `OnFsCreated`(dir) → `FolderScanner` rooted there → diff/apply; the move case falls out.
- **Effort·Risk:** M · medium. **Done-when:** create/move/delete of files *and* directories reconcile on
  both platforms with the monitor enabled. **Depends-on:** FM-2, FM-4, FolderScanner FS-5.

### FM-6 — `.gitignore` honoring (optional)
- **Goal:** implement the `use_git_ignore` TODO so exclusion follows the repo's own ignore rules.
- **Effort·Risk:** M · low. **Done-when:** ignored paths are neither scanned nor watched.
- **Depends-on:** FM-3.

### FM-7 — Re-enable + verify
- **Goal:** flip `foldermonitor.enabled: yes` by default once stable.
- **Scope:** GUI verification on both dev boxes (create/delete/rename files + dirs while the workspace
  view is open); watch-count sanity on Linux. **Depends-on:** FM-2..FM-5.
