# Session Cache — architecture & design proposal

Status: **Phase 1 COMPLETE** (design 2026-06-13; decisions folded 2026-06-14; implementation
2026-06-15). Documents, layout, window geometry, and debounced autosave all round-trip on the feature
branch and are GUI-verified. Step 2 (registry + restore) is the next phase. This document analyses the
problem, proposes an architecture, lays out a phased implementation, and (the section directly below)
tracks the live build state.

**Decisions locked 2026-06-14** (see §3.1–§3.3, §4, §4.2): the existing *global* window-geometry file
(`WindowLocation` / `gedit_lastwinloc.yml`) is **removed** — geometry becomes per-root session state and
no rendering backend does file I/O; a session is a property of an open **folder**, never a single file;
`SessionManager` is a singleton that is the sole owner of session disk I/O; the only surviving *global*
file is the live session **registry** (§3.5). Still open: multi-root-in-one-instance, whether declining
restore clears the registry, and the exact cold-start default geometry.

## 0. Implementation status (resume point — read first)

**Branch:** `feature/session-cache-phase1` (off `main`). Per-machine SDL backend is auto-selected
(`if(APPLE)` in `CMakeLists.txt`, `28c413b`) — no more toggle dance. **Phase 1 is feature-complete**:
window geometry + debounced autosave landed on top of the documents/layout work.
**Verified-green set is now 221** (the `session` module grew to 20 cases — added geometry resolve +
callback; the obsolete `winlocation` module was removed with `WindowLocation`); run from
`cmake-build-debug/` (`libutests.dylib` on macOS / `.so` on Linux):
`-m clipboard,document,vnav,cpplang,jsonlang,cppnumbers,linelayout,dcoverlay,layout,jsengine,workspace,terminalscreen,vtermparser,keymapping,hexprojection,bytestream,hexview,indent,session`

**DONE (committed, on the branch):**
- **Scaffolding + DTOs** (`cb84229`): `src/Core/Session/` (`SessionState.h` pure DTOs, `SessionManager`
  singleton, `utests/test_session.cpp`); CMake wired. `DocumentViewMode` lives in its own leaf header
  (`Core/DocumentViewMode.h`) to break an include cycle — **not** a forward declaration (deliberate; the
  house style extracts a leaf header rather than forward-declare).
- **Owner serialisation hooks (§11.3)** — `DocumentViewState::To/FromSession` (`55a5974`; added raw
  un-sorted `Selection::GetRawStart/GetRawEnd` so backward selections round-trip), `Document` (`e8e8af1`),
  `Workspace` enumerate + `ReopenDocument` skip-missing (`d9c5bf1`), splitter hooks
  `ViewBase::SetSessionId` + opt-in `To/FromSession(LayoutSession&)` on `VSplitView`/`HSplitView`
  (`c1c591b`), `WorkspaceView` expand/collapse persist + seed-on-rebuild (`eea83f1`).
- **Serialiser (§11.2)** — `SessionSerializer::To/FromYaml` (`dd39f83`); own header keeps yaml-cpp out of
  the view layer; `version` checked, never throws (returns false on malformed/newer → start clean).
- **SessionManager Save/Load (§11.4)** — `6e58cde`: resolves via AssetLoader `kProject`
  (`<cwd>/.goatedit/session.yml`); POSIX atomic write (tmp → `fsync` → `rename`). Added
  `AssetLoaderBase::ReplaceSearchPath` so a project re-point gives a single authoritative `kProject` path.
- **Documents end-to-end (§11.4)** — `06a9688`: `Editor::EstablishProjectDir` (auto-create `.goatedit/`
  + register `kProject`, **is_directory branch only** — single files sessionless), `RestoreSession`
  (load + reopen docs), `SaveSession` (in `Editor::Close`). `session:` section added to `config.yml`.
  **GUI-verified by the user 2026-06-15** (open files → quit → relaunch restores tabs + cursors + active).
- **Layout end-to-end (§4.1, §11.4)** — `4279a3e`: splitters + focused top-view + tree expand/collapse.
  **Key architectural fact:** layout restore **cannot** ride in `Editor::RestoreSession` — that runs in
  `Initialize`, *before* `main.cpp` builds the view tree. So it is a **second pass**,
  `Editor::RestoreLayout()` (public), called from `main.cpp` after `rootView.Initialize()` (and after the
  no-doc→workspace focus block, so a restored focus wins). It reads the session **already loaded** by
  `RestoreSession` (lingers in `SessionManager::CurrentSession`); `RestoreSession` now always `Load()`s
  when enabled, with doc-restore gated separately on `restore_open_files`. Mechanism:
  `ViewBase::CollectLayout/ApplyLayout` recurse self+subviews (subviews is protected, so the base owns the
  walk) — one call at the root persists/restores every tagged splitter at any depth. Splitters tagged in
  `main.cpp`: `split.workspace` (the workspace│editor `VSplitView`), `split.terminal` (the editor│terminal
  `HSplitViewStatus`). `RootView` overrides `To/FromSession` for `layout.focusedTopView`. WorkspaceView
  expand/collapse lives on `RootSession` (not `LayoutSession`) so it can't ride the walk — `Editor`
  reaches the view via a `GetWorkspaceView()` helper. `SaveSession` clears+repopulates → idempotent for
  the coming autosave.

**DONE — the final two Phase-1 pieces (2026-06-15, GUI-verified):**
1. **Window geometry (§4.2 / §11.3)** — re-routed through the session, with an explicit **layering
   inversion** (the load-bearing design decision here): the **graphics backend must NOT depend on
   `SessionManager`** (a high-level app construct). Geometry flows through plain data + a callback:
   - `ScreenBase` exposes `SetRequestedWindowGeometry(x,y,w,h)`, a `WindowGeometryChangedHandler`
     callback (plain ints), and `GetPrimaryDisplayBounds()` (a genuine graphics concern). The
     default-centering + clamp-to-display math (`ResolveStartupGeometry`, out-params) stays in graphics
     because it needs the display bounds — but it touches **no** session/config.
   - **`Editor::WireScreenGeometry(screen)`** is the glue (called in `SetupSDL2`/`SetupSDL3` *before*
     `Open()`): reads `SessionManager` geometry → feeds it in via the setter; registers the callback that
     writes live geometry back into the session + triggers autosave; owns the `restore_window_geometry`
     config gate. Geometry only ever flows **out** of graphics through the callback.
   - **Deleted** `WindowLocation.{h,cpp}`, `RuntimeConfig::GetWindowLocation()` + member, the obsolete
     `utests/test_winlocation.cpp` (+ its CMake entry), and the `gedit_lastwinloc.yml` mechanism, both
     backends. Cold-start default = centered 80% of the display (was hard-coded `1920×1080 @ (0,0)`).
     Restore clamps to display bounds (monitor may have changed). `ScreenBase` got a new `ScreenBase.cpp`.
2. **Debounced autosave (§11.4)** — `SessionManager` owns the debounce `Timer` + `NotifyChanged()`, with
   an **injected** `autoSaveHandler` (set by `Editor` to `SaveSession()`) so `SessionManager` keeps **no**
   `Editor` dependency (mirrors the keymap-delegate pattern). On elapse it posts the save to the
   main-thread runloop via `RootView::PostMessage` (no `TriggerUIRedraw` kick — the loop drains within one
   `PollEvents` tick, ≤250 ms, ample for a background save). `Clear()` stops the timer (test isolation).
   Triggered from **app-level** chokepoints only (all legitimately above `SessionManager`):
   `Workspace::AddOpenDocument`/`RemoveOpenDocument`/`SetActiveDocument`, splitter `SetSplitterPos` (via a
   new `ViewBase::NotifySessionChanged()` so the widely-included view header stays free of the session
   include), and the geometry write-back callback in the glue. Debounce coalesces a resize/relayout storm
   into one save. Config: `session.autosave_timeout_ms: 1500` (0 = off). Mirrors `TextBuffer`'s autosave.

**Known/deferred:** document paths are stored as-is (likely absolute) — relativise-to-root for
portability later. `Editor::LoadDocument` ↔ `Workspace::ReopenDocument` share an open sequence
(`FIXME(consolidation)` in `ReopenDocument`) — deliberately **not** touched mid-feature (§9). The
`.goatedit/` a real run leaves under a dir is picked up as `kProject` by the editor at init — the one
test that asserted a fresh `kProject` path (`save_load_roundtrip`) was made hermetic via
`ReplaceSearchPath` to mirror production; watch for the same env-pollution trap in new tests.

## 1. Goal

> Quit and reopen the application and it should feel like you never closed it.

Concretely, restore on next launch:

- **Window geometry** — pixel size + position of the OS window.
- **Layout** — splitter positions (workspace│editor, editor│terminal), terminal pane height,
  which top-view had focus.
- **Open files** ("tabs") — the set of open documents, the active one, per-document scroll
  (`viewTopLine`), cursor position, selection, and **view mode** (text / hex).
- **Workspace tree** — which roots are open, and the expand/collapse state of the browse tree.
- **Edit history** (stretch goal) — per-document undo stack, so undo still works after a restart.

And, forward-looking, the same on-disk substrate must be able to back a **file/token search
index** ("find anything" / fuzzy search) without being redesigned for it.

## 2. What state exists today (inventory)

| State | Lives in | Notes |
|---|---|---|
| Cursor, selection, scroll, view mode | `DocumentViewState` (`Cursor.h` `LineCursor` + `Selection` + `DocumentViewMode`) | Already carved out as per-view state — built to be serializable. |
| Undo stack | `EditState` → `UndoHistory` (`std::deque<UndoItem::Ref>`) | `EditState.h` explicitly says it exists to be "a serializable unit for restore-on-reopen". Heavy: per-line `std::u32string` snapshots. |
| File identity, dirty flag | `Document::path`, `TextBuffer::BufferState` (`kBuffer_Empty/FileRef/Loaded/Changed`) | |
| Open documents + active | `Workspace::openDocuments`, `Workspace::activeDocument` | Single source of truth. |
| Root folders | `Workspace::projectRoots` (each a `ProjectRoot` with a canonical `rootPath`) | **Multiple roots supported.** |
| Splitter positions | `VSplitView::splitterPos`, `HSplitViewStatus::splitterPos` | Views are **anonymous stack locals** in `main.cpp` — no IDs to address them by. |
| Tree expand/collapse | `WorkspaceView` `expandCollapseCache` (`map<pathString,bool>`) | In-memory only, rebuilt on tree rebuild. **Consolidation candidate.** |
| Node meta (size/type) | `Workspace::Node::metaData` (`ConfigNode`) | Per-scan cache, not session. Leave as-is. |
| Window geometry | **Already implemented** — `WindowLocation` (a `ConfigNode`) persists x/y/w/h to a *global* `~/.local/state/.../gedit_lastwinloc.yml`; both SDL2 & SDL3 `SDLScreen` read it at window-create and `UpdateWindowLocation()` saves on move/resize. (The `main.cpp:54` TODO was already done this way.) **Decision (2026-06-14): remove this global file** — geometry becomes per-root session state (§3.1, §4.2); the SDL get/set hooks are kept but re-pointed at `SessionManager` (in memory), and the backend stops doing file I/O. |

Path/format infrastructure already present and reusable:

- **`XDGEnvironment`** — `GetUserStatePath()` (`~/.local/state`), `GetUserCachePath()` (`~/.cache`),
  `GetUserDataPath()` (`~/.local/share`), `GetUserConfigPath()`, `GetUserHomePath()`. App subdir is `goatedit`.
- **`.goatedit/` project dir** — already the opt-in project marker (`Editor::Initialize` registers it
  as the `kProject` asset path, **only for a strict sub-directory of `$HOME`**; `/`, `$HOME` itself,
  and anything outside home can never become a project). The exact guard we want for session placement.
- **yaml-cpp** (config/theme) and **nlohmann json** (theme) are both in-tree. `ConfigNode` wraps YAML.
- **`BinBuffer` / `ByteStreamReader`** for compact binary blobs.
- **`AssetLoaderBase::ResolveWritePath(file, locationType)`** — its own comment notes it wants a
  stable "primary write path per location"; the session manager needs exactly that.

**Takeaway:** the MVP needs *no new third-party dependency*. The seams (DocumentViewState, EditState)
were deliberately cut for this, and the XDG + `.goatedit/` plumbing already exists.

## 3. Design decisions (the four questions)

### 3.1 Where should the cache reside?

Three locations, each with a clear job:

- **`<root>/.goatedit/`** (in-project) — the **per-root session detail**: open files, cursors, scroll,
  selection, view-modes, splitters, tree expand/collapse, **and window geometry**. Everything that makes
  one project come back as you left it lives here, with the project (§3.1 body below).
- **`$XDG_STATE_HOME/goatedit/`** (`~/.local/state/goatedit/`) — only the **live session registry**
  (§3.5): one tiny entry per running instance (pid + root paths). Not the per-root detail. Also the home
  for the central *fallback* sessions of roots that can't have a `.goatedit/` (outside `$HOME`).
- **`$XDG_CACHE_HOME/goatedit/`** (`~/.cache/goatedit/`) — *regenerable* data: the search/token index.
  Safe to delete; rebuilt on demand.

(Geometry is **per-project**, so it belongs in each project's `.goatedit/`, not in one global file.
Note this is a *correction of existing behaviour*, not a hypothetical: a global geometry file
(`gedit_lastwinloc.yml`) exists today and is being **deleted** (§4.2). The one thing that legitimately
stays global is the live session **registry** (§3.5) — but that holds *which instances are open*, not
geometry; the two were never the same concern. After this change the XDG dir holds only the registry.)

**The session detail lives WITH the project, in `<root>/.goatedit/`** — *everything* for that session,
window geometry included. The XDG dir holds only a small **live session registry** (§3.5), never the
per-root detail. This is the key simplification: there is no central-vs-in-project duality to reconcile
— per-root detail is always in `.goatedit/`; central is purely the fallback for roots that can't have
one (outside `$HOME`, §3.3).

```
<root>/.goatedit/
  session.yml          # open files, active doc, cursors, selection, scroll, view-modes,
                       # tree expand/collapse, splitter positions, AND window geometry
  undo/<file-key>.undo # binary undo blobs (gated, Phase 2)
```

Per-root detail is self-contained: a single root is reasoned about, copied, or deleted independently,
and the file travels with the project (clone the repo on another box and your layout comes too, if you
choose to commit it — though `.goatedit/` is normally git-ignored as machine-local state).

For the central fallback case only, `<root-key>` = `<basename>-<fnv1a(canonical-abs-path)>` (e.g.
`editor-3f9a1c20`). **Use a small in-tree FNV-1a, not `std::hash`** — `std::hash` is not guaranteed
stable across runs/platforms, which is disqualifying for a persistent key. The basename prefix keeps
the dir human-recognisable.

### 3.2 Multiple root folders

Two distinct meanings of "multiple roots" — keep them separate:

1. **Several roots open in ONE running instance/process.** Supported by `Workspace` today
   (`projectRoots` is a list). This is the corner case: one `.goatedit/session.yml` can't live in "the"
   project root because there are several. Handling: pick a **primary root** (the one the process was
   launched with / the active root) to host `session.yml`, and record the *other* open roots inside it
   as a list of paths. The registry entry (§3.5) for that instance lists all of them so restore relaunches
   the process with the same `goatedit <primaryRoot> <root2> …` argument set. Acceptable as a corner case;
   the common path is one root per instance.
2. **Several instances/processes, one root each** — your actual workflow (3 projects = 3 processes).
   Each process owns its own `.goatedit/session.yml` independently; no coordination needed. The registry
   + restore module (§3.5) is what brings them all back. **This is the case the design optimises for.**

### 3.3 Folders outside home

A decision table, guarded by config (`session.persist_external`, default **off**):

| Location | `.goatedit/` exists | Behaviour |
|---|---|---|
| Strict subdir of `$HOME` | yes | **In-project** `<root>/.goatedit/session.yml`. |
| Strict subdir of `$HOME` | no | **Auto-create** `<root>/.goatedit/` and use it (default, `session.auto_create_project_dir: yes`). State travels with the project and the dir is already the editor's project marker. Set the flag to `no` to fall back to a **central** path-hash session instead. |
| Outside `$HOME` (e.g. `/var`, `/etc`, a mounted volume) | — | **No session unless** `session.persist_external: true`. If enabled → **central only** (never write a `.goatedit/` into a folder outside home — don't litter system dirs, and auto-create is suppressed here regardless). |
| `/` or `$HOME` itself | — | Never a session root (same guard as `kProject` today). |

This reuses the existing `IsStrictSubdir(cwd, home)` logic verbatim — no new policy primitive.
**Decision (2026-06-13): auto-create `.goatedit/` for in-home roots by default** — sessions live with
the project, mirroring how `.goatedit/` already anchors project-local assets. (`.goatedit/` should be
git-ignored by the user; the session file is machine-local state.)

**Decision (2026-06-14): a session is a property of an open FOLDER, not a file.** `.goatedit/` is
created (and a session loaded/saved) *only* when a **folder** is opened — the `is_directory` branch of
`Editor::OpenDocumentOrFolder` (`Editor.cpp:875`, which already `chdir`s the cwd to that folder).
**Opening a single file creates no `.goatedit/` and has no session** — cursor/scroll for a loose file
are not persisted. This makes the rule a clean one-liner — *session ⇔ open folder* — and the decision
table above only ever applies on the folder path.

### 3.4 Data format

Tiered by data characteristics:

- **Session state (small, inspectable, occasionally hand-edited)** → **YAML**. Reuses the project's
  config language, `ConfigNode`/yaml-cpp infra, stays diffable. This covers per-root `session.yml` and
  the registry entries (§3.5). (JSON via nlohmann is the fallback if YAML emit proves awkward; YAML
  wins for consistency.)
- **Undo history (large, machine-only, fragile)** → **compact binary** via `BinBuffer`, one blob per
  document under the central cache, **gated** (see §5 Phase 2). Serialising per-line `u32string`
  snapshots as YAML would be enormous and pointless to read.
- **Search/token index (large, query-heavy, regenerable)** → **SQLite (FTS5)** — the one genuinely new
  dependency, and only for the future feature (§6).

Every file carries a top-level `version:` for forward-compat. All writes are **atomic** (write to
`*.tmp`, `fsync`, `rename`) so a crash mid-save never corrupts a session.

### 3.5 The live session registry + restore module (the second step)

This is the layer built **on top of** working per-root persistence — a deliberate two-step design.
Step 1 (Model 1) makes each project restore perfectly *when opened*. Step 2 makes "what was open"
come back automatically, without any multi-window or multi-process orchestration inside the editor.

**Registry** — now the editor's *only* global persisted state (window geometry became per-root, §3.1).
A directory of tiny entries in the XDG state dir, one per *running* instance:

```
~/.local/state/goatedit/sessions/
  <instance-id>.yml     # { pid, primary_root, roots: [..], started_at }
```

Lifecycle is the whole trick:

- **On startup**, an instance **writes its entry** (essentially just the root path(s) it was launched
  with + its pid).
- **On clean exit**, the instance **removes its own entry**.

So the registry is exactly the set of sessions that were *live*. After a clean quit-all it is empty
(nothing to restore — you closed everything on purpose). After a **reboot or crash**, the OS killed
the instances before they could remove themselves, so their entries **survive** — and *those* are what
restore brings back. This naturally captures "what was running when the machine went down" with no
snapshot-on-save needed and no separate "last session" concept.

**Restore module** — a small component (a CLI subcommand / flag, e.g. `goatedit --restore` or a tiny
companion, wired into your login/autostart) that:

1. Reads the registry.
2. **Skips entries whose `pid` is still alive** (that instance is already running — don't double-spawn).
   The remaining entries are the dead-but-unclean ones from the reboot.
3. For each, does the two-step spawn: `chdir(entry.primary_root)` → `spawn("goatedit", entry.roots…)`
   as a detached process. The freshly-spawned `goatedit` then loads that root's `.goatedit/session.yml`
   (Step 1) and restores files/cursors/geometry/layout on its own.

No throwaway "launcher window," no process to kill afterwards: the restore module is a plain spawner
that exits when done; each spawned `goatedit` is a normal full instance. Single-instance-per-root is
enforced by the pid liveness check (and a `.goatedit/session.lock` owning-pid file as backstop).

**Refinement (2026-06-14): restore can be an in-app cold-start modal, not only an external spawner.**
A cold-started instance (launched with no folder) reads the registry itself and, if it is non-empty,
offers a *"restore previous session?"* modal (the modal infra already exists — `ListSelectionModal`).
On **yes** it does the chdir+spawn per dead entry; on **no** it drops to the empty workspace. A clean
exit de-registers the instance regardless. This keeps restore inside the editor — no separate restore
binary or login/autostart integration is required, though the `goatedit --restore` CLI remains an
option for headless/login-hook setups. **Open:** whether declining restore should *clear* the registry
is undecided. **Caveat:** the pid-liveness check must be disambiguated against pid *recycling* across a
reboot (compare `started_at` / a boot id) — otherwise an unrelated live process that inherited a
recycled pid would mask a dead entry that should have been restored.

**Why this ordering matters:** Step 2 needs *nothing* from inside the editor except "write/remove my
registry entry on start/exit" — a dozen lines. All the heavy lifting (restoring a project's state) is
Step 1, which is independently valuable and testable. If Step 1 feels right, Step 2 is a thin, low-risk
addition.

## 4. Architecture

A new **`SessionManager`** singleton under `src/Core/Session/` — orchestrator, not god-object. It
owns *placement policy*, *load/save/atomic-write*, and *triggering*; the actual state is serialised
by each subsystem through small `ToSession`/`FromSession` hooks. This matches the codebase grain
("model stays logical; owners serialise themselves") and the Phase-2 seams.

**Two invariants (locked 2026-06-14):**
1. **`SessionManager` is the sole owner of session disk I/O.** No subsystem and no rendering backend
   reads or writes a session file directly — they exchange in-memory DTOs with it. (This is why
   `SDLScreen::Open()` must stop calling `WindowLocation::Load()` and instead read in-memory geometry.)
2. **Reachability is the singleton `SessionManager::Instance()`** (mirrors `Config` / `KeyMappingCache`
   / `XDGEnvironment`; add a `Clear()` for test isolation). It is loaded during `Editor::Initialize`,
   so it is fully populated *before* `OpenScreen()` creates the window. It is **not** registered in
   `RuntimeConfig` — that hub is for instances swapped per run (the screen backend, the root view); a
   global session service isn't one, and a second handle would just be redundant.

```
src/Core/Session/
  SessionManager.{h,cpp}    # Step 1: per-root load/save, atomic write, debounced autosave
  SessionState.{h,cpp}      # plain DTOs: RootSession, DocumentSession, LayoutSession (+ geometry)
  SessionPaths.{h,cpp}      # Step 2 ONLY: FNV-1a root-key + central fallback (outside-home). Phase 1
                            #   uses AssetLoader kProject instead (§4.4) — no SessionPaths needed.
  SessionRegistry.{h,cpp}   # Step 2: write-on-start / remove-on-exit registry entry (§3.5)
  SessionRestore.{h,cpp}    # Step 2: read registry, pid-liveness filter, chdir+spawn (§3.5)
```

Steps map to phases: `SessionManager`/`SessionState` are the Model-1 MVP (Phase 1), resolving paths via
`AssetLoaderBase` `kProject` (§4.4); `SessionPaths`/`SessionRegistry`/`SessionRestore` are the second
step (Phase 2), added only once Step 1 feels right.

Serialisation responsibilities pushed to owners:

- `DocumentViewState::ToSession()/FromSession()` — cursor, selection, scroll, view mode. *(It was
  designed for exactly this.)*
- `EditState` — undo blob (Phase 2, gated).
- `Document` — path + dirty marker; aggregates the two above.
- `Workspace` — enumerate roots/open-docs/active; a `ReopenRoot(path)` / `ReopenDocument(path)` path
  that skips missing files gracefully.
- `WorkspaceView` — its existing `expandCollapseCache` becomes the session's tree state
  (**consolidation**: persist on save, seed on build instead of starting empty).
- View tree splitters — see §4.1.
- `ScreenBase` (+ SDL2/SDL3) — *report* live geometry to `SessionManager` and *apply* restored
  geometry; the SDL get/set hooks already exist (§4.2). The backend never touches a file.

### 4.1 Addressing the splitters

The persistable splitters are anonymous stack locals in `main.cpp`. Give each a stable string id and
register it so `SessionManager` can pull/push `GetSplitterPos()` generically:

- Add `ViewBase::SetSessionId(const std::string&)` + an opt-in `ToSession/FromSession` returning the
  splitter position (default no-op).
- In `main.cpp`, tag the two persistable splitters (e.g. `"split.workspace"`, `"split.terminal"`) and
  register them with `SessionManager` (or `RuntimeConfig`). `main.cpp` already knows the layout, so
  this is wiring, not redesign.
- Store positions **relatively** (ratio of content extent) as well as absolute, so a restore onto a
  different window size lands sensibly (`GetSplitterPosRelative()` already exists on the splitters).

### 4.2 Window geometry

**This already exists and is being re-pointed, not built.** `WindowLocation` (a `ConfigNode`) persists
x/y/w/h to a *global* `~/.local/state/.../gedit_lastwinloc.yml`; both SDL2 and SDL3 `SDLScreen` already
call `SDL_Get/SetWindowSize` + `SDL_Get/SetWindowPosition` (in `Open()` and `UpdateWindowLocation()`,
fired on move/resize). The work is to **delete the global file and route geometry through the per-root
session instead:**

- **Delete** `WindowLocation`, `RuntimeConfig::GetWindowLocation()`, and `gedit_lastwinloc.yml`. Cold
  start (no project) no longer restores a window — it uses a **sensible default** (center at a fraction
  of the current display; *not* the current hard-coded `1920×1080 @ (0,0)`, which clips on small monitors).
- **Read:** `SDLScreen::Open()` asks `SessionManager::Instance().GetGeometry()` — purely in memory; the
  session was loaded during `Editor::Initialize`, which runs *before* `OpenScreen()`. The backend reads
  **no file**.
- **Change:** the move/resize handler reports `(x, y, w, h)` to `SessionManager` (in-memory + mark dirty
  → debounced disk write, owned by `SessionManager`). The backend writes **no file** either.
- **Per-root, three timings:** (a) `goatedit <folder>` — cwd is the root and the session is loaded
  before the window is created, so it opens at the restored geometry with no create-then-jump flicker;
  (b) desktop/icon launch (no folder) — the window opens at the default, and a later folder-open applies
  that root's geometry to the live window (move/resize); (c) runtime folder-switch — same as (b).
- On restore, **clamp to the current display bounds** (a monitor may have changed or disconnected).

### 4.3 When do we save / restore?

- **Restore** (Step 1, the running instance restoring *its own* project): in `main.cpp` after
  `rootView.Initialize()` and before `Runloop::DefaultLoop()` — views exist, so splitter/geometry/
  open-doc restore can apply. The root(s) for this instance come from the launch args (or, for Step 2, a
  spawned `goatedit <roots>`); the per-root `.goatedit/session.yml` supplies the rest.
- **Save**:
  - On clean shutdown (`Editor::Close()`) — and remove the registry entry here (Step 2).
  - **Debounced autosave** on meaningful events (document open/close, active-doc switch, splitter move,
    window resize/move). Reuse the existing autosave-timer pattern (there's already a "save on change"
    timer for documents). Debounce so a window drag doesn't write 100 files.
  - A crash leaves the last debounced snapshot — acceptable, and atomic writes guarantee it's intact.

### 4.4 Go *through* `AssetLoaderBase`, not around it (the XDG abstraction)

`AssetLoaderBase` is the project's existing abstraction over "where does a file live": callers ask for a
file by **location type** (`kSystem`/`kUser`/`kProject`/`kAny`) and the loader resolves the real path.
All XDG/`.goatedit/` knowledge is registered **once** in `Editor::Initialize` via `AddSearchPath(...)`.
The session subsystem should consume that abstraction rather than re-deriving paths:

- **`kProject` already maps to `<cwd>/.goatedit`** (`Editor.cpp:199-200`), and opening a folder
  `chdir`s so **cwd == the launched root** (`Editor.cpp:896`). So the per-root session file is simply a
  `kProject` asset named `session.yml`:
  - **Write** via `assetLoader.ResolveWritePath("session.yml", kProject)`.
  - **Read** via `assetLoader.LoadAsset("session.yml", kProject)`.
  This means `SessionManager` knows **nothing** about XDG, `$HOME`, or `.goatedit/` — the placement
  policy and the strict-subdir-of-home guard already live in (and stay owned by) the `AddSearchPath`
  registration. No duplicated logic.
- **`auto_create_project_dir` belongs at that registration**, not in `SessionManager`: the existing code
  only registers `kProject` when `.goatedit/` already exists, so creating it there (when in-home +
  config allows) is the one change needed to make the `kProject` write-path available.
- **`ResolveWritePath` is therefore load-bearing, not optional** — it is the primary write API for the
  session file, so its "stable primary-write-path per location" hardening (its own TODO) is in-scope.

What does *not* fit the asset model (and so stays a thin direct `XDGEnvironment` consumer, in Step 2):
the **live registry** (a *directory of per-instance entries* needing enumeration) and the **central
fallback** for outside-home roots (a per-`<root-key>` *subdirectory* — `LoadAsset` resolves by bare
filename across search paths, so it can't express "one subdir per root"). Optionally these motivate
adding `kState`/`kCache` location types later (registered from `GetUserStatePath()`/`GetUserCachePath()`
to keep the "no caller touches XDG" invariant), but Phase 1 needs none of it.

**Net effect on Phase 1:** with the defaults (`persist_external: no`, `auto_create_project_dir: yes`)
the session file is *always* `<root>/.goatedit/session.yml`, reachable purely through `kProject`. So
**Phase 1 drops `SessionPaths`, the FNV root-key, and the central fallback entirely** — they belong to
Step 2 when the registry and outside-home cases arrive.

### 4.5 Launch without a folder (desktop / icon launch)

The "feels seamless" restore must not assume there is a project root at startup. A desktop-environment
launch (clicking the icon) starts with **no meaningful cwd** — the menu/Finder launches at `/`, and
`Editor::Initialize` already relocates cwd to `$HOME` in that case (`Editor.cpp:186-191`). So there is
**no project, no `.goatedit/`, no `kProject`** until the user opens a folder.

Consequences (these drive the wiring in §11.4):

- **No-folder is a valid running state with no session.** Sessions are per-root; no root ⇒ nothing to
  save or restore. The app already lands in the workspace view when there's no active document
  (`main.cpp:523-525`). Don't manufacture a session for "nothing open".
- **`kProject` + the session are established lazily, on the FIRST folder-open** — which is the *same*
  code path as a launched-with-a-root start (that just opens the folder during init). So the
  open/restore trigger hangs off `OpenFolder`, not off init-time cwd. This subsumes the
  "runtime-folder-open re-register" caveat from §11.1 — it's not an edge case, it's the desktop-launch
  main path.
- **Picking the folder is a UX gap, not a session-cache concern.** The intended affordance is a modal
  folder/recent-projects picker (not yet built). Until it exists, desktop-launch simply shows the empty
  workspace and the user opens a folder via the existing open action. **Out of scope for Phase 1.**
  Synergy worth noting: the Step-2 registry — plus a small recent-roots (MRU) list in XDG state — is
  exactly the data that picker's "recent projects" list would show, so the two features feed each other
  later.

## 5. Robustness / edge cases

- **File moved or deleted on disk** since last session → on restore, skip that document, drop its
  cursor/undo, keep going. Never fail the whole restore for one stale entry.
- **File changed on disk** (undo restore) → store the file's `(mtime,size,content-hash)` alongside the
  undo blob; restore undo **only** if it still matches, else discard undo but keep cursor. Undo against
  a buffer that drifted underneath it would corrupt edits.
- **Versioning** → `version:` field on every file; unknown/newer → best-effort or ignore, never crash.
- **Atomic writes** → temp + rename (§3.4).
- **Concurrent instances** on the same root → simple owning-pid lockfile with last-writer-wins
  fallback; don't over-engineer. (Two editors, same project, last close wins.)
- **Privacy/size** → undo + index can embed file contents; they live under the user's home with user
  perms and never get copied to a shared/central location for read-only roots.
- **Caps** → `session.undo_cap` (max items/bytes per doc) and a total per-root cache budget, so a
  giant edit session doesn't write hundreds of MB.

## 6. Future: file/token search index ("find anything")

Decoupled subsystem sharing only the root-key hashing and the cache-dir layout — **the session-cache
MVP must not depend on it or on SQLite.**

- **Dependency**: **SQLite amalgamation** dropped into `src/ext/` like duktape (single .c/.h, public
  domain, no submodule). Build with **FTS5** for full-text + token search.
- **Location**: `~/.cache/goatedit/index/<root-key>.sqlite` (cache tier — regenerable, deletable).
- **Schema sketch**:
  ```
  files(id, path, mtime, size, lang)
  symbols(id, file_id, name, kind, line, col)
  files_fts   USING fts5(path, content, content='files')      -- full-text over file bodies
  symbols_fts USING fts5(name, content='symbols')             -- token/identifier search
  ```
- **Population**: a background indexer reusing the existing `Job` / background-tokenizer pattern. The
  `LangLineTokenizer` already produces per-line tokens — tee identifiers into `symbols`. The
  `FolderMonitor` seam (when re-enabled) keeps the index live on fs changes.
- **Query**: FTS5 + a fuzzy/subsequence ranker (fzf-style) on top for "find anything". This is where a
  Telescope/Helix-style picker (already on the `main.cpp` wishlist) plugs in.

## 7. Configuration (`session:` section in `config.yml`)

```yaml
session:
  enabled: yes                  # master switch (Step 1: per-root persistence)
  persist_external: no          # generate sessions for roots OUTSIDE $HOME (central only)
  auto_create_project_dir: yes  # create <root>/.goatedit/ on FOLDER open for in-home roots (default on);
                                # opening a single FILE never creates it — loose files are sessionless
  persist_undo: yes             # serialise undo stacks (gated by undo_cap + mtime/hash guard)
  undo_cap: 2000                # max undo items per document
  restore_window_geometry: yes
  restore_layout: yes           # splitters / terminal pane
  restore_open_files: yes
  register_instance: yes        # Step 2: maintain the live registry entry (start/exit). Independent of
                                # whether the restore module is wired into login/autostart.
```

## 8. Affected areas (impact map)

| Area | Change |
|---|---|
| `src/Core/Session/*` (new) | Step 1: `SessionManager`, `SessionState` DTOs (paths via AssetLoader `kProject`). Step 2: `SessionPaths` (FNV-1a + central fallback), `SessionRegistry`, `SessionRestore`. |
| `Editor::Initialize` / `Editor::Close` | Step 1: **auto-create `.goatedit/` at the `kProject` registration** (`Editor.cpp:199-200`); load/save per-root session via AssetLoader, reopen docs. Step 2: write/remove the registry entry on start/clean-exit. |
| CLI / `main.cpp` arg parsing | Step 2: `--restore` entry point (read registry → chdir+spawn). |
| `main.cpp` | tag splitters with session ids; call layout/geometry/open-file restore after `rootView.Initialize()`. |
| `Workspace` | enumerate roots/open-docs/active; `ReopenRoot`/`ReopenDocument` (skip-missing). |
| `Document` / `DocumentViewState` / `EditState` | `ToSession`/`FromSession`; undo blob (gated). |
| `WorkspaceView` | persist+seed expand/collapse cache (consolidates the in-memory cache). |
| `ScreenBase` + SDL2 + SDL3 | **Re-route** the existing geometry get/set (already in `Open()` / `UpdateWindowLocation()`, both backends) to read/write `SessionManager` *in memory* — backend does **no file I/O**. |
| `WindowLocation` / `RuntimeConfig` | **Delete** `WindowLocation`, `RuntimeConfig::GetWindowLocation()` + its member, and `gedit_lastwinloc.yml`. Geometry moves into per-root `session.yml`. |
| `ViewBase` + `VSplitView`/`HSplitViewStatus` | session id + splitter `ToSession/FromSession`. |
| `Config` | new `session:` section + defaults. |
| `AssetLoaderBase` | **Phase 1 primary write API** via `kProject` (§4.4): harden `ResolveWritePath`. Step 2 may add `kState`/`kCache` location types for the registry + index. |
| `XDGEnvironment` | Phase 1: not touched directly (AssetLoader owns XDG resolution). Step 2: registry/central-fallback consume `GetUserStatePath()`. |

## 9. Consolidation opportunities (invited)

- **`WorkspaceView` expand/collapse cache** → fold into the per-root session (persist + restore). It
  already keys by path string — a direct lift.
- **`.goatedit/` project dir** → one consistent "project state" directory: already holds project-local
  assets (`kProject`), now also `session.yml`. Single concept, not two.
- **`AssetLoaderBase::ResolveWritePath`** → implement the "primary write path per location" the code
  already wishes for; the session manager is its first real consumer.
- **Node meta cache** (filesize/type) → leave for now, but note the future search-index DB is the
  natural long-term home for "what files exist under this root".
- **`Editor::LoadDocument` ↔ `Workspace::ReopenDocument`** → both run the same open sequence (file-ref
  node → `LoadData` → readonly-meta → `AddOpenDocument`). Extract one shared `Workspace` open primitive
  and have both call it — `FIXME(consolidation)` is in `Workspace::ReopenDocument`. **Deferred
  deliberately:** `Editor::LoadDocument` is a known leftover API and the live open path; consolidate it
  as its own separately-verified change, not mid-feature.

## 10. Phasing

The decided shape (2026-06-13): **two steps first** — per-root persistence, then session awareness on
top — with undo and the index as later, independent additions.

1. **Step 1 / MVP — per-root persistence (Model 1, zero new deps).** `SessionManager` + `SessionState`
   + atomic YAML I/O, **paths via AssetLoader `kProject`** (§4.4 — no `SessionPaths` yet). Each project's
   `.goatedit/session.yml` stores open files, active doc,
   cursor/scroll/selection, view-mode, splitters, tree expand/collapse, **and window geometry**. Save on
   shutdown + debounced autosave; restore on open. Opening a project = it comes back exactly as left.
   *Reopening is manual at this stage.* **Build and validate this before Step 2.**
2. **Step 2 — session awareness (`SessionRegistry` + `SessionRestore`, §3.5).** Write-on-start /
   remove-on-clean-exit registry in the XDG state dir; restore module reads it, filters live pids, and
   does `chdir + spawn goatedit <roots>` per dead entry. Thin layer on top of Step 1 — only added once
   Step 1 feels right. Brings every project back after a reboot, no multi-window/in-editor orchestration.
3. **Undo persistence (in scope, gated).** Binary undo blobs in `.goatedit/undo/` with an
   `(mtime,size,hash)` guard; `persist_undo: yes` by default but capped (`undo_cap`) and skipped when the
   file drifted on disk. Independent of Steps 1–2; can land any time after Step 1.
4. **Search index (SQLite/FTS5).** New `src/ext/` dep; background indexer on the `Job` pattern;
   "find anything" picker on top. Fully decoupled.

**Out of scope (explicitly):** Model 3 — multi-window-in-one-process. Never the intention; it would
require a new UI and a rearchitecture of the single-window Editor/Screen/Runloop. The north star is
multiple *views within* a window, not multiple OS windows. Multi-instance restore is achieved by Step 2
(spawn N processes), not by hosting N windows.

Each step is independently shippable; 3 and 4 do not block 1–2, and Step 2 does not block Step 1.

## 11. Phase 1 implementation checklist (per-root persistence / Model 1)

Scope: a single launched instance, on close, writes its project's `.goatedit/session.yml`; on open of
that project, restores it exactly. **No registry, no respawn, no undo, no index** (Steps 2–4). Work
roughly top-to-bottom; each group is mergeable on its own.

> **Live status is in §0** (commit refs + the resume point). Boxes below: `[x]` done, `[~]` partial,
> `[ ]` not started. The two unfinished pieces are window geometry (§11.3 last box) and debounced
> autosave (§11.4).

### 11.1 Scaffolding & placement (via AssetLoader — see §4.4)
- [x] Create `src/Core/Session/` and add to `CMakeLists.txt` (lib + `utests`). *(`cb84229`)*
- [x] **Do NOT write `SessionPaths` for Phase 1.** Session file reached purely through `kProject`
      (`<cwd>/.goatedit/session.yml`):
  - [x] Write path: `assetLoader.ResolveWritePath("session.yml", kProject)`.
  - [x] Read: `assetLoader.LoadTextAsset("session.yml", kProject)`.
- [x] **Create `<cwd>/.goatedit/` before registering `kProject`** when in-home + `auto_create_project_dir`.
      Implemented as `Editor::EstablishProjectDir` (`06a9688`) using the new `ReplaceSearchPath` (single
      authoritative `kProject` path). Strict-subdir-of-home guard reused (`IsStrictSubdir`).
- [~] **Register/create `kProject` on folder-open** (§4.5): wired for the *init-time* `is_directory`
      branch (`EstablishProjectDir` called from `OpenDocumentOrFolder`). The **runtime** desktop-launch
      case (user picks a folder *after* startup → `SessionManager::OnFolderOpened`) is still a **stub** —
      tied to the not-yet-built folder picker (§4.5, out of Phase-1 scope, but the hook exists empty).
- [x] **Gate on folder vs file** (§3.3): `.goatedit/` + session only on the `is_directory` branch; a
      single-file open creates nothing and starts no session.
- [ ] *(Deferred to Step 2)* FNV-1a root-key, central outside-home fallback, `persist_external`.

### 11.2 Data model (DTOs + format)
- [x] `SessionState.h` DTOs (`cb84229`): `SplitterSession`, `WindowGeometrySession`, `LayoutSession`
      (geometry + splitters + `focusedTopView`), `SelectionSession`, `DocumentSession`, `RootSession`
      (`version`, documents, active-doc index, `extraRoots`, `expandCollapse` map, `LayoutSession`). Pure
      value types — no yaml-cpp, no logic.
- [x] YAML emit/parse — done in a dedicated `SessionSerializer` (`dd39f83`), **not** via `ConfigNode`, to
      keep yaml-cpp out of the view layer. `version` written + checked on read; `FromYaml` never throws.
- [x] **Atomic write**: `session.yml.tmp` → `fsync` → `rename` (`6e58c32`/`6e58cde`, in `SessionManager`).

### 11.3 Owner serialisation hooks (model serialises itself)
- [x] `DocumentViewState::ToSession()/FromSession()` (`55a5974`) — `LineCursor` + `Selection` (via new
      raw un-sorted `GetRawStart/GetRawEnd` so backward selections round-trip) + `viewMode`.
- [x] `Document::ToSession/FromSession` (`e8e8af1`) — path + aggregates `DocumentViewState`. **No dirty
      field yet** — deferred (not actionable until buffer-snapshot persistence, which is Step 3 undo).
- [x] `Workspace` (`d9c5bf1`) — `ToSession` enumerate open-docs + active index; `ReopenDocument` **skips
      missing/stale** (never aborts the restore); `FromSession` reopens + sets active.
- [x] `WorkspaceView` (`eea83f1`) — `Save/RestoreExpandCollapseState`; one-shot seed merged into
      `PopulateTree` then cleared (so the user can still collapse a restored node).
- [x] Splitters (`c1c591b`) — `ViewBase::SetSessionId` + opt-in `To/FromSession(LayoutSession&)` on
      `VSplitView` + `HSplitView` (HSplitViewStatus inherits). Stores abs+relative; restores relative,
      clamped at the `ClampSplitterPos` chokepoint. Tagged + walked in `4279a3e` (see §0).
- [x] **Window geometry — DONE (2026-06-15), with a layering inversion.** The graphics backend does
      **not** depend on `SessionManager`. `ScreenBase` exposes plain-data hooks (`SetRequestedWindowGeometry`,
      a `WindowGeometryChangedHandler` callback, `GetPrimaryDisplayBounds`) + `ResolveStartupGeometry`
      (default + clamp, out-params, no session/config). The glue `Editor::WireScreenGeometry` (in
      `SetupSDL2`/`SetupSDL3`, before `Open()`) reads the session geometry, feeds it in, and registers the
      write-back/autosave callback. **Deleted** `WindowLocation`, `RuntimeConfig::GetWindowLocation()`, the
      `winlocation` test module, and `gedit_lastwinloc.yml` (both backends). Cold-start = centered 80% of
      the display; restore clamps to display bounds. New `ScreenBase.cpp`. *(`WindowGeometrySession` DTO
      `IsValid()` sentinel still means "no stored geometry → cold-start default".)*

### 11.4 Wiring & triggers
- [x] `SessionManager` singleton (`cb84229`/`6e58cde`): `Instance()`+`Clear()`, no-arg `Save()`/`Load()`,
      `CurrentSession()` accessor, **sole owner of session disk I/O**. Loaded during `Editor::Initialize`.
      Not in `RuntimeConfig`. *(Debounce timer not yet — see autosave box below.)*
- [~] **Trigger** (§4.5): the init-time `is_directory` path is wired (`EstablishProjectDir` +
      `RestoreSession` in `Initialize`; `RestoreLayout` from `main.cpp`). `SessionManager::OnFolderOpened`
      for the **runtime** desktop-launch-then-pick case is still a **stub** (depends on the folder picker,
      §4.5 — out of Phase-1 scope).
- [~] **Apply restore on-demand.** Startup restore is done in **two passes** (documents in
      `RestoreSession` during init; layout in `RestoreLayout` from `main.cpp` after the view tree exists —
      see §0 for why the split is necessary). Restore into an *already-live* window (runtime folder-switch)
      is **not** wired (same OnFolderOpened gap).
- [x] Tag the two splitters in `main.cpp` (`4279a3e`): `split.workspace`, `split.terminal`. Walked via
      `ViewBase::CollectLayout/ApplyLayout` rather than an explicit registry list.
- [x] **Save** on `Editor::Close()` (`06a9688`) — captures docs + layout, idempotent.
- [x] **Debounced autosave — DONE (2026-06-15).** `SessionManager` owns the debounce `Timer` +
      `NotifyChanged()` with an injected `autoSaveHandler` (set by `Editor` → `SaveSession`, so
      `SessionManager` keeps no `Editor` dep); on elapse it posts the save to the main-thread runloop.
      Triggered from app-level chokepoints: `Workspace` doc add/remove/set-active, splitter `SetSplitterPos`
      (via `ViewBase::NotifySessionChanged`), and the geometry write-back callback. `session.autosave_timeout_ms`
      (default 1500, 0 = off). Mirrors `TextBuffer`'s autosave timer.
- [x] `Config` — `session:` section added to `config.yml` (`06a9688`); everything gated on `session.enabled`.
- [x] `AssetLoaderBase::ResolveWritePath` is the primary write API; added `ReplaceSearchPath` (`06a9688`)
      so a `kProject` re-point yields a single authoritative path. Returns the auto-created `.goatedit/` path.

### 11.5 Robustness (Phase-1 relevant only)
- [x] Restore tolerates: missing files (skip), unknown/newer `version` (ignore, start clean), and
      off-screen geometry (**done** — `ScreenBase::ResolveStartupGeometry` clamps to display bounds, tested
      in `session_geometry_resolve`). Out-of-range cursor clamp-to-buffer remains a watch-item.
- [x] Never create `.goatedit/` outside `$HOME` (`EstablishProjectDir` reuses `IsStrictSubdir`);
      `persist_external` default-off respected (outside-home roots are sessionless in Phase 1).
- [x] Corrupt/half-written `session.yml` doesn't crash startup — atomic write prevents a partial file;
      parse failure → log + start clean (`FromYaml` returns false, never throws).

### 11.6 Tests (`utests/test_session.cpp`, trun — **18 cases, all green**)
- [x] `RootSession` YAML round-trip incl. selection + view-mode + geometry (`yaml_roundtrip`); bad/newer
      version + corrupt input return false, never throw (`yaml_bad_version`, `yaml_corrupt`).
- [x] `DocumentViewState` round-trip + backward-selection direction preserved
      (`documentviewstate_roundtrip`, `selection_direction_preserved`).
- [x] Path resolution: `ResolveWritePath("session.yml", kProject)` →
      `<cwd>/.goatedit/session.yml` (`save_load_roundtrip`, `assetloader_replace_kproject`). **Made
      hermetic via `ReplaceSearchPath`** — a `.goatedit/` left under the test cwd by a real run was
      shadowing it (env pollution, not a code bug).
- [x] Reopen skips missing file, rest still restore (`workspace_reopen_skips_missing`).
- [x] Splitter round-trip (both axes) + untagged no-op + recursive walk over the real nested splitter
      shape (`splitter_roundtrip`, `splitter_untagged_noop`, `layout_walk_roundtrip`).
- [x] WorkspaceView expand/collapse persist + seed (`workspaceview_expandcollapse`).
- [x] Atomic write leaves no leftover `.tmp` (`save_load_roundtrip`).
- [x] Window-geometry policy: `ScreenBase::ResolveStartupGeometry` default/preserve/clamp + no-bounds
      fallback (`session_geometry_resolve`); `NotifyWindowGeometryChanged` forwards to the handler / safe
      no-op without one (`session_geometry_callback`). Pure graphics math — no session in the test.
- [x] `session` added to the **verified-green set** (now **221**) in `CLAUDE.md`'s list — *(pending: the
      CLAUDE.md resume-point prose still says 202/218; bump it when the branch merges to `main`)*.

### Definition of done (Phase 1)
Open project A (edit, scroll, split, move/resize window, expand some tree nodes, switch to hex on one
file), quit, relaunch `goatedit <A>` → files, active tab, cursors, scroll, selection, view-modes,
splitters, tree expansion, and window geometry all return. Three separate projects each round-trip
independently with no cross-talk. Full restore survives a file having been deleted on disk underneath it.
</content>
</invoke>
