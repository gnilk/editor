# Folder scanner — extract the filesystem walk out of Workspace

> **Planning doc.** Started 2026-06-17. The recursive filesystem walk lives inside `Workspace`
> (`ReadFolderToNode`) welded to `Workspace::Node`; complexity is growing (depth bounds, exclude
> filtering, future lazy expansion, monitor reuse) and none of it is `Workspace`'s concern. This logs
> the analysis + a sequenced plan to extract a self-contained, testable `FolderScanner`. Same shape as
> [`cmake-cleanup.md`](cmake-cleanup.md) / [`session-cache.md`](session-cache.md): §0 status, analysis,
> then sequenced work items (FS-n) with Goal/Scope/Approach/Effort-Risk/Done-when/Depends-on.
>
> **Directly subsumes most of [`open-bugs.md`](open-bugs.md) #4** (`ReadFolderToNode` ingests build
> dirs) — exclude + depth-bound land here as scanner concerns rather than bolt-ons in `Workspace`. See
> §10.

---

## §0 — Status / read this first

**Phase: COMPLETE on `fix/folder-scanner`** (W1–W6 landed; FS-5/FS-6 remain deferred — see §0.1).
The in-scope scanner extraction shipped and resolved [`open-bugs.md`](open-bugs.md) #4 end to end.

| Item | Status | Commit |
|------|--------|--------|
| FS-1 — extract pure `FolderScanner` (traversal → plain-data events) | ✅ done (W1; compiles standalone, event-stream test green) | — |
| FS-2 — shared exclude/glob matcher; relocate exclude list off `foldermonitor` | ✅ done (W4: `FsFilter` on `Glob.h`; exclude moved `foldermonitor`→`workspace`; prune tests green) | — |
| FS-3 — rewire `Workspace::OpenFolder` onto the scanner; delete in-Workspace recursion | ✅ done (W5: `ReadFolderToNode` now drives the scanner + parent stack; recursion deleted; `test_workspace` regression green) | — |
| FS-4 — `maxDepth` + symlink-cycle guard as scanner `Options` | ✅ done (W1 `maxDepth` + W3 symlink no-follow; depth/cycle/follow tests green) | — |
| FS-5 — monitor reuse: scan rooted at a newly-created directory | 🔲 deferred (monitor disabled) | — |
| FS-6 — lazy expansion: shallow scan + `onEnterDir` veto + `Node::isRead` + on-expand callback | 🔲 deferred (own effort) | — |
| FS-7 — scanner unit tests (event stream / exclude prune / depth bound / cycle) | ✅ done (`test_folderscanner` 10 cases green; added to the verified-green set, 247 tests) | — |

**Drivers (user-stated):** (1) the walk originated in `Workspace` but doesn't *belong* there as
complexity grows; (2) make it a **self-contained, testable** unit; (3) make it **reusable by the folder
monitoring system**; (4) decouple traversal from tree-building via **callbacks + flags** on item
discovery, NOT a template on the parent-node type; (5) leave a seam for node change-hooks / dirty
marking (without folding that concern *into* the scanner).

**Relationship to other docs:**
- **`open-bugs.md` #4** — the build-dir hang/crash. Exclude-at-scan + the depth bound are implemented
  here (FS-2/FS-4), so #4 is mostly *superseded*; re-evaluate the residual once the scanner lands (§11).
- The folder monitor is currently **disabled** (`foldermonitor.enabled: no`) and being reworked. This
  doc does NOT depend on the monitor working; it shares *data and a matcher* with it, never the
  subsystem (§6).

### §0.1 — Branch work-log (`fix/folder-scanner`)

Sequenced commit plan for this branch. In scope: FS-1/2/3/4/7. Deferred (NOT this branch): FS-5
(monitor disabled), FS-6 (own effort). Order is dependency-driven: FS-1 → FS-4 → FS-2 → FS-3 → close.
`open-bugs.md` #4 is not a separate task — its two halves land in FS-4 (crash/symlink) + FS-2
(build-dir) and the branch closes it (§11). #6 (`SwapQueues` race) gates only the *continuous*
producer (FS-5, §7); the rewired `OpenFolder` scan stays **synchronous/main-thread** so #6 stays out.

| # | One-liner | Refs |
|---|-----------|------|
| W1 ✅ | Add `src/Core/FileSystem/FolderScanner.{h,cpp}`: `FsEntryKind` + `Options` + enter/file/leave+depth hooks; recurse internally, emit plain-data events. Source-list add, no new target. Uses non-throwing `std::error_code` fs overloads; honours `Options.maxDepth` from the start (neuters #4's ENAMETOOLONG by bounding path growth). | FS-1, §3/§9 |
| W2 ✅ | Add `test_folderscanner` (registered in verified-green set at W6): records events via lambdas over a temp tree; asserts structure (counts/depth/child-between-enter-leave/veto), not sibling order. No `Workspace`/singleton. | FS-7, §3/§4 |
| W3 ✅ | Symlink no-follow guard (`maxDepth` already in W1): a symlinked dir is emitted via `onEnterDir` but only descended when `followSymlinks`. Tests: maxDepth bound, symlink-cycle terminates (count==2 vs 9 if followed), follow=true reaches the file twice. → crash half of #4. | FS-4, §5; #4 |
| W4 ✅ | `FsFilter` on `Glob.h` (filename glob match) threaded through the scan; `Options.exclude` pruned before any event/descent. Exclude list moved `foldermonitor.exclude`→`workspace.exclude` (+ `use_git_ignore`); monitor's `GetExclusionPaths` reads the new key. Tests: build-dir subtree → zero events, `cmake-build-*` glob prune. → build-dir half of #4. | FS-2, §5/§6; #4 |
| W5 ✅ | `ReadFolderToNode` reimplemented as a scanner adapter: a parent-`Node` stack (push/pop balanced on enter/leave) feeds `ApplyFsEntry` (still THE single mutator); recursion deleted. `Options.exclude` from `workspace.exclude`; operational `maxDepth` from `workspace.maxScanDepth` (default 64 — generous so legit deep trees aren't truncated; symlink cycles handled by no-follow). **Synchronous/main-thread** (#6 stays out). Regression `test_workspace_openfolder_excludes_builddir` green. | FS-3, §2/§3; #6 |
| W6 ✅ | Closed out: #4 marked FIXED (no residual; FS-6 noted as separate deferred); `folderscanner` added to the verified-green set (CLAUDE.md, 247 tests); §0 statuses flipped; `work-log.md` updated. | §11, FS-7 |

---

## §1 — Goal & motivation

A filesystem walk that knows nothing about the editor's data model:

- **Self-contained & testable** — runnable over a temp dir with no `Workspace`/`Editor`/singleton in
  scope. The walk's behaviour (pruning, depth bound, cycle safety, ordering) is unit-tested at *its own*
  layer, recording an event stream and asserting on it.
- **One traversal, two consumers** — the initial `OpenFolder` scan and (later) the folder monitor's
  "a directory appeared, populate its subtree" both drive the *same* walker.
- **`Workspace` shrinks** — the recursion, the depth bound, and the exclude matching all leave
  `Workspace.cpp`; `Workspace` keeps only the adapter that turns scanner events into `Node`s.

This is a **leaf extraction** (the scanner depends on nothing in `Workspace`) — the sanctioned kind per
the house header style: it removes downward-only logic, it does not break a dependency cycle.

---

## §2 — How the model works today (read before "fixing" with depth/flags)

Two phases, both **eager** — the whole cost is paid up front, before the first frame:

1. **Scan** — `Workspace::OpenFolder` (`Workspace.cpp:359`) → `ReadFolderToNode` (`Workspace.cpp:391`)
   recurses the *entire* subtree into the `Node` tree, unconditionally. Recursion is driven by
   `fs::is_directory(entry)` — which **follows symlinks**, so a cyclic link is unbounded recursion →
   stack overflow (the observed "crash").
2. **View build** — `WorkspaceView::PopulateTree` (`WorkspaceView.cpp:163`) → `FillTreeView`
   (`WorkspaceView.cpp:24`) walks the *whole* already-built `Node` tree and materialises a `TreeView`
   node for every entry. The `hide_dot_files` `"."` filter (`excludePrefixList` /
   `IsStringExcluded`) is applied **here, at view-build time** — dotfiles are still scanned into the
   `Node` tree, just not displayed.

**Expansion in the WorkspaceView is purely visual.** `isExpanded` on a `TreeNodeRef` toggles visibility
of children that *already exist*; there is **no callback from expand back into the Workspace to scan on
demand**. Lazy expansion is therefore NOT a property of the current model (it needs new plumbing — FS-6).

### The seam is already half-built

`ApplyFsEntry` (`Workspace.cpp:410`) is documented in-code as *"THE single filesystem→tree mutator …
shared by the initial scan and the folder monitor."* The monitor already routes node creation back
through `Workspace`:

- `ProjectRoot::OnFsCreated/OnFsRemoved/OnFsChanged` (`Workspace.h:419/438/448`) are the single entry
  points the monitor calls.
- `CreateNodeDelgate` / `DeleteNodeDelgate` (`Workspace.h:332-333`), wired in `GetOrAddProjectRoot`
  (`Workspace.cpp:442`), are `Workspace`-supplied callbacks that funnel into `ApplyFsEntry`.

So the *node-building handler* is already the shared seam. What's tangled is the **recursive walker**,
which lives in `Workspace` and is welded to `Node`. That is the piece to extract.

---

## §3 — Design: a pure, event-emitting scanner

`FolderScanner` is a producer of **plain-data discovery events**; the consumer owns the tree. A
SAX-style visitor with depth, where the consumer keeps the parent stack:

```cpp
enum class FsEntryKind { kDirectory, kFile, kSymlink };

class FolderScanner {
public:
    struct Options {
        int  maxDepth      = 8;                  // hard safety bound (cycles / pathological depth)
        bool followSymlinks = false;             // default: don't follow (cycle safety)
        std::vector<std::string> exclude;        // static filter: build dirs, dotfiles (glob/name)
    };

    // Return false from onEnterDir → do NOT descend (consumer veto: collapsed/lazy).
    std::function<bool(const std::filesystem::path &, int depth)> onEnterDir;
    std::function<void(const std::filesystem::path &, int depth)> onFile;
    std::function<void(const std::filesystem::path &, int depth)> onLeaveDir;

    void Scan(const std::filesystem::path &root, const Options &opts);
};
```

`Workspace` becomes a thin adapter: `onEnterDir`/`onFile` push/pop a parent-`Node` stack and call the
existing `ApplyFsEntry`; `onEnterDir` returns `true` (always descend) for the eager scan. The recursion,
the depth bound, and the exclude matching all move *into the scanner*.

Event vocabulary deliberately carries `kDirectory`/`kFile`/`kSymlink` + `depth` as plain data — exactly
the flags the user asked for, with zero knowledge of `Node`, `Document`, or any app type.

---

## §4 — Why callbacks + plain data, NOT a template on the node type

The alternative floated was templating the scanner on "who is the parent node," threading a node handle
through the recursion. Rejected:

- **Testability** (the decisive reason, and the user's standing reason to avoid templates for testable
  units): a template forces every test to instantiate the scanner against *some* node type. A
  callback-based scanner is tested with trivial lambdas that append to a `vector<FsEvent>` — no fake
  node type, no `Workspace` linkage.
- **Layering** — a template parameter re-couples the low-level walker to the consumer's tree type at
  compile time, the opposite of the "low layer never knows the app layer" principle the codebase
  enforces elsewhere (the `WireScreenGeometry` / injected `autoSaveHandler` inversions).
- **Header hygiene** — a template lives in the header and drags its guts into every TU; a plain class
  with `std::function` hooks keeps the surface a `.h` and the body a `.cpp`, matching the house style.

The parent-threading the template was meant to provide is recovered cheaply by the consumer's own
push/pop stack against the enter/leave events.

---

## §5 — Static filter vs dynamic veto (this is where #4 + lazy-expansion both land)

Two distinct exclusion mechanisms, kept separate on purpose:

- **Static filter = scanner `Options.exclude` + `maxDepth`.** Build dirs / dotfiles are *config*; the
  scanner prunes them and simply never emits an event. This is the #4 fix (FS-2/FS-4). The matcher is
  shared with the monitor (§6) so the two can't drift.
- **Dynamic veto = `onEnterDir` return value.** Policy the scanner can't know — *"this node is
  collapsed, don't walk deeper yet"* — lives in the consumer and is expressed by returning `false`.
  This is the hook for **future lazy expansion** (FS-6): the scanner stays dumb, the policy stays in
  `Workspace`/`WorkspaceView`. **NB:** a vetoed *and* a depth-bounded directory are both "emitted but not
  descended" and must be marked unscanned so they can be expanded later — that marker is the crux of
  FS-6, analysed in **§12**.

Mapping each driver onto the right mechanism keeps the scanner free of editor policy while still giving
both features a clean home.

---

## §6 — Reuse by the folder monitor (shares data + matcher, never the subsystem)

> The folder monitor is its own work area with its own platform analysis —
> [`folder-monitor.md`](folder-monitor.md) (macOS FSEvents vs Linux inotify). This section is only the
> scanner side of the shared seam.

The monitor does NOT share the *traversal* in the obvious way — it gets single-path change events from
the OS, it has no walk of its own. What it genuinely shares:

1. **The node-building handler** (`ApplyFsEntry`) — already the documented shared seam (§2).
2. **The exclude matcher** — the same `FsFilter`/glob used by the scanner, so a path the scan skips is a
   path the monitor also ignores. `Glob.h` already exists in `Core/`; lift the matcher onto it.
3. **The traversal — only when a directory appears.** When the monitor reports a *directory* created
   (e.g. a `git clone`/branch-switch dropping a subtree), the current `cbCreateNode`
   (`Workspace.cpp:442`) adds just the one node. The correct behaviour is to run the **scanner rooted at
   that new directory** to populate its subtree — genuine traversal reuse (FS-5).

**The exclude list must move off the `foldermonitor` config section.** Today it's read via
`FolderMonitor::GetExclusionPaths()` (`FolderMonitor.cpp:40`) from `foldermonitor.exclude`
(`config.yml:53`: `.git, .idea, bld, cmake-build-debug, cmake-build-release`). The scan must NOT depend
on the disabled monitor subsystem — relocate the list to a scan-owned key (e.g. `workspace.exclude` /
`workspaceview.exclude`); the monitor reads the same key when it's re-enabled. Share the **data**, not
the monitor.

---

## §7 — Threading model (producer on a thread; the tree mutated only on the main thread)

The monitor is a continuous **background producer**, and the scanner may also run on a background thread
(e.g. driven by the monitor, FS-5). The workspace `Node` tree was never designed for concurrent access
— and it **must not be locked** to make this safe. Reuse the message-pump hand-off the codebase already
has:

- **`SafeQueue` is genuinely thread-safe** (`src/Core/SafeQueue.h`, mutex + condvar); `push()` is locked,
  so any thread may post.
- The monitor already routes through it: `FolderMonitor::DispatchEvent` (`FolderMonitor.cpp:22`)
  `PostMessage`s, and the callback is invoked in `Runloop::ProcessMessageQueue` **on the main thread**
  (`msg.Invoke()` → `ApplyFsEntry`). So **tree mutation is already marshalled to the main thread.**

Rules for the scanner/monitor:

- **Background thread = pure producer.** Touches only the filesystem; emits **plain-data** `FsEvent`s;
  reads NO singletons (`Config`/`Editor`/`Workspace`) — everything it needs is passed as `Options`. (The
  current Linux monitor reading `Config` on its poll thread is the anti-pattern; don't carry it forward.)
- **Main thread = sole mutator.** All `Node`/`Document`/tree writes happen in the pump callback.
- **Batch, don't stream.** A background scan buffers its events into a local `vector<FsEvent>` and posts
  **one** batch callback — not a message per file. The main thread applies the whole batch in a single
  pump cycle (tree touched once, coherently). For a normal source tree that's a handful of entries per
  change; the queue is never "smashed" unless you're (wrongly) monitoring a live build dir — which
  exclusion already prevents.

This is the coarse-grained simplicity we want **without a tree lock**, and it avoids reintroducing the #4
hang from the other side (a lock held across a full scan would stall every main-thread reader — each
redraw / `WorkspaceView::PopulateTree`). Contrast `TerminalScreen`/`screenLock`: that model mutates the
grid *directly* on the pty thread because it has hard latency needs; the scanner/monitor has none, so it
marshals instead of locking.

**Hard prerequisite — `Runloop::SwapQueues` is not thread-safe.** The double-buffered pump swaps the
`incoming`/`processing` queue pointers non-atomically (`Runloop.cpp:37`, flagged `// FIXME: must be
atomic or thread-safe`) while a producer may be dereferencing `incomingQueue` to push. Latent today (the
async tokenizer in `TextBuffer` is the only frequent cross-thread producer); it becomes a **live data
race** the moment a continuously-producing monitor/scanner thread runs. Filed as
[`open-bugs.md`](open-bugs.md) #6 — **must be fixed before enabling any continuous background producer.**

---

## §8 — What stays OUT of the scanner

- **Dirty / change-hooks.** That's *tree* state, not a scan concern. A `Node::dirty` flag + change
  notification already has a home (`Workspace::NotifyChangeHandler` / `SetChangeDelegate`,
  `Workspace.h:564/472`). The scanner emits *events*; whether a resulting node is new/changed/dirty is
  the consumer's bookkeeping. Folding it in would re-tangle exactly what we're separating.
- **Scan-time vs view-time exclusion (a deliberate choice).** Scan-time exclusion (build dirs) means
  those dirs are *never in memory* — correct, never wanted. The dotfile hide stays a *view-time* filter
  so a future "show hidden files" toggle works by view-rebuild without a rescan. Don't collapse the two:
  exclude heavy/build dirs at scan time; keep the dotfile hide at the view.

---

## §9 — Placement

- `src/Core/FileSystem/FolderScanner.{h,cpp}` — sibling to `FolderMonitor` (one walks once, one watches).
- Shared `FsFilter` (glob/name matcher) alongside, built on the existing `Glob.h`.
- No new build target/library — a source-list addition only (consistent with the cmake-cleanup stance
  of "named source groups feeding one target, not separate libs").

---

## §10 — Sequenced work items

Each: **Goal / Scope / Approach / Effort·Risk / Done-when / Depends-on.**

### FS-1 — Extract the pure `FolderScanner`
- **Goal:** a traversal that emits plain-data events; zero `Node`/`Document`/`Workspace` knowledge.
- **Scope:** new `FolderScanner.{h,cpp}`; `FsEntryKind`; `Options`; enter/file/leave + depth hooks.
- **Approach:** lift the loop out of `ReadFolderToNode`; recurse internally; emit events.
- **Effort·Risk:** S · low (new leaf unit, nothing depends on it yet).
- **Done-when:** scanner compiles standalone; FS-7 event-stream test green.
- **Depends-on:** —

### FS-2 — Shared exclude matcher + relocate the exclude list
- **Goal:** scan-time pruning of build dirs/dotfiles, fixing the #4 hang.
- **Scope:** `FsFilter` on `Glob.h`; move exclude list to a scan-owned config key; `Options.exclude`.
- **Approach:** scanner consults the filter before emitting/descending; pruned dirs emit nothing.
- **Effort·Risk:** S · low.
- **Done-when:** synthetic `cmake-build-debug/`-like subtree produces no events; monitor reads same key.
- **Depends-on:** FS-1.

### FS-3 — Rewire `Workspace` onto the scanner
- **Goal:** delete the in-`Workspace` recursion; `Workspace` becomes an adapter.
- **Scope:** `OpenFolder` drives `FolderScanner`; `onEnterDir`/`onFile` push/pop a parent stack →
  `ApplyFsEntry`; remove `ReadFolderToNode`'s recursion.
- **Approach:** keep `ApplyFsEntry` as the single mutator; the adapter only supplies the parent.
- **Effort·Risk:** M · medium (touches the live open-folder path — regression-test node counts).
- **Done-when:** `OpenFolder` builds an identical tree (minus excluded dirs) via the scanner;
  `test_workspace` green.
- **Depends-on:** FS-1, FS-2.

### FS-4 — Depth bound + symlink-cycle guard
- **Goal:** hard safety bound against cycles / pathological depth (the "crash").
- **Scope:** `Options.maxDepth` (default 8–10), `followSymlinks=false`; optional `workspace.maxScanDepth`.
- **Approach:** bail recursion at `depth >= maxDepth`; skip symlinked dirs unless `followSymlinks`.
- **Effort·Risk:** S · low.
- **Done-when:** depth-bound + symlink-cycle tests green (no overflow, node count bounded).
- **Depends-on:** FS-1.

### FS-5 — Monitor reuse: scan a newly-created directory *(deferred — monitor disabled)*
- **Goal:** when a directory appears, populate its subtree via the scanner.
- **Scope:** `cbCreateNode` runs `FolderScanner` rooted at the new dir for `kDirectory` events.
- **Effort·Risk:** S · low — but blocked on the monitor rework.
- **Done-when:** (when monitor re-enabled) creating a dir tree under a root populates it.
- **Depends-on:** FS-1..FS-4 + monitor re-enable + `open-bugs.md` #6 (SwapQueues race) — see §7.

### FS-6 — Lazy expansion *(deferred — its own effort; design notes in §12)*
- **Goal:** on-demand subtree scan for genuinely huge *legitimate* trees (monorepo), not build output.
- **Scope:** `Node::isScanned` marker; a scanner "emitted-but-not-descended" signal
  (`onLeaveDir(path, depth, bool fullyScanned)`); on-expand callback from `WorkspaceView` into
  `Workspace`. The "shallow scan mode" is **not** new code — it's `Scan(dir, {maxDepth = 1})` (§12).
- **Approach:** designed as one piece — `isScanned` is inert until the scanner signal + on-expand
  callback exist, so do NOT add it before then. Full analysis + the silent-truncation gap it closes: §12.
- **Effort·Risk:** L · medium — UI + model + scanner all involved.
- **Done-when:** expanding a collapsed folder triggers exactly one shallow scan; revisits are no-ops.
- **Depends-on:** FS-1..FS-3.

### FS-7 — Scanner unit tests
- **Goal:** lock the scanner's behaviour at its own layer.
- **Scope:** event-stream over a temp dir; exclude-prune; depth-bound; symlink-cycle; ordering.
- **Approach:** lambdas append to `vector<FsEvent>`; assert sequence/counts. No `Workspace`/singleton.
- **Effort·Risk:** S · low.
- **Done-when:** new `test_folderscanner` module green; added to the verified-green set.
- **Depends-on:** FS-1 (extended per item).

---

## §11 — What this means for `open-bugs.md` #4

Once FS-2 + FS-4 land, the substance of #4 is **resolved at the scanner layer**, not in `Workspace`:

- "recursive scan ingests build dirs" → FS-2 (scan-time exclude). **Resolved.**
- "no depth limit / symlink-cycle crash" → FS-4 (depth bound + no-follow). **Resolved.**
- The originally-proposed `Node::isRead` was premature; it belongs to FS-6 (lazy expansion), not the
  bug fix. **Re-scoped, deferred.**

**Action:** when the scanner lands, re-read #4 and either close it or trim it to whatever residual the
scanner did NOT cover (expected: none of the original symptom). **Done 2026-06-17 — #4 marked FIXED.**

---

## §12 — Lazy expansion: the "not-scanned" node marker (FS-6 design notes)

> Added 2026-06-17 from a review note while landing FS-3. Lowering the scan depth is the natural way to
> make expansion lazy — but the current model can't tell a depth-bounded directory from an empty one, so
> it would *truncate* rather than *defer*. Captured before it's built; FS-6 stays deferred.

**The gap.** Three things make the scanner emit a directory but **not descend** into it: the depth bound
(`depth+1 >= maxDepth`), the `onEnterDir` veto, and `followSymlinks=false`. In every case the dir node is
created with no children — and **today nothing distinguishes that node from a genuinely empty directory.**
So setting `workspace.maxScanDepth` low *silently truncates* the tree instead of producing a lazily-
expandable one. At the W5 default of 64 this is latent (no real tree hits 64, so nothing is mis-marked);
it bites the instant depth is lowered for laziness.

**Why `FlattenChilds` is shallow (it's not a bug).** It primarily drives the `WorkspaceView`/`TreeView`
file-tree, which materialises **one level at a time**. That per-level shape is exactly what lazy
expansion wants: one level scanned ⇒ one level shown. Keep it shallow.

**Insight: the depth bound IS the shallow-scan mechanism.** FS-6 originally posited a separate
"shallow (one-level) scan mode." Not needed — `Scan(dir, {maxDepth = 1})` already emits exactly one
level. Lazy expansion is then: initial scan at a small depth, and **each expand re-runs `Scan` rooted at
the expanded directory** (again shallow). So `maxScanDepth = 1` must mean "walk one level, mark the rest
expandable," NOT "truncate."

**What must be added — one coherent piece; do NOT add any part before the others exist:**

1. **A per-dir-node `isScanned` flag** (default `false`). This is the FS-6 / old-#4 `isRead`, reframed
   with a real home. Set `true` for a directory **only when the scan actually walked its contents** —
   including the *walked-and-found-empty* case, so expanding a known-empty dir is a no-op, not a re-scan.
2. **A scanner signal for "emitted but not descended."** The consumer can't infer it — it doesn't know
   the scanner's depth/symlink decision. Recommend reporting *effective descent on leave*:
   `onLeaveDir(path, depth, bool fullyScanned)`, where `fullyScanned = descend && mayFollow &&
   (depth+1 < maxDepth)` computed at the recurse **call-site** (not the top-of-`ScanDir` guard). The
   adapter then does `node->isScanned = fullyScanned`. One bool unifies all three non-descent reasons
   AND the empty-dir = scanned case. (Alternative: a `canDescend` hint arg on `onEnterDir` — weaker, it
   misses the empty-dir distinction.) **This is an additive change to the callback contract** I shipped
   in FS-1; flag it as such when FS-6 is built.
3. **Trigger the scan by trapping the expand ACTION in `WorkspaceView::OnAction` — do NOT add a callback
   out of `TreeView`** (decided in review — the better option). There are TWO trees: the model
   (`Workspace::Node`, built by the scanner) and the VIEW tree (`TreeView<Workspace::Node::Ref>::TreeNode`,
   mirrored by `WorkspaceView::FillTreeView`). `TreeView::Expand()` (bound to `kUIActionLineRight`) only
   flips `isExpanded` + re-`Flatten()`s, and `TreeView` is **generic over `T`** — it can't scan.
   `WorkspaceView::OnAction` already dispatches to `treeView->OnAction(kpAction)` (`WorkspaceView.cpp:224`),
   so **intercept `kUIActionLineRight` *before* delegating**: the row about to expand is
   `treeView->GetCurrentSelectedItem()` (the model `Node::Ref`); if it's an unscanned folder, run
   `Workspace::ScanNode(node)` (model gains children + `isScanned = true`) and mirror the new level, THEN
   delegate to `treeView->OnAction` — `Expand()`'s `Flatten()` reveals it because the view nodes now
   exist. **`TreeView` stays generic and untouched** (editor scan policy lives in the editor view —
   correct layering), and there is **no dangling-ref-in-`Expand()` gotcha** since the populate happens
   outside `Expand`.

**Wiring detail (the part that was unclear) — `TreeView` ↔ `WorkspaceView`:**

- **`FillTreeView` already self-limits to the scanned frontier** (recurses on `node->FlattenChilds`), so
  the *initial* mirror is already lazy — an unscanned model node stops the mirror. Expansion only mirrors
  the one new level.
- **Mirroring the new level — two ways:**
  - *(A) targeted:* mirror just the expanded node's children. Needs the VIEW `TreeNode` (not only its
    `data`) to attach under — add a small generic `TreeView::GetCurrentSelectedNode()` (symmetric with
    `GetCurrentSelectedItem`), or reuse `WorkspaceView::FindDocumentNode(root, path)` to locate it, then
    `FillTreeView` that node (empty → adds exactly one level, no dedupe).
  - *(B) reuse the change path:* `ScanNode` → `Workspace::NotifyChangeHandler()` → the EXISTING
    `SetChangeDelegate` (`WorkspaceView.cpp:75`) → full `PopulateTree()`, which re-applies expand state
    from `expandCollapseCache` (keyed by path). This is the "flatten redone / whole logic retried" worry —
    and it **already works**; heavier (full re-mirror) but bounded to the scanned frontier, needs no new
    accessor, and is async-ready. **Start with (B)**, drop to (A) only if the re-mirror proves costly.
- **Cursor stays stable** (children insert *below* the active line; `idxActiveLine` unchanged across the
  re-`Flatten()`), so `WorkspaceView::OnAction`'s "active line changed" check stays false — no spurious
  `SetActiveFolderNode`.
- **Caveat — the trap only catches actions routed through `WorkspaceView::OnAction`** (user keyboard
  expand). PROGRAMMATIC expansions bypass it: `PopulateTree`'s root `treeView->Expand()` and
  `SetCurrentlySelectedItem` → `ExpandToNode` (ancestor expand when syncing to the active document). Those
  target already-present (already-scanned) nodes today, so it's fine — but if a future path expands an
  unscanned node programmatically, route it through a shared "ensure-scanned" helper. (A `cbOnExpand`
  inside `Expand()` would catch those automatically, at the cost of an app-shaped hook on the generic
  widget + the dangling-ref care above — the tradeoff to weigh only if programmatic lazy-expand is needed.)
- **Async:** if `ScanNode` runs on a background thread (§7, gated on `open-bugs.md` #6) the trap kicks it
  off and the synchronous `Expand` shows nothing yet; the children appear when the batch lands via the
  change-delegate (option B). A synchronous scan shows immediately. Contract either way: "expand triggers
  exactly one scan; revisits are no-ops."
- **Perf:** `AddItem` `Flatten()`s per child — add a batch "insert many, flatten once" path to avoid N
  redundant flattens during a populate.

**Bonus — this de-risks the W5 `maxScanDepth` knob.** With the marker a depth-bounded dir is no longer
*lost*, only *deferred*: the bound becomes a performance/laziness dial, not a correctness hazard. The
generous default (64) can stay; a future lazy mode lowers it without data loss. (Until the marker exists,
treat `workspace.maxScanDepth` as "must be deep enough to cover the whole real tree.")

**Edge case — symlinked dirs.** A non-followed symlinked dir is also "not scanned." Auto-rescanning it on
expand re-opens cycle risk (bounded per-scan by no-follow, but each expand steps one level). Mark it
distinctly (`kSymlink`) and decide policy (expand-once with no-follow, or treat as a leaf) when FS-6 lands.
