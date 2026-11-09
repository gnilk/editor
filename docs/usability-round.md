# Usability round - open files, splitters, workspace search

Status: **Planned 2026-11-08.** The code index is paused before its step 4 (`code-index.md`); this round comes first.
Order: the change set in the tree (§0), reloading files changed on disk (§0b), changed lines in the gutter (§0c), open
files list (§1), dragging splitters (§2), workspace search (§3), quick commands: close all and `!` (§5), headless drawing that can be checked (§5b), glyphs beyond
code point 255 on SDL (§6). The note on key bindings for script commands (§4) is recorded, not scheduled.
**Next:** §0c, changed lines in the gutter - §6 (glyphs past 255) is built, so its bar can be `│`.

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

**Tests:** the diff on strings (only added, only removed, changed, moved blocks, an empty base, an empty buffer, a large
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
- **Feedback:** none needed at first; the views follow the pointer. A cursor shape change is a later nicety.

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

**Tests:** the executor on `.cb!` / `.cb !` / `.cb` (the arguments the script gets), a name that is only `!`, and `.ca`
over a workspace with a modified and an unmodified document.

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
allowed it ("too wide alone"); drawn, it reads as a bug. Open: drop the `...` before it in that case, or cut the name
with `...` itself.

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
