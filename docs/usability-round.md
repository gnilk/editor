# Usability round - open files, splitters, workspace search

Status: **Planned 2026-11-08.** The code index is paused before its step 4 (`code-index.md`); this round comes first.
Order: the change set in the tree (§0), reloading files changed on disk (§0b), changed lines in the gutter (§0c), open
files list (§1), dragging splitters (§2), workspace search (§3), quick commands: close all and `!` (§5), headless drawing that can be checked (§5b), glyphs beyond
code point 255 on SDL (§6). The note on key bindings for script commands (§4) is recorded, not scheduled; the
overview - a minimap on SDL, then the terminal (§7) - is planned, not scheduled.
**Next:** §3, workspace search - everything before it in this round is built (§0, §0b, §0c, §1, §2, §5, §5b, §6).

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

**Built (2026-11-09), with §5's `!`:** `.reload` / `.rl` takes the file - at once when unedited, asking first (Reload /
Cancel) when it would drop edits; the reload is one undo step, so they can be stepped back to. `.s` over a file another
program rewrote (`[!]`, or rewritten since and not yet noticed) asks: Overwrite / Reload / Cancel. `.rl!` and `.s!` don't
ask. `DocumentAPI` gained `IsChangedOnDisk()` and `Reload()`. There is no native save action - saving is `.s` and the
autosave, which already refuses.

**Still to do:** a check on window focus. Large files are read on the main thread, like a normal open.

**Tests:** `reload` - clean reload, unchanged lines kept as they were, undo, lines only added / only removed / everything /
emptied, cursor on a shorter file, unsaved edits kept and the file untouched, our own save, a save of an unedited buffer,
the autosave refusing, another folder's flush. Three mutations (no autosave check, a stamp on every save, a reload of every
line) each fail one test.

## 0c. Changed lines in the gutter

**Why:** the change set (§0) says which files an agent touched; the next level is which lines, in the file you have open.
The gutter - where the line numbers are - marks each line that differs from the last commit, so the change can be read
in place instead of in a diff.

**What is marked**, against `HEAD` as the change set is:

| Change | Mark | Theme key |
|---|---|---|
| Added line | a bar | `gutter_added` |
| Changed line | a bar | `gutter_modified` |
| Lines removed here | a low mark on the line after them (on the last line, when the end was removed) | `gutter_deleted` |

The mark goes in the gutter's free column, between the number and the active line's `*` (`    12│ *`), so nothing
moves. A coloured `│` (U+2502): the default font (Andale Mono) has it, not the thinner `▎` (U+258E). The SDL backends
draw it since §6. The importer maps VS Code's `editorGutter.addedBackground` / `modifiedBackground` / `deletedBackground` onto the
keys. A file git doesn't track yet (`N` in the tree) is all added; a file outside a repository has no marks.

**Against the buffer, not the file:** the marks follow what you type, unsaved edits included - the question is "what
differs from the commit", and the answer shouldn't wait for a save. So the comparison is the buffer's lines against the
file as it is in `HEAD`, done in the editor:
- **The base:** `git show HEAD:<path>` through `Process`, once per open file, kept until `HEAD` moves - which the
  repository monitor already notices (`.git/HEAD`, `.git/logs/HEAD`, §0). Read in the background; the marks come when
  it is there.
- **The diff:** a line diff (Myers, `O(ND)`: cheap when the differences are few, which is the common case) of the base
  against the buffer's lines - plain, in `src/Core`, tested on strings. Run off the main thread after a pause in typing,
  as the structure pass is (`TextBuffer`'s pause), and after a reload (§0b) or a change of the base. Not per keystroke.
- **The result:** per document, plain data - for each line added / changed / nothing, and where lines were removed. The
  gutter only draws it.

**Layering:** `GitStatus`'s neighbour in `src/Core/Git/` asks git for the base (composition, on `Process`); the diff knows
nothing of git; the document holds the result; `GutterView` draws it. Nothing in the gutter runs git.

**Later:**
- **Next / previous change** (`F7`-like, key open): the result is the list to step through.
- **The base:** against the index (what is staged) or a branch's base (`main...`, as §0's later), switchable.
- **For an agent:** "which lines did this change touch" per file - the same data, asked from outside (code index §10.3).
- **Hover or a key on a mark:** the base's lines that were there (the `+/- 5 lines` pop-up of the relationship view).

**Built (2026-11-09):**
- **Hashes, not text:** a line's hash is kept on the `Line` (`ContentHashLocked`), cleared when its text changes. After a
  pause in typing the buffer hands its line hashes to the document - one walk over the lines per pause, as the
  structure pass makes, hashing only the edited lines; no text is copied. The base is hashed the same way
  (`Line::HashOf`), from git's text split as a load splits a file (`TextBuffer::SplitLines`, next to `ReadLines`).
- **The pause hook** is the buffer's, for any language (`TextBuffer::SetLineHashesHandler`): after every parse, at
  the parse thread's idle tick, or at once without a parse thread. `RequestLineHashes()` asks again when a new base
  arrives. The document listens only while it has a base, so a file outside a repository costs nothing.
- **`LineDiff`** (`src/Core/`): Myers on the hashes, after the common start and end are taken off; past 2000
  differences the middle is marked changed. A run of removed and added lines between equal ones: as many changed as
  both sides have, the rest added, more removed leaves a mark on the line after (`kRemovedAbove`; the last line's
  `kRemovedBelow` when the end went).
- **`GitBase`** (`src/Core/Git/`): `git show HEAD:./<name>` from the file's folder. Failing that, `git status` of the
  file: untracked or staged as added - all added (a repository with no commit too); renamed - the old name's text;
  ignored, no repository - no marks. Binary - no marks.
- **The glue** (`Editor::RefreshChangeBase`): on open, and after every applied change set (a commit, a checkout - the
  workspace's `SetChangeSetAppliedHandler`), for every open document; git, the split and the hashes off the main
  thread, the document set on it. A checkout that leaves the change set as it was changes the files, so the reload
  re-diffs - against the old base until the next change set. Switched with `repomonitor`.
- **Drawn:** column 6, between the number and the `*`. `│` for added and changed; `‾` (U+203E) for removed above
  and `_` for removed below the last line - Andale Mono has neither the eighth blocks (U+2581, U+2594) nor `▸`. A
  line's own change wins over a removal next to it. Colours `gutter_added` / `gutter_modified` / `gutter_deleted`,
  falling back to the tree's; the importer takes VS Code's `editorGutter.*Background`, then `gitDecoration.*`.

**Tests:** `linediff` (same, added, removed, changed, moved, empty either side, 100 000 lines with two changes, the
limit, and a sweep over every insert/remove at every place), `changemarks` (the base from temporary repositories:
committed, new, staged, no commit yet, ignored, outside, binary, renamed; the split; a document's marks following
unsaved edits, all added, cleared; and the gutter drawn into the headless grid - the mark per row, added and changed
apart from each other and from the numbers), `themeimport` (the gutter keys). Not drawing the marks fails the drawn
test.

**Planned tests (as designed):** the diff on strings (only added, only removed, changed, moved blocks, an empty base, an empty buffer, a large
file with one change - the `O(ND)` case), the marks for each, the base from a temp repository, and the marks following an
unsaved edit.

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

**Built (2026-11-08):**
- **The header** (`EditorHeaderView::LayoutEntries`, plain and tested apart from drawing): the entries that fit, joined by
  ` | `; a cut end shows `...` and a separator. The row starts later when the active file would fall past the edge, so it
  is always shown - a file too wide on its own is shown anyway, and the screen cuts it. Each `...` is clickable and opens
  the list.
- **The list** (`OpenFilesModal`) is a `QueryListModal`, not `ListSelectionModal` (unused, older): typing filters by name
  (the code index's `MatchName`: prefix, word starts, subsequence - best first, ties in the header's order), the folder
  relative to its root sits dimmed on the right, Enter switches and goes to the editor view - from the terminal or the tree
  too. Nothing typed: the active file is selected (`QueryListModal::OnInitialRow`, a new hook).
- **One set of markers:** `Workspace::StatusMarkers` (`*`, `R`, `[!]`, `[D]`) for the header and the list.
- **The key:** `ShowOpenFiles`, `Cmd+E` / `Ctrl+E`. On Linux, `Ctrl+E` in the terminal view is the editor's, not the
  shell's end of line - as `Ctrl+T` and `Ctrl+B` already are there.

**Found while testing - the tree jumped to its top:** switching the active document re-lays out every view
(`Editor::SetActiveDocument` -> `RootView::Initialize()`), and `TreeView::ReInitView` set its top row to 0 while keeping
the selection - so opening a file from the tree, switching from the header or `Cmd+E`, or a resize left the tree at its
first row and the selected file off screen. A re-layout now keeps the scroll position, moved only as far as needed to keep
the selection in sight (`TreeView::FitViewToHeight`).

**Tests:** `openfiles` - the header layout (all fit, cut at the end, at the start, at both, one too wide, and a sweep over
widths, rows and active files: the active one is always shown and what is drawn fits), the filter, the folder shown.

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
- **Feedback:** none needed at first; the views follow the pointer. A cursor shape change is a later nicety (built, below).

**Tests:** capture routing on a view tree without a screen (press captures, moves outside still arrive, release ends it,
no focus change mid-drag), and a splitter moved by a press and moves stays clamped.

**Built (2026-11-08):**
- **The drag, in the UI layer, for any backend:** `ViewBase::OnMouseDragStart` / `OnMouseDrag`. On a press,
  `RootView::DispatchMouse` asks the views from the outside in, before focus and before the view under the pointer; the
  one that takes it gets every move and the release, wherever the pointer goes. A press with no release before it (let go
  outside the window) ends the drag and is a new press.
- **The terminal splitter** (`HSplitView`, so `HSplitViewStatus`): its divider row - the status line - with the left
  button. **The workspace splitter** (`VSplitView`) has no column of its own: the divide is the left panel's last column,
  which the drag takes before the tree sees the press. Both follow the pointer through `SetSplitterPos`, which clamps and
  saves the position.
- **The workspace divide is drawn:** a dimmed `|` (`gutter_foreground`) down the left panel's last column, drawn by the
  `VSplitView` after its panels (`DoDraw`) so they don't paint over it. No column is spent: that cell was the air gap
  right of the change letters already. The column right of the line - the gutter's padding - is a handle too: one cell is
  easy to miss. Tried before a plain colour change, the other way (2026-11-08). Not a `VSplitViewStatus`: a column that
  carries nothing costs space on an SSH terminal, and the change letters belong to the tree's rows.
- **`|`, not `│`:** the SDL font atlas holds code points 32-255 only (`stbttf.h`), so `│` (U+2502) drew nothing - the cell
  was filled, the glyph skipped. The ASCII `|` draws on every backend, as a dashed line on SDL (it doesn't fill the cell's
  height). `│` came back with §6 (2026-11-09). A `DrawVRule` next to `DrawHRule` (a 1-pixel line on SDL) was the other way; not taken
  - the glyph is the general fix.
- **The backends:** SDL3 and SDL2 send a move while a button is held, and only when the pointer enters another character
  cell - SDL reports every pixel, and each move lays the views out again. Gansi needed nothing: it already asks the
  terminal for motion while a button is held (mode 1002) and turns it into moves.
- **SDL2 on macOS:** `cmake -B cmake-build-sdl2 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DGEDIT_BUILD_SDL3=OFF
  -DGEDIT_BUILD_SDL2=ON`, with Homebrew's `sdl2` - which is `sdl2-compat`, the SDL2 API on SDL3: it checks our SDL2 code,
  not the Linux SDL2 itself.

**Tests:** `layout` - a drag of the terminal splitter (past the bottom and outside the view: clamped; the panels see
nothing; after the release a move is nobody's), of the workspace splitter over its panel (the divider follows, the next
column is the panel's, another button isn't a drag), and no focus change or quick-command leave, a lost release.

**The pointer's shape (built 2026-11-09):** over a divider the pointer is a resize arrow, over text an I-beam, elsewhere
the arrow - and a drag keeps its shape wherever the pointer goes.
- **The views decide, the backend draws:** `ViewBase::PointerShapeAt(x, y)` (none: no opinion) and `DragPointerShape()`;
  `RootView::PointerShapeUnder` asks from the outside in, as a press for a drag is asked, so a divider beats the panel
  under it. The splitters share `IsOnDivider` between the shape and the drag. `EditorView` and `TerminalView` say text.
  `ScreenBase::SetPointerShape(kPointerShape)` is plain data (`Core/UI/Graphics/PointerShape.h`).
- **A hover:** `MouseEvent::kMouseEventKind_Hover`, a move with no button held, sent by the SDL backends only when the
  pointer reaches another cell. `RootView` takes it for the shape alone: no view sees it, nothing is laid out. After a
  press or a release the shape is asked again (a drag starts or ends there). Only a change reaches the screen.
- **SDL:** system cursors, made the first time each is shown, freed on close. SDL3 `DEFAULT` / `TEXT` / `EW_RESIZE` /
  `NS_RESIZE`; SDL2 `ARROW` / `IBEAM` / `SIZEWE` / `SIZENS`. Gansi: no pointer (it doesn't ask the terminal for hover
  motion); `SetPointerShape` does nothing there.
- **Stale until the next move:** a splitter moved by a key leaves the shape as it was until the pointer moves a cell.
- **Tests:** `layout` - `pointer_shape` (a divider over a panel, the status row, text, a drag over text and its release)
  and `pointer_hover` (a hover reaches no view and moves nothing; only changes reach the screen).

## 3. Workspace search - three kinds

All three are wanted. Each is its own piece, smallest first.

| | What | Key | From |
|---|---|---|---|
| a | Type in the workspace tree to jump to the next matching name | letters, in the tree | The tree's own nodes |
| b | Open a file by part of its name from anywhere | `Cmd+P` | All the workspace's files, exclusions applied |
| c | Plain text search across the workspace, results in a list | `Cmd+Shift+F` | The files, read in the background |

- **a)** Letters typed in the workspace view build a short prefix (reset after a pause), and the selection moves to the next
  visible match. Whether it should also filter the tree is open.
  **Built 2026-11-09:** `Core/UI/TypeToFind` (plain, `test_typetofind`) in the generic `TreeView::FindByTyping`, so any
  tree has it; `WorkspaceView::HandleKeyPress` gives it the letters no binding took (not with Ctrl/Cmd). Jump only, no
  filter. Case doesn't count; visible rows only; wraps. While the prefix grows the row it is on counts ("do" stays on
  `docs/`); one letter goes to the next match, and one letter repeated steps through them unless a name starts with the
  repeat - Finder's rules. A tree action (an arrow) or a pause (`workspaceview.type_to_find_pause_ms`, 1000) starts a new
  name.
- **b)** Not the code index: that holds only the files a structure scanner reads (C/C++). The list comes from the
  workspace's tree, with `ExclusionRules` applied. Ranked like Go to symbol (`CodeIndex::MatchName`: prefix, word starts,
  subsequence), closeness to the active file next.
  **Built 2026-11-09:** `QuickOpenModal` (`Cmd+P` / `Ctrl+P`; next/previous search hit moved to `Cmd+G` /
  `Cmd+Shift+G`, Ctrl on Linux). Not the workspace tree either - it is scanned lazily, folder by folder, so it doesn't
  hold the files below a closed folder. `Core/FileSystem/FileFinder` walks the project roots (the code index's: not the
  startup folder's root, not one holding the home directory) off the UI thread with each root's `ExclusionRules` - an
  ignored folder isn't entered, symlinked folders aren't followed, 200k files at most. Each `Cmd+P` walks again; the
  modal lists the last walk's files at once and takes the new list when it is in (a redraw, then `Requery`). Ties after
  the match: open files, then the active file's folder, then the shorter path. A query with a `/` matches the path
  below the root (`core/doc`). Opening goes on the navigation stack (`JumpBack` returns); an open file keeps its cursor.
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

## 5. Quick commands - close all, and `!` to skip the question

**Close all:** `.closeall`, short `.ca`, a script command next to `.cb` (`closeactive.js`). Closes every open document.
With unsaved edits in any of them it asks as `.q` does - Save all (then close), Discard (close, the edits are dropped),
Cancel - listing the documents. `.ca!` closes them all at once, dropping the edits.

**`!` - do it, don't ask** (vi's `:q!`): one convention for every command that asks before it drops something.
- **Today:** `.cb` and `.q` already ask, and already take `!` to skip it - but only as a separate argument (`.cb !`). The
  executor (`PluginExecutor::ParseAndExecuteWithCmdPrefix`) takes the first word as the command's name, so `.cb!` looks for
  a command called `cb!` and finds none.
- **Change:** the executor takes a `!` at the end of the command's name off and passes it as the first argument, `!`. So
  `.cb!`, `.q!`, `.ca!` work, `.cb !` keeps working, and no script changes: they already look for a `!` argument. A command
  that has no question to skip ignores it.
- **The commands it fits:** `.q!` exit, `.cb!` close the buffer, `.ca!` close all - dropping unsaved edits; and from §0b:
  `.reload` asks before it drops the edits of a `[!]` document and `.reload!` doesn't, `.s` asks before it writes over a
  file changed on disk and `.s!` doesn't.
- **Help:** `.h` lists the commands; one that takes `!` says so (a `force:` line in `jsplugs.yml` with what `!` skips).

**Built (2026-11-09) - the `!`:** `PluginExecutor::SplitCommandLine` takes a `!` off the end of the name and passes it
as the first argument; empty words are dropped, and an empty command line no longer indexes an empty list. `.cb!`,
`.q!`, `.rl!`, `.s!` work; the scripts were unchanged except `.s`, which now has a question to skip. The `force:` line
for `.h` wasn't added: the descriptions say what `!` does, as `.cb`'s and `.q`'s already did.

**Built (2026-11-09) - close all:** `.closeall` / `.ca` (`closeall.js`), as `.q` asks: Save all / Discard / Cancel,
listing the edited ones; a failed save closes nothing. `EditorAPI::CloseAllDocuments()` closes them all (a never-saved
one leaves the tree, as with `.cb`).

**Tests:** `jsengine` - `bangsuffix` (`.cb!` / `.cb !` / `.cb`, an argument ending in `!` left alone, a name that is only
`!`, empty lines), `reloadcmd` (unedited: no question; edited: Cancel keeps, Reload drops; `.rl!` through the command
line), `savecmd` (untouched: saved; rewritten: Cancel writes nothing, Overwrite, Reload takes theirs; `.s!`). An
`IsChangedOnDisk` that is always false fails `savecmd`; an executor that keeps the `!` on the name fails `reloadcmd`.
`closeallcmd` (an edited and an unedited document: Cancel keeps both, Discard closes both and writes nothing, Save
all writes then closes; `.ca!`; nothing edited: no question).

## 5b. Headless drawing that can be checked

**Why:** the headless backend (`--backend headless`, what every `trun` case runs on) drops every draw call:
`HLScreen` hands out windows over the base `DrawContext`, whose virtuals are empty. Layout maths can be tested, but not
what ends up on screen - the workspace divide in its column, a header cut with `...`, a change letter at the right edge, a
modal's frame. Those were checked by eye, with a screenshot or a `fprintf` added and removed again; nothing stayed behind
as a test.

**The grid is already there:** the Gansi backend composites every window into one cell grid (`gnilk::ansi::Terminal`'s
back buffer: per cell a code point, fg, bg, attributes), with the real window offsets and clipping, and only then turns it
into ANSI. `test_gansibackend` drives it over a `MockTerminalIO` and asserts on cells. That is the analytical renderer; it
was only used to test itself.

**Change:**
- **Headless draws into a Gansi grid.** `HLScreen` becomes a `GansiScreen` over a terminal IO that keeps nothing it is
  sent (`HeadlessTerminalIO`: a fixed size, no input, writes dropped - no ANSI piling up over a test run). Same size as
  before (100 x 100 cells), so the existing tests see the same layout. Without the ANSI backend in the build
  (`GEDIT_BUILD_ANSI=OFF`) headless stays the old no-op screen.
- **Read back as text:** `HLScreen::Dump(rect)` gives the grid's characters, one line per row - for a test to compare a
  region against, and to look at a layout without a display. `HLScreen::CellAt(x, y)` for the colour and attributes.
  A test reaches it through `HLScreen::FromRuntime()` (null when the headless screen isn't the grid one).
- **Assert on the grid, not on a log of draw calls:** a call log breaks when the drawing order or batching changes while
  the screen stays the same; the grid is the result. Tests assert the property ("the divide is in the left panel's last
  column, after a drag too"), not a whole-screen snapshot.

**Where it stops:** cells, not pixels. How SDL draws a glyph (the dashed `|`), font metrics, whether a code point
rasterises: those stay with the font code's own tests (§6, without a screen) and a screenshot. A view that draws through
SDL-only calls with no cell equivalent shows nothing in the grid.

**Built (2026-11-09):**
- **`HLScreen`** (`src/Core/Graphics/Headless/HLScreen.h`) is a `GansiScreen` over `HeadlessTerminalIO`, 100 x 100 cells,
  with `Dump()`, `Dump(rect)`, `CellAt(x, y)`, `ClearGrid()` and `FromRuntime()`. The no-op screen stays behind
  `#ifndef GEDIT_USE_ANSI`. The whole verified-green set passes on it unchanged, `gansibackend` included (619 cases).
- **A view draws only with a window:** a bare `ViewBase` has none (`window` is null, and `DoDraw` dereferences it), so
  a test's panels make their own - `FilledView` in `test_layout` fills its window with `.`, which also shows that the
  divide is drawn over the panel, not under it.
- **Headless now opens a Gansi screen,** so `GansiScreen::Open` sets the clipboard's update callback (OSC 52 into the
  dropped output). Tests that watch the clipboard set their own callback, as before.

**Tests:** `layout` - `draw_vsplit`: the workspace divide is a `|` on every row of the left panel's last column, over
the panel and nowhere else, and follows a drag; the row read back with `Dump`. Not drawing the divide fails it. More views
move over as they are touched (the tree's change letters next).
`openfiles` - `drawn`: real open documents in the editor's workspace, the header drawn at widths 20-90 with each file
active - the active name is drawn whole and underlined, nothing past the header's width, `...` exactly when a name is left
out. Not underlining the active name fails it; so does a row that never starts later.

**Found by the drawn header test:** when the active name doesn't fit with `... | ` on both sides (a narrow editor, a long
name), `LayoutEntries` shows it anyway and the screen cuts it - so the active name is the part cut off. The layout test
allowed it ("too wide alone"); drawn, it reads as a bug. **Fixed (2026-11-09):** the `...` before it goes - when the
active one is left alone and has no room for a `... | ` in front, `LayoutEntries` clears `isCutBefore`, so the row
starts `[Files] <name>` and the screen cuts the name's end, not its start. The `...` after it still opens the list.
The drawn test now checks those widths too (the name starts right after `[Files] `); without the fix it and the layout
tests fail.

## 6. Glyphs beyond code point 255 on SDL

Status: **built 2026-11-09.**

**Today:** both SDL backends (`SDL2/ext/stbttf.h`, `SDL3/ext/stbttf.h` - two copies, already drifted apart) pack code points
32-255 into one texture when the font opens, and skip anything else when drawing: `│`, `▎`, `→`, box drawing, accented
letters past Latin-1, all draw as an empty cell. Gansi has no such limit - the terminal draws the text.

**Change - a page table, filled on demand:** split a code point into a page (`cp >> 8`) and a slot (`cp & 0xff`).
- **The lookup** is two array indexes, `pages[cp >> 8]->glyphs[cp & 0xff]` - no hashing, no search. The only extra test
  over today's `cp < 256` is whether the page exists: a null pointer, the one branch, and it is the miss.
- **A miss builds the whole page,** 256 code points in one `stbtt_PackFontRange` into its own texture - the code the font
  already runs for page 0, with another start. Pages are few in practice (Latin, box drawing `0x25xx`, arrows `0x21xx`,
  maybe CJK), so a page per texture keeps it simple; the draw switches texture between pages, cheap at our character counts.
- **Page 0** is built when the font opens, as today; nothing changes for ASCII text.
- **The table:** 4352 page pointers for all of Unicode (`0x110000 >> 8`), about 34 KB per font, or a smaller table grown
  on demand. A page that has been tried is never built again.
- **Not in the font:** `stbtt_FindGlyphIndex` gives 0. Mark that slot missing when the page is built and draw nothing (or
  a placeholder box), so a missing glyph costs a lookup, not a retry. Andale Mono lacks `▎` - this is where a **fallback
  font** would come in (Menlo on macOS, DejaVu Sans Mono on Linux): build the missing slots of a page from the next font.
  Later; the page table is the place for it.
- **The grid stays monospaced:** a glyph is drawn into its cell, advance or not. Wide glyphs (CJK, emoji over two cells)
  are a separate question.

**One copy:** the page table, the missing marks and the build-on-miss know nothing of SDL - only uploading a page's bitmap
to a texture and drawing from it differ between SDL2 and SDL3. Move the font code into one shared place with those two
as the backend's part, instead of fixing it twice.

**Tests:** without a screen - the page and slot of a code point; a lookup in page 0; a miss builds the page once; a code
point the font lacks is marked missing, not retried - against the shipped `Assets/Resources/Andale Mono.ttf` (has `│`,
lacks `▎`).

**Built (2026-11-09):**
- **`GlyphFont`** (`src/Core/Graphics/GlyphFont.{h,cpp}`, in the core sources - no SDL): the page table (4352 slots),
  a page built on a miss with `stbtt_PackFontRanges` over only the code points the font has - `stbtt_FindGlyphIndex`
  first, since stb would otherwise pack the font's "no glyph" box for a missing one. A page tried once stays, empty or
  not. The bitmap starts at 128 px and doubles up to 4096. Page 0 is built when the font opens.
- **One cell per character:** the old code advanced the pen by each drawn glyph and skipped anything past 255 without
  advancing - so a `│` also pulled the rest of the line one cell left. Now every character advances by the cell (a
  space's advance), drawn or missing. Same as before for a monospaced font.
- **The checked bytes:** stb_truetype reads without bounds checks, so junk bytes crashed it. `GlyphFont` first checks
  the sfnt tag and that the table directory fits; the tables themselves are trusted.
- **The backend part** is `SDLFont` per backend (`SDL2/`, `SDL3/`): a texture per page, uploaded the first time a
  glyph of it is drawn, colour-modded at draw. `SDLFontManager` owns it; closing the screen releases the textures
  before `SDL_Quit`. Both `ext/stbttf.h` copies are gone.
- **The `const char *` path** draws ASCII only, as before - a byte past 127 is a piece of UTF-8 - but each byte takes
  its cell, as the background fill already did.
- **The workspace divide** is `│` again.

**Tests:** `glyphfont` - page and slot, page 0 at open, a page built once, a missing glyph and an empty page not
retried, glyph rectangles inside their bitmap and with ink, every glyph one cell wide, junk bytes refused. Not covered:
the texture upload and the draw - SDL, needs a window (a look at the running app).

## 7. The overview - the whole file at the editor's right edge

Status: **outlined 2026-11-09, planned the same day**, not scheduled. Not built. SDL first, the terminal after.

**Why:** the change marks (§0c) say which lines differ, but only for the lines on screen. In a file of 1500 lines
(`Document.cpp`) the question is "where are the changes in this file?", and the answer means paging through it. VS Code
answers it with the minimap: the whole file drawn small down the right edge, a line one or two pixels high, with a
translucent box over the part the editor shows - the changes, the search hits and where you are, all at a glance.

**Two parts, the second on the first:**

| | What | Where it draws |
|---|---|---|
| a | **The overview ruler**: one column the height of the editor, the whole file mapped onto it. Marks only - the change marks, the search hits, the active line - and the visible part shaded. | Any backend: cells. |
| b | **The minimap**: the file's text drawn small - a pixel per character, coloured as the highlighter coloured it, whitespace left out - under the same marks and the same shaded part. | SDL only: pixels. |

(a) answers the question that started this and works on any backend. (b) adds the shape of the code - where the long
functions are, the comment blocks - and needs a pixel path the cell grid doesn't have.

**Build order** (planned 2026-11-09):

1. **The model, no drawing:** the mapping, the marks per row, the visible box, click and drag -> line. Plain, tested on
   strings (below). **Built 2026-11-09:** `Core/OverviewMap` (`test_overviewmap`). A row shows the heaviest mark:
   active line > search hit > removed > modified > added. A click and a drag are one path - `GrabOffset` (inside the
   box: where it was taken; outside: its middle, so the view centres) then `TopLineFor`. For the minimap the rows are
   pixel rows, but `MouseEvent` carries whole cells - step 2 needs the pointer's position within the cell (SDL has it)
   or a click lands to the nearest ~20 px.
2. **The minimap on SDL3 and SDL2:** `MinimapImage`, `DrawImage`, the texture, the marks strip, the box. Bug #18
   (glyphs drawn at 1x on Retina) is fixed: `SDLFont` rasterises at the screen's density and draws in logical
   units - the minimap's texture wants the same (pixels at density, a destination rect in logical units).
   **Built 2026-11-09** (`OverviewView`, `Core/Minimap`, `PixelImage`, `DrawContext::DrawImage`,
   `ScreenBase::CellSizeInPixels` / `PixelDensity`). Where it went other than planned:
   - **The image is the overview's rows, not the file's lines.** A texture a line a row would be 250k rows for
     `sqlite3.c` (past any texture limit); `Minimap::Render` blends a row's lines itself (a column's colour the
     average of its characters, its strength the share of the row's lines that have one) into an image the size of
     the column in screen pixels - drawn 1:1, nothing scaled.
   - **One image, composed by the view:** the text, the marks strip (left, two logical px) and the box blended in
     (`minimap_viewport`). Three layers, each redone only on a change: the text on a buffer revision
     (`TextBuffer::Revision` - an edit, a parse, a recolour), waiting 4x its last build cost before the next (a
     huge file doesn't rebuild at every key); the frame when the text, a mark or the box moved; the texture on
     the frame's revision. So `DrawImage` is the only pixel primitive - what Gansi will implement too.
   - **The caret rides along.** "The cursor doesn't move" can't hold: `cursor.position.y` is screen-relative and
     the caret is always on screen (the wheel moves it too). `Document::ScrollViewTo` moves the view and the active
     line together - the caret keeps its screen row and its wanted column, as a page jump does.
   - **Sub-cell mouse:** `MouseEvent::subCellX/Y` (SDL fills them). The SDL backends now send every pixel of a
     drag; `RootView` drops a move within the same cell unless the drag view says `IsSubCellDrag()` - the
     splitters see what they saw before.
   - **Placed in the editor's `HStackView`** after the container (as the gutter is, `kFixed`), not inside the
     container; it draws nothing in hex mode, as the gutter. Not added at all without pixels (until the ruler).
   - Config: `minimap.enabled` / `width` (cells, 10) / `max_pixels_per_line` (2, logical). Theme:
     `ui.minimap_viewport`; the importer maps `minimapSlider.background` (VS Code), a faint foreground otherwise.
   - **Marks are 3 logical px wide and at least 2 high** (`OverviewMap::Thicken`, centred, heaviest wins): at one
     screen row a line, a change in `Document.cpp` was a single pixel - too thin to see.
3. **The terminal (Gansi), explored after (c):** the minimap where the terminal can draw pixels, the ruler (a) where it
   can't - decided once, at startup. (a) is small once 1 exists; it is the fallback, not a step of its own.

### The mapping (both)

- **The whole file fits the height** (VS Code's `minimap.size: fit`): a line is `h = viewHeight / numLines` pixels (or
  rows), at most 2 px - a short file doesn't stretch, it stays at the top. 1500 lines in a 900 px editor: 0.6 px a
  line, so several lines share a pixel row; a row shows the heaviest mark of its lines (removed > changed > added), as
  a folder in the tree shows the heaviest change below it. No scrolling of the overview itself - the point is all of it
  at once.
- **The visible part**: the box from the view's top line to its bottom line, in the same mapping - translucent over
  the minimap, a shaded background in the ruler. At least one row high, so a huge file still shows where you are.
- **Plain and tested apart from drawing**: line -> row, row -> line, the box for a view, the marks per row. Soft-wrapped
  lines (`WrapRow`) count as one line - the overview is of the file, not the screen.

### The mouse

- **Click**: the editor scrolls so the line under the pointer is centred. **Drag** the box: the view follows -
  `OnMouseDragStart` / `OnMouseDrag` from §2, so it keeps the pointer outside the column too.
- The cursor doesn't move: it is a look, not a jump. (VS Code moves neither.)

### (a) The ruler, in cells

- **Where:** a column at the editor's right edge, inside the `EditorViewContainer` next to the text (and not drawn over
  the hex view). One column - two with a gap; `overview.width`.
- **Two slots per cell:** `▀` (U+2580, Andale Mono has it) with the upper half's colour as foreground and the lower
  half's as background gives each cell two marks, so 40 rows show 80 slots. The visible part is the background of its
  slots in `overview_viewport`.
- **What is marked**, one colour per slot, heaviest wins: removed / changed / added (§0c's colours), a search hit
  (`search`), the active line (`caret`). Later: diagnostics, the code index's "where is this symbol used".
- **The drawn test** (§5b): a document with marks, the ruler drawn into the headless grid - the slot of each changed line
  carries its colour, the visible part is shaded and moves with the view. The whole of (a) is checkable without a
  window.

### (b) The minimap, in pixels

- **Where:** the same place, wider (`minimap.width`, in cells: 10 cells of 10 px hold 100 characters at a pixel each).
  The ruler's marks become a strip at its left edge, as VS Code puts them.
- **From the lines' colours, not from the text:** a row of the minimap is the line's attribute spans
  (`Line::Attributes()`, what the highlighter left) turned into runs of pixels - no lexing, no copy of the text. Tabs
  expand as the view does (`CharToVisualColumn`). Kept per line, rebuilt for the lines a parse touched: the same
  region the highlighter re-parses, as the structure pass and the change marks do; a full parse or a recolour
  rebuilds them all.
- **The image** is plain data in Core (`MinimapImage`: a width, and per row runs of `(x, length, colour)`), built and
  tested without SDL. The backends draw it: a texture they update for the changed rows only, scaled into the column
  when the file is taller than the minimap (several lines to a pixel row - the scaling blends them, which is what fit
  mode looks like).
- **The drawing primitive:** the cell-based `DrawContext` has nothing for pixels. A new `DrawImage(rect, image)` - the
  rect in cells, the image in its own pixels - that SDL implements and the other backends leave empty; the view checks
  whether the backend draws pixels (a `ScreenBase` capability) and shows the ruler instead when it doesn't. Nothing in
  `Core/Graphics` knows a `Document` (the layering rule in `CLAUDE.md`).
- **The box over it:** a translucent fill (`minimap_viewport`, with alpha), one draw.
- **Checked by:** the image built from lines on strings (a known line's runs, tabs, a recolour), the mapping (a), and a
  look at the running app for the texture - as §6 left it.

### (c) The terminal (Gansi) - after SDL

Planned 2026-11-09, to explore once (b) runs on SDL. In the terminal, pixels are the terminal program's to give, not
the pty's (the pty only carries bytes) - so the editor has to ask it, at startup, and pick the overview from the answer.

- **Today Gansi only reads the environment** (`TERM`, `TERM_PROGRAM`, `COLORTERM` in `gansi/src/Capabilities.cpp`).
  That can't decide this: `TERM_PROGRAM` doesn't cross SSH, and no variable says "draws images".
- **The probe:** one write at startup - kitty's graphics query (`ESC _G i=31,s=1,v=1,a=q,t=d,f=24;AAAA ESC \`), then
  `CSI c` (DA1), which every terminal answers, so there is no timeout to wait for. A `_G i=31;OK` before the DA1 reply:
  kitty graphics. A `4` among DA1's parameters: sixel. `CSI 16 t`: a cell's size in pixels (also in the window size's
  `ws_xpixel`/`ws_ypixel` on many terminals; SSH carries those). Optional: `CSI >0q` (XTVERSION), the terminal's name,
  which does cross SSH. Gansi's input parser has to learn these replies - APC (`ESC _ ... ST`) isn't parsed today, so a
  kitty reply would most likely turn into key presses - and hand them to the probe, not to the key handler.
- **The result** is a Gansi capability (`graphics`: none / kitty / sixel, and the cell's pixel size), read by the
  screen's pixel capability from (b). `overview.kind: minimap` without it falls back to the ruler - the same check SDL
  answers with "yes", so nothing in the views knows about terminals.
- **Kitty graphics** (kitty, Ghostty, WezTerm, Konsole in part) is the one to build: an image lives on its own layer, by
  id, so the cell grid's diff never writes over it. The box is a second, translucent image, moved by placing it again
  (a few bytes, with a pixel offset within the cell). Transmit compressed (`o=z`) - a minimap is mostly background -
  and only on the pause after typing, not per key; a changed band could be sent alone (the protocol's frame edit - to
  check). The protocol lives in Gansi (`Terminal` places, updates and removes an image); `GansiDrawContext::DrawImage`
  hands it the image.
- **Sixel** (foot, WezTerm, xterm, iTerm2, Windows Terminal): pixels in the cells - text drawn there erases them, the
  whole image goes again on any change, no alpha. Only if a terminal in use needs it.
- **Cells as pixels** (`▀`, quadrants, sextants U+1FB00, octants U+1CD00 - two colours a cell): a minimap without a
  graphics protocol, checkable in the headless grid. A middle ground between the ruler and pixels; an option, not
  planned.
- **tmux** answers DA1 itself and blocks kitty graphics unless `allow-passthrough` is on - the probe sees no graphics
  and the ruler is shown, which is right.
- **Headless:** `HLScreen` records the images placed (rect, size) so a test checks where the minimap went, and draws the
  ruler into the grid as the fallback.

### Config and theme

`overview.enabled`, `overview.kind` (`ruler` / `minimap`, `minimap` falls back to the ruler without pixels),
`overview.width`, `minimap.max_pixels_per_line` (2). Theme: `overview_viewport`, `minimap_viewport`; the marks reuse
`gutter_added` / `gutter_modified` / `gutter_deleted`, `search`, `caret`. The importer: VS Code's
`minimapSlider.background`, `editorOverviewRuler.*`, `minimapGutter.*`.

### Open

- Ruler and minimap side by side (VS Code has both), or one or the other - one, by `overview.kind`, to start.
- The terminal view and the tree: a ruler for the terminal's scrollback is the same mapping; not now.
- A hover on the minimap showing the lines there (VS Code doesn't; Sublime doesn't either) - no.

## 8. Select all is missing

Filed 2026-11-11 (found while trying YAML reformat). The editor has no notion of select all: no action in `kAction`,
nothing in `Document`, and `Cmd+A` is not bound in any keymap - on SDL it does nothing. With no way to select the whole
file, `ReformatBlock` (`Cmd+I`) can't be run over a file either, short of selecting it line by line.

**The ANSI backend makes it worse:** a terminal keeps most `Cmd` combinations for itself, so `Cmd+A` selects the
terminal's own text instead of reaching the editor (`Cmd+I` likely never arrives either - Terminal.app opens its
Inspector). Gansi only sees `Cmd` when the terminal reports it as `Super`. A select all for Gansi needs a binding a
terminal passes through. Open bug #20 is the general case (the terminal takes `Cmd` keys first).

Open: the action (`SelectAll` - editor-owned `kAction`, it's functionality), its key per backend, and whether reformat
over a whole file should also become a quick command (§4's `.fm_file`). Related: open bug #27 (selecting line by line
can't take in the last line).

### Plan (2026-11-11) - select all, `.fm_file`, `.fm_block`

**What the JS API has today:** a `Document` (`DocumentAPI`, wrapping a workspace node) can save, reload, report whether it is
modified, and give its name. It has no access to the content (no lines to read or write, no line iterator) and no
selection. `TextBufferAPI` only has a name, a file name and the language.

**Why that doesn't block this:** reformat doesn't happen in JS. It is `Document::ReindentLineRange` (C++, the indent
engine, one undo item), and `OnActionReformatBlock` already knows the selection and the enclosing `{ }` block. The quick
commands only need to *ask* for a reformat, so two thin methods do it. Content access from JS (lines, an iterator,
the selection as data) is a larger API that nothing here needs; it waits for a script that does.

**A. `SelectAll` (native action)**
- `kAction::kActionSelectAll` ("SelectAll" in `KeyMapping.cpp`); `Document::OnActionSelectAll`: selection from `(0,0)`
  to the end of the last line `(len(last), n-1)`, caret at that end. An empty buffer selects nothing.
- Keys: `CopyPasteModifier + 'a'` in both `default_keymap.yml` (`Cmd+A` macOS, `Ctrl+A` Linux - neither is bound now).
  The terminal version on macOS stays behind #20 (`Cmd+A` is the terminal's), and `.fm_file` is the way around it there.
- Check: copy after select all gives the whole buffer, through the external round-trip (E.18); `ReformatBlock` after it
  takes the last line (its `end.x == 0` rule doesn't fire, since the end is past the last character).

**B. `DocumentAPI::Reformat*` (C++ -> JS)**, following the API pattern (`DocumentAPI` -> `DocumentAPIWrapper` ->
`RegisterModule`):
- `ReformatFile()` -> `ReindentLineRange(0, n-1)`.
- `ReformatBlock()` -> the same path as `Cmd+I`: the selection, or else the enclosing block, or else the line. The body
  of `OnActionReformatBlock` moves to a public `Document::ReformatSelectionOrBlock()`, which both the action and the
  API call, so the two can't drift apart.
- Both return `false` when there is nothing to do: no document, no language, or a language with `reindent: no` (YAML).
  That needs an `IsReindentable()` that the scripts can ask through the document, so they can say why.

**C. The scripts** (`jsplugs.yml`, like `reload.js`)
- `.fm_file` / short `.fmf` -> `Editor.GetActiveDocument().ReformatFile()`.
- `.fm_block` / short `.fmb` -> `ReformatBlock()`: the selection when there is one, as `Cmd+I`. (Quick mode doesn't cancel
  the selection. A test confirms it.)
- `.fm_line` is not added. `.fm_block` with no selection and no enclosing block already reformats the line, and
  `Cmd+L` stays.
- When reformat is off (YAML) they print `Reformat is off for 'yaml'` and don't edit anything.

**D. Bug #27, in the same branch** (it's the other half of "select some lines and reformat"): `Shift+Down` on the last
line moves the caret to the end of that line, and `Shift+Up` on the first line moves it to the start. Write the repro
test first.

**Tests** (`document` and `jsengine`):
- `selectall`: copy after it is the whole buffer; it on an empty buffer; it then `ReformatBlock` takes the last line.
- `fmfile` / `fmblock` through the command line: a misindented C++ buffer comes out reindented, and one undo brings it
  back; `.fm_block` with a selection only touches those lines; YAML leaves it byte-identical and prints the note.
- #27: the line-down selection covers the last line.

The mutation checks: a `ReformatFile` that stops at `n-2`, or a select all that ends at column 0 of the last line, fails
these tests.

**Branch:** `feat/select-all-reformat-cmds`, A -> B/C -> D, one commit each. About a day's work at most; no new design
questions left.

**Decided:** both the long and the short names (`.fmf` / `.fmb`); `Ctrl+A` on Linux is select all.

**Built (2026-11-11), branch `feat/select-all-reformat-cmds`:**
- **Select all:** `SelectAll` (`Cmd+A` / `Ctrl+A`); `Document::SelectAll`, handled in `OnAction` before `DispatchAction`
  (which cancels the selection after a handled action).
- **Bug #27:** `Down` on the last line moves the caret to that line's end, with or without `Shift`, mirroring `Up` on the
  first line (which already went to its start).
- **Reformat for scripts:** `Document::ReformatFile` / `ReformatSelectionOrBlock` (`Cmd+I` now calls the latter) /
  `IsReindentable`. `DocumentAPI` exposes them as `ReformatFile`, `ReformatBlock`, `IsReformatEnabled` and
  `GetLanguage`.
- **Commands:** `.fm_file` / `.fmf` (`reformatfile.js`) and `.fm_block` / `.fmb` (`reformatblock.js`).
- **Tests:**
  - `document`: `selectall`, `selectall_empty`, `selectall_reformat`, `selectdown_lastline` (it failed before the fix),
    `selectup_firstline`.
  - `jsengine`: `fmfilecmd`, `fmblockcmd`, `fmyaml`.
- **Mutation check:** a `ReformatFile` that stops one line short fails `fmfilecmd`.
- **Not yet tried in the app:** select all with `Cmd+A` on SDL3. On the terminal version on macOS it stays behind #20, and
  `.fmf` works there.

## 9. The git branch in the status line

Filed 2026-11-11. When the opened folder (`goatedit .`) is in a git repository, the status line
(`HSplitViewStatus::DrawSplitter`) shows the branch that is checked out: `⎇ main`. Just the branch - no ahead/behind
(not now). Outside a repository, or when git isn't installed, nothing is shown.

**What exists:**
- `RepoMonitor` (`src/Core/Git/`) keeps each project root's change set current. It already re-queries when a git
  operation touches `.git/index`, `.git/HEAD` or `.git/logs/HEAD`, so a `git switch` in the terminal already reaches
  it - no new watcher is needed.
- `GitStatus::Snapshot` knows `gitDir`, which is worktree-aware.

**Where the branch comes from:** add `--branch` to the status query that already runs
(`git status --porcelain=v1 -z --branch`). The first record is then `## main...origin/main`, `## HEAD (no branch)`
when detached, or `## No commits yet on main` in a new repository. `GitStatus::Parse` takes the `##` record off before
the file changes and keeps only the name.
- **Why not read `<gitDir>/HEAD`:** it would be cheaper, but git interprets it better than we would. With the newer
  reftable ref storage, `HEAD` holds a placeholder (`ref: refs/heads/.invalid`) and the real branch is elsewhere.
  `--branch` costs no extra process.

**Model:**
- `Snapshot` gets a `branch`: the name, or the short SHA when detached.
- `ChangeSet` carries it, so the status line reads one place.
- **A catch:** `RepoMonitor` only posts a new change set when `IsSameAs` says it differs. A switch between two branches
  with the same working-tree state changes nothing there, so `IsSameAs` must compare the branch too. A test pins this.

**Whose branch:** the opened folder's - the one the editor was started on (`goatedit .`). That is the repository being
worked in, and the branch doesn't change as documents are switched.
- A folder inside a repository (`goatedit src/`) shows the enclosing repository's branch: git finds the top level.
- Loose files (the default root, opened without a folder) don't matter: with no opened folder, nothing is shown.
- **Not handled, on purpose:**
  - more than one opened folder (each would have its own `RepoMonitor` and branch) - the first one wins;
  - repositories nested inside the folder (a submodule shows the outer branch).

**How it shows:**
- In the status line's right part, before the view's own info: `⎇ main`.
- When detached: `⎇ (a1b2c3d)`.
- Very long names are cut in the middle (`feature/very-…-thing`).
- `⎇` (U+2387) is in few monospace fonts; the system font fallback finds it on SDL. Gansi depends on the terminal's
  font.

**Tests:**
- `gitstatus`:
  - the `##` record parsed into the branch, including detached, no commits yet, and a branch with no upstream;
  - the file changes after the `##` record are unchanged;
  - `IsSameAs` is false for two change sets that differ only in branch.
- `workspace`: the opened folder's root gives the branch; no opened folder, no branch.

## 10. Double-click on a folder in the workspace tree expands or collapses it

Filed 2026-11-11. A double-click on a file opens it. A double-click on a folder does nothing today; it should toggle
the folder: expand it when it's collapsed, collapse it when it's expanded.

**Today:**
- `WorkspaceView::OnMousePressedEvent` selects the row on every press, and on `clicks >= 2` calls `OpenSelectedItem`.
- `OpenSelectedItem` is the one open path for both Enter and double-click. It returns early on a folder ("You
  selected a directory!").
- Expanding and collapsing exist only as the arrow keys (`TreeView::OnAction`: Left -> `Collapse`, Right -> `Expand`).
  `Expand` also fires the lazy fetch of a folder's children (FS-6), so a double-click takes the same path and needs
  nothing new for unscanned folders.

**Change:**
- `TreeView::ToggleExpanded()` (expanded -> `Collapse`, else `Expand`), next to the two.
- In `OpenSelectedItem`, a folder toggles instead of returning. Since Enter shares that path, **Enter on a folder
  toggles too** - most file trees do that, and it keeps "one open path, two triggers".
- **Triple-click:** SDL counts on (`clicks` 3, 4, ...), and each press arrives as its own event. With `>= 2`, a
  triple-click toggles twice and ends where it started. Open/toggle on `clicks == 2` only.
- The expand/collapse state is already kept in the session (`BuildExpandCollapseCache`), so a toggled folder stays as
  it was left.

**Tests (`treeview` / `session`):**
- a double-click on a collapsed folder expands it, and a second double-click collapses it;
- on an unscanned folder, the double-click fetches the children (the lazy path);
- Enter does the same;
- a press with `clicks == 3` doesn't toggle again;
- a double-click on a file still opens it.

**Built (2026-11-12):**
- `TreeView::ToggleExpanded` (through `Expand`, so an unfetched folder fetches its children as the arrow key does).
- `WorkspaceView::OpenSelectedItem` toggles a folder instead of returning - Enter on a folder toggles too.
- `OnMousePressedEvent` opens/toggles on `clicks == 2` only.
- **Tests:**
  - `session`: `workspaceview_dblclick_folder`, `workspaceview_dblclick_unscanned` (a git-ignored folder),
    `workspaceview_enter_folder`, `workspaceview_tripleclick` - all four failed before the change -, and
    `workspaceview_dblclick_file` (passed before and after).
  - `treeview`: `toggleexpanded`.
- **Mutation check:** `clicks >= 2` fails `workspaceview_tripleclick`.
- **Not yet tried in the app** (SDL2 / SDL3).

## 11. Revisit the key bindings - modifiers by purpose

Filed 2026-11-11. The named modifiers are meant to group bindings by purpose, so a whole group can be rebound in one
place. `UINavigationModifier` does that for the views and panels. But `CopyPasteModifier` has grown into "anything on
`Cmd`":
- editing: copy, paste, cut, undo, comment, reformat, select all;
- finding and opening: find, quick open, open files;
- code navigation: go to symbol, the go-to family, back and forward.

Code navigation is also split between `Cmd+<key>` (`Cmd+B`, `Cmd+T`, `Cmd+[`) and `Cmd+Shift+<key>` (implementation,
base, type, relationships). It should be one group under one modifier, as UI navigation is.

**How modifiers work today** (`KeyMapping.cpp`):
- **The set of names is fixed:** `strToModifierMap` has `Selection`, `CopyPaste` and `UINavigation` (each also with
  the `...Modifier` suffix). `ValidateModifiers` rejects any other name in a keymap's `modifiers:` section.
- **Each name stands for one key:** the value must pass `Keyboard::IsNameModifierMask` - `KeyCode_Cmd`, not
  `KeyCode_Cmd + KeyCode_Shift`. **So a modifier can't be a combination today.**
- **A named modifier also tags the binding** with a `kActionModifier`, and only `Selection` means anything at run time
  (`Document::OnAction` starts a selection on it). `CopyPaste` and `UINavigation` are tags only.
- **Only an optional modifier tags a binding, and only on a special key** (corrected 2026-11-12; this said "the last
  named modifier wins"). In `ParseKeyPressCombinationString` the tag comes from an `@` modifier present in that
  permutation, and the ASCII branch never sets it. So `CopyPasteModifier + SelectionModifier + 'b'` (`GotoImplementation`)
  is **not** tagged and starts no selection; nor do `GotoBase`, `GotoType`, `ShowRelationships`, `PrevSearchResult`.
  `Shift+Down` (`@SelectionModifier`) is. **Checked** (hitlist #3): `keymapping` `shiftedletter_untagged` and
  `document` `gotoimpl_noselection` resolve the real keys through the platform keymap; tagging the way this note
  assumed (required modifiers too, letters too) makes both fail - the document then starts a selection. Keep that
  rule when modifiers become combinations below.

**Proposal:**
- **Modifiers can be combinations:** a value like `KeyCode_Cmd + KeyCode_Shift` parses into one mask
  (`ModifierMaskFromString` over the `+` parts). The `@` optional form keeps working: it permutes the whole mask.
- **The groups:**

| Modifier | Purpose | macOS | Linux |
|---|---|---|---|
| `SelectionModifier` | extends a selection | `Shift` | `Shift` |
| `FunctionModifier` | editing and the editor's functions: copy/paste/cut, undo, find, select all, comment, reformat, quick open, open files | `Cmd` | `Ctrl` |
| `CodeNavigationModifier` | code navigation: go to symbol, go to (toggle, implementation, base, type), relationships, back/forward | to decide (below) | to decide |
| `UINavigationModifier` | views, panels, sizes, view modes, buffers | `Option` | `Alt` |

- **`FunctionModifier` replaces `CopyPasteModifier`.** The old name stays as an alias, so a user's own keymap keeps
  working (and logs that it is deprecated).
- **CLAUDE.md's rule** ("`Cmd`/`Ctrl` is functionality, `Option`/`Alt` is UI control") gets the third group.

**`CodeNavigationModifier` - the choice:**
- **`Cmd+Shift` / `Ctrl+Shift`:** every go-to moves to `Shift`, and the letters stay readable (`B` toggle, `I`
  implementation, `U` base, `Y` type, `T` symbol, `R` relationships, `[` / `]` back and forward). The cost: `Cmd+B`,
  which is in the fingers (and is CLion's), becomes `Cmd+Shift+B`.
- **`Ctrl+Option` on macOS / `Ctrl+Alt` on Linux:** free of the OS's own `Cmd` combinations. But `Ctrl+Alt+<letter>` is
  taken by some Linux desktops, and on Linux it's two of the same keys the other groups use.
- **Keep `Cmd` (= `FunctionModifier`) for the go-to family and only regroup the names:** the least churn, but no
  separate rebinding.
- Before deciding: list what macOS, GNOME/KDE and Ghostty take in each candidate combination (open bug #20 has
  Ghostty's).

**Also in this pass:**
- **An inventory** - one table of every binding: action, group, key per OS. It lives in `docs/keybindings.md` and is
  the input to `.h` / help. Today the only list is the keymap files.
- **The terminal version** (bug #20) chooses its own section; the groups make that a matter of four modifier values,
  not every binding.
- **A test (`keymapping`):** a combined modifier value parses into one mask; an unknown modifier name is still
  rejected; the `CopyPasteModifier` alias resolves; a binding keeps the `Selection` tag only through an `@SelectionModifier`
  (as today - `shiftedletter_untagged` guards it).

## 12. More languages - CMake, Python, and languages as data files

Filed 2026-11-11. A design of its own: [`support-languages.md`](support-languages.md). In short:
- a generic engine (`DataLanguage`) builds the existing tokenizer from a YAML file per language - its name,
  extensions, author, date, then states, tokens, comments, strings and numbers;
- languages can be added without recompiling (system, user and project directories, plus `.lang_reload`);
- CMake and Python are the first data languages, and JSON is ported to data to prove the engine.

## 13. Mouse bindings in the keymaps

Filed 2026-11-12 (while building `Cmd+click`, code-index §5.6). What a click does is nowhere in a file today: the tree's
double-click (§10), the go-to click and the terminal view's modifiers are written into the views. Only the go-to click's
modifier comes from the keymap (`KeyMapping::ModifierMask`), and the click -> `GotoToggle` part is still code.

**Change:** a `mouse:` section in the keymap files, next to `actions:` - not a separate `mousebindings.yml`:

```yaml
# macOS/default_keymap.yml (the Linux file is the same - CopyPasteModifier is Ctrl there)
mouse:
  GotoToggle: CopyPasteModifier + Click
# workspace_keymap.yml
mouse:
  CommitLine: DoubleClick        # open a file, toggle a folder
```

**Why in the keymaps:**
- the named modifiers (`CopyPasteModifier`) already resolve per OS - no second table to keep in step;
- the `Linux/` / `macOS/` split, `inherit:` and the per-view keymaps (`KeyMappingCache`) come for free: the tree's
  double-click goes in `workspace_keymap.yml` as its keys do;
- a binding turns into the same `EditorAction` a key does, so a jump lands on the navigation stack as `Cmd+B`'s does.

**Scope:** only clicks that stand for an action - a modifier plus `Click`, `DoubleClick`. Selecting, dragging, the wheel
and the terminal's mouse pass-through stay in code: they are gestures, not actions.

**Then:** `EditorView::IsGoToClick` and the `clicks == 2` in `WorkspaceView::OnMousePressedEvent` become lookups
(`KeyMapping::ActionFromMouse`), and their tests (`document` `gotoclick*`, `session` `workspaceview_*click*`) stay as
they are - they check behaviour, not where it's bound.

Open: the names (`Click`, `DoubleClick`, `MiddleClick`?), and whether a view without a `mouse:` binding falls back to
today's behaviour or does nothing.

