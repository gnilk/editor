# Usability round - open files, splitters, workspace search

Status: **Planned 2026-11-08.** The code index is paused before its step 4 (`code-index.md`); this round comes first.
Order: the change set in the tree (§0), reloading files changed on disk (§0b), open files list (§1), dragging splitters
(§2), workspace search (§3). The note on key bindings for script commands (§4) is recorded, not scheduled.

## 0. The change set in the workspace tree

**Why:** working with an agent, the question is often "what has it touched?". The files in the current change set, what
`git status` lists, stand out in the workspace tree: in a colour of their own, with a letter at the right edge of the row
saying what kind of change it is.

**Where the answer comes from: `git status`, not the folder monitor.** The monitor says a file was written, not that it
differs from the last commit: a file changed and changed back is not in the change set, and a commit or a `git add` changes
it without touching the working tree. So git is asked, and the monitor only says when to ask.

**The call** (`GitStatus`): `git -C <root> rev-parse --show-toplevel --absolute-git-dir`, then `git --no-optional-locks -C
<root> status --porcelain=v1 -z --untracked-files=normal -- .`, in the background - the editor never waits for git.
- **Stable:** porcelain v1 is documented as stable across git versions and not changed by user configuration; `-z` gives
  names unquoted. What configuration could still change is given explicitly (untracked files).
- **`--no-optional-locks`:** a background status doesn't take the index lock while an agent runs git in the terminal, and
  doesn't rewrite `.git/index` - which the check below would see, and run status again, for ever.
- **`-- .`:** only what is under the workspace root. Paths still come from the top level, which may be above the root.
- **The git directory** comes from git: in a worktree or a submodule `.git` is a file pointing elsewhere.
- About 13 ms on this repository.

**When it runs** (`RepoMonitor`, one per project root):

| Trigger | Catches |
|---|---|
| The folder opens | The first state |
| The folder monitor reports dirty folders | Files written by anyone: an agent, the terminal, another editor, our own saves |
| `.git/index`, `.git/HEAD` or `.git/logs/HEAD` changes, checked by modification time every `check_interval_ms` (1 s) on a thread of its own - no process | commit, add, reset, checkout, switch, stash, merge: the change set changes, the working tree may not. `.git` is excluded, so the folder monitor never sees these. |

Nothing runs while nothing changes. One query runs at a time; asks meanwhile make one more after it. The change set is
built on git's thread and posted to the main thread only when it differs from the last.

**Found on the way - startup messages were dropped:** `Runloop::DefaultLoop` cleared both message queues when it started, so
anything another thread posted during startup was lost - here the first status, which arrives before the loop runs, so the
tree stayed plain until the folder monitor's first change. The clear is gone; what was posted runs on the first pass. The
two queues stay: they keep a message posted while one is processed from running in the same pass.

**No git, or not a repository:** one `rev-parse`, then nothing; nothing is marked. A repository whose status fails keeps
what is shown.

**Config (decided 2026-11-08):** `repomonitor.enabled` switches it, apart from the folder monitor: some don't want an
application looking around their disk without saying so. Both default to `yes`. With the folder monitor off, files
changed outside the editor without git (an agent writing them) aren't seen until the next git operation - said in the
`config.yml` comment rather than polled for.

**What is marked:**

| State (porcelain) | Letter | Theme key |
|---|---|---|
| Modified, staged or not (`M`, `T`) | `C` changed | `tree_modified` |
| Untracked (`??`). Git lists an untracked folder, not its files: what is inside it counts as untracked. | `N` new | `tree_untracked` |
| Added (`A`), copied (`C`) | `A` added | `tree_added` |
| Renamed (`R`, under the new name) | `R` | `tree_added` |
| Deleted (`D`) | `D`, and the row struck through | `tree_deleted` |
| Conflicted (`U`, `AA`, `DD`) | `!` | `tree_conflicted` |
| A folder holding any of these | no letter: the colour of the heaviest change below it - a collapsed tree still shows where the changes are | |

**The letter is at the right edge of the row** (decided 2026-11-08). First a `*`, then a letter in front of the name in a
column every file row kept, so names wouldn't move - but that column made the tree read thick at every level, and a 4-wide
indent to make room was worse. At the right edge the tree stays as it was, and the letters line up in one column. More
markers can join it as they come; a real gutter for the tree is possible later. Not git's letters: `M` / `??` / `U` read
worse, and `U` is "unmerged" in git but "untracked" elsewhere. `N` and `A` stay apart - created, and staged, are
different steps. `TreeView` has it as a generic option, a markers delegate asked when the tree is flattened; a name that
would run into the markers is cut with `...`. The other trees have none.

The theme importer maps VS Code's `gitDecoration.*Foreground` colours onto the keys. A theme without them shows the letter
alone.

**Looked up when a row is drawn, not stored on the nodes** (revised 2026-11-08, was: node metadata). A `ChangeSet` maps the
snapshot onto the paths the nodes have (the root as the workspace has it, whatever symlinks lie between) and is replaced as
a whole. Node metadata would go stale: a folder expanded later, or a reconcile that rebuilds its children, would show its
files unmarked until the next status. A lookup is a hash probe per visible row.

**Deleted files** are the one case that needs nodes: they aren't on disk, so nothing else puts them in the tree. Applying a
change set adds a node for each where its folder is in the tree and read (a folder not read yet shows the change on
itself). A reconcile keeps them. They go when the change set stops listing them - committed, or restored - unless the file
is back on disk or open. Nothing tracks commits as such: the next status no longer lists the file.

**Layering:** `Process` (`src/Core/`, generic, with the platform part in `ProcessUnix`, `src/Core/unix/`, as `FolderMonitor`
has its per-platform classes; `Process::Create` is defined by the platform's source, so a Windows port adds a file and the
base names no platform). Async by default on its own thread, stdout / stderr / exit as callbacks there, `WaitFor()`,
`Kill()`, a working directory and the exit code; it knows nothing of `Runloop`. Modelled on testrunner's `procspawn`, written
anew. Git is built on it by composition: `GitStatus` runs `Process`es, `ChangeSet` is plain data, `RepoMonitor` asks and
posts (`src/Core/Git/`). The workspace holds a monitor per root and draws nothing; the view asks `Workspace::ChangeOf`.

**Later:** for an agent's work on a branch: the change set against a base (`git diff --name-status main...`), not only
against the last commit. That set is also what the code index's §10.3 "what did this change do to the structure?" would
scan.

**Tests:** `process` (exit codes, output, large output, working directory, kill, async callbacks, stdin, a pipe held by a
grandchild) and `gitstatus` (the parser on fixed output, every XY code, real temp repositories, a root below the top level,
the change set's lookups and roll-up, the monitor seeing a commit through the git directory, and the workspace: marks, the
deleted node kept by a reconcile and gone after the commit).

## 0b. Reloading a file changed on disk

**Today:** nothing reloads. An open file that something else rewrites - an agent, `git checkout`, a formatter in the
terminal - keeps its old text in the editor, and a save writes the old text back over the new. Working with an agent this is
the common case, not the rare one. Nobody wrote it; it is a missing piece, not a bug.

**What is there already:** the folder monitor reports the folders that changed (on the main thread, `ProjectRoot::FlushDirty`),
the workspace knows the open documents and their paths, a buffer knows whether it has unsaved edits (`kBuffer_Changed`), and
a deleted file is already handled (kept, struck through, `IsMissingOnDisk`).

**Behaviour** (as VS Code and most editors do it):
- **No unsaved edits: reload, silently.** The cursor stays on its line (clamped), the view keeps its top line. The code index
  and the semantic colouring follow through the buffer's parse, as for any edit.
- **Unsaved edits: never touch the buffer.** Mark it in the header (`[!]`, next to `*` and `[D]`) and say so in the status
  line. The choice is the user's: reload and lose the edits (a quick command, `.reload`), or save, which overwrites the file
  on disk - asked first (`ConfirmModal`), since that drops what the other program wrote.
- **Our own saves don't reload.** Each document keeps the size and modification time of the file as it last loaded or saved
  it; only a file that differs from that is reloaded. That also skips a folder event for a sibling file.
- **Not the active file only:** every open document in a dirty folder is checked; a background tab reloads too.

**Built (2026-11-08):**
- **The stamp:** each document keeps the size and modification time of its file from its last load, from a save that
  wrote, and from a reload. A save of an unedited buffer writes nothing and keeps the stamp - else a file rewritten
  meanwhile would be taken as ours and never reload.
- **The check:** on the folder monitor's flush, every open document in a dirty folder (all of them on lost events) whose
  file differs from its stamp. `Workspace::CheckOpenDocumentsOnDisk`, also for a later check on window focus.
- **The reload replaces only what differs** (`Document::ReloadFromDisk`): the lines the same at the start and at the end
  stay - the same `Line`s, colours and all - and the middle goes through `ReplaceLineRange`, which re-parses that region
  only. Lines are read as a load reads them (`TextBuffer::ReadLines`, shared with `Load`), so like is compared with like.
- **Undo:** the reload is one undo step (decided 2026-11-08): what another program wrote can be stepped back, as VS Code
  does.
- **Unsaved edits:** kept, `[!]` in the header, a status line message.
- **The autosave** checks the file before it writes: autosave is on by default, 2 s after the last key, so without the
  check it would write over an agent's rewrite within seconds - before the folder monitor had even said so. It marks the
  document instead.
- **A bug found on the way:** every scanned node starts with its "path changed" flag set (it is meant for save-as), and
  only a save cleared it, so the first save of any file force-wrote the buffer, edited or not - over a file an agent had
  rewritten meanwhile. A load now clears it: a document read from that path is in step with it.

**Still to do:** `.reload` to take the file and drop the edits; an explicit save of a `[!]` document asks first
(`ConfirmModal`), as it writes over what the other program wrote - today it saves as before. A check on window focus.
Large files are read on the main thread, like a normal open.

**Tests:** `reload` - clean reload, unchanged lines kept as they were, undo, lines only added / only removed / everything /
emptied, cursor on a shorter file, unsaved edits kept and the file untouched, our own save, a save of an unedited buffer,
the autosave refusing, another folder's flush. Three mutations (no autosave check, a stamp on every save, a reload of every
line) each fail one test.

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
