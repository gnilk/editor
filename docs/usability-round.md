# Usability round - open files, splitters, workspace search

Status: **Planned 2026-11-08.** The code index is paused before its step 4 (`code-index.md`); this round comes first.
Order: the change set in the tree (§0), open files list (§1), dragging splitters (§2), workspace search (§3). The note on
key bindings for script commands (§4) is recorded, not scheduled.

## 0. The change set in the workspace tree

**Why:** working with an agent, the question is often "what has it touched?". The files in the current change set, what
`git status` lists, should stand out in the workspace tree: in a colour of their own, perhaps with a `*` in front as the
header has for unsaved buffers.

**Where the answer comes from: `git status`, not the folder monitor.** The monitor says a file was written, not that it
differs from the last commit: a file changed and changed back is not in the change set, and a commit or a `git add` changes
it without touching the working tree. So git is asked, and the monitor only says when to ask.

- **The call:** `git -C <root> status --porcelain=v1 -z`, in the background: the editor never waits for git. The result is
  posted to the main thread (`Runloop::PostMessage`) when the process has finished, and only then put on the nodes. About 13 ms on this repository. Paths come
  relative to the repository's top level (`git rev-parse --show-toplevel`), which may be above the workspace root.
- **When:** after the first scan; when the folder monitor reports dirty folders (debounced, it often reports in bursts);
  after the editor saves a file; and when `.git/index` or `.git/HEAD` changes. `.git` is excluded outright, so the monitor
  never sees those two: their modification times are checked on an idle tick instead. Nothing runs while nothing changes.
- **No git, or not a repository:** nothing is marked, nothing is reported.

**What is marked:**

| State (porcelain) | Shown | |
|---|---|---|
| Modified, staged or not (`M`) | `*` and the modified colour | |
| Added (`A`), untracked (`??`) | the added colour | Untracked folders are listed as the folder, so the folder is marked. |
| Renamed (`R`) | as added, under the new name | |
| Deleted (`D`) | a struck-through row, as for a file gone from disk | not in the tree today, so a node is added for it |
| Conflicted (`U`) | the conflict colour | |
| A folder holding any of these | a lighter mark | so a collapsed tree still shows where the change is |

Colours are theme keys next to the existing tree ones, with fallbacks. `TreeView` has style and dim delegates but no
colour one; a colour delegate is the generic addition.

**Layering:** a small `GitStatus` below the workspace: run the command, parse the output into plain data (path → state).
It knows nothing about nodes or views. The workspace puts the states on its nodes (node metadata, like `ignored` and
`missing_on_disk`), the view draws them. Running a process needs a small runner; today the only process code is the terminal's
`forkpty`. A `Process` in `src/Core/unix/` (macOS and Linux), modelled on testrunner's `procspawn` (`posix_spawn`, pipes,
`poll`): async by default on its own thread, stdout / stderr / exit as callbacks there, `WaitFor()` to wait for completion,
`Kill()`, a working directory and the exit code. It knows nothing of `Runloop`; the caller posts to the main thread.
The core stays generic: anything git-specific is built on it by composition (`GitStatus` holds and runs a `Process`), not by
deriving from it.

**Decided (2026-11-08):** `*` is reused: in the tree it means "in the change set", in the header "unsaved". Deleted files are
shown, struck through.

**Later:** for an agent's work on a branch: the change set against a base (`git diff --name-status main...`), not only
  against the last commit. That set is also what the code index's §10.3 "what did this change do to the structure?"
  would scan.

**Tests:** the porcelain parser on fixed output (renames, spaces in names, `-z`), the roll-up to folders, and a temp
repository with one modified, one untracked and one staged file.

## 1. Open files - the header overflows, `Cmd+E` lists them

**Today:** `EditorHeaderView` draws every open file's name in a row and never checks the width, so names past the right
edge are cut off, including the active one.

**Change:**
- **Stop at the edge** and end the row with `...`.
- **Keep the active file in sight:** if it would fall past the cut, the row starts later so that it is shown.
- **The list:** `Cmd+E`, `Ctrl+E` on Linux (functionality, see `CLAUDE.md`, Key bindings; JetBrains uses it for recent
  files). A modal lists the open files with the markers the header has (`*` changed, `R` read-only, `[D]` gone from disk),
  the active one selected; Enter switches. `ListSelectionModal` is most of the way there.
- **Clicking `...`** opens the same list.

**Not `Cmd+L`:** that is `ReformatLine` (native, `Document::OnActionReformatLine`). See §4.

## 2. Dragging the splitters with the mouse

**Today:** splitters move with `Option+W/A/S/D` (`Option+M` maximises the height). The SDL backends send press, release and
the wheel, but no moves; `kMouseEventKind_Move` is only filled in by the Gansi screen. Each mouse event is hit-tested on its
own.

**Change:**
- **Moves while a button is held,** from both SDL backends (keep them in step).
- **Mouse capture in `RootView::DispatchMouse`:** a press that a view takes for a drag captures the mouse; moves and the
  release go to that view until the release, wherever the pointer is. Focus-follows-click stays out of a drag. Generic, in
  the UI layer, so drag-select can use it later (`done/mouse-support.md` P4).
- **The splitters:** a press on the divider row (`HSplitView`) or column (`VSplitView`) is not covered by either child, so
  the hit lands on the split view itself today. On a move it calls `SetSplitterPos`, which already clamps and saves the
  position to the session. `HSplitViewStatus` draws the status line on its divider row, so the status line is the handle.
- **Feedback:** none needed at first; the views follow the pointer. A cursor shape change is a later nicety.

**Tests:** capture routing on a view tree without a screen (press captures, moves outside still arrive, release ends it,
no focus change mid-drag), and a splitter moved by a press and moves stays clamped.

## 3. Workspace search - three kinds

All three are wanted. Each is its own piece, smallest first.

| | What | Key | From |
|---|---|---|---|
| a | Type in the workspace tree to jump to the next matching name | letters, in the tree | The tree's own nodes |
| b | Open a file by part of its name from anywhere | `Cmd+P` | All the workspace's files, exclusions applied |
| c | Plain text search across the workspace, results in a list | `Cmd+Shift+F` | The files, read in the background |

- **a)** Letters typed in the workspace view build a short prefix (reset after a pause), and the selection moves to the next
  visible match. Whether it should also filter the tree is open.
- **b)** Not the code index: that holds only the files a structure scanner reads (C/C++). The list comes from the
  workspace's tree, with `ExclusionRules` applied. Ranked like Go to symbol (`CodeIndex::MatchName`: prefix, word starts,
  subsequence), closeness to the active file next.
- **c)** The code index's D1: "after the symbol index", which has shipped. Search runs off the UI thread, results arrive as
  they are found, each row a file, a line and the line's text in colour (as the relationship view does). Needs more design
  before it starts: literal or regex, case, limits, what a big tree costs.

## 4. Note - key bindings for script commands (not scheduled)

The script commands (`src/Plugins/jsplugs.yml`, run from quick command mode as `.name` or the short form) can't be bound to a
key today; keymaps only know the native `kAction` / `kUIAction` names. Allowing a key binding for a script command would let
rarely used native actions move to quick commands, while anyone who uses one often can still bind it to a key:
- `ReformatLine` and `ReformatBlock` (now `Cmd+L` / `Cmd+I`) could become `.fm_line` and `.fm_file` (names open). They need
  a reformat call in the JS API (`DocumentAPI`) first.
- A binding could name the command, say `Command: "fm_line"`, next to the action names, resolved when the key fires.
