# Work log

Short, reverse-chronological index of completed feature efforts. Each entry summarises *what shipped*
and *the load-bearing decisions*; the linked detail doc carries the full cold-start history. What's
planned is in [`hitlist.md`](hitlist.md). For known-wrong code deliberately left unfixed, see
[`open-bugs.md`](open-bugs.md) (the active tracker, not logged here). For work explicitly deferred /
left out of scope from shipped features, see [`deferred.md`](deferred.md).

---

## Messages tab (2026-11-20) ✅

**Hitlist #53 step 5 / #52.** `view: messages` in `panel.tabs`: the last build's errors and warnings kept open in the
lower panel - the second kind of tab. **Load-bearing decisions:** the view reads `BuildMessages` when it draws (a
terminal's output already asks for frames), so it needs no feed of its own; its selection follows F8 through
`BuildMessages::Current`, a new run through `RunNumber`. `Cmd/Ctrl+F8` shows the tab when there is one - the modal
stays for a panel without it. The tab has its own keymap so the panel's close key never reaches the editor's
close-file. A panel always keeps a terminal. Detail: `panel-tabs.md` §5 step 5.

---

## Configured tabs - an agent's terminal (2026-11-20) ✅

**Hitlist #53 step 4.** The lower panel's tabs come from `panel.tabs` (name, view, config section); an agent's tab is
a terminal handed its own section (`ai_agent: { inherit: terminal, bootstrap: ["claude"], promote_target: yes }`).
**Load-bearing decisions:** one terminal view for any long-lived console app - configuration differs, not classes.
`inherit:` is generic (`ConfigNode::ResolveSection`), and a view is handed its section resolved instead of looking
up a global one by name (no `Config::Instance()["terminal"]` left). An agent is a shell with a bootstrap, not the
tab's process. A project config never sets a command in any section. `TerminalPanelView` became `PanelView`; the
generic `TabbedView` owns the tab views. Detail: `panel-tabs.md` §5 step 4, §7.

---

## Font styles in themes (2026-11-19) ✅

**Hitlist #55.** VS Code themes set bold / italic per scope; the importer dropped them. **Load-bearing decisions:**
`font_styles` is its own table in `colors.json`, keyed by content key - not a colour value - so the colour script and
the sections stay as they were and a theme without it is unchanged. The style rides with the colour into
`LineAttrib::textAttributes` (`Editor::FontStyleFromLanguageToken`, set at the four places that colour a token), so
the existing draw path draws it. The importer resolves the style as a property of its own (VS Code's model), but never
from a context rule - Dark 2026's `markup.heading entity.name` made every function bold on the first try. SDL italic is a
synthetic slant (`SDL_RenderGeometry`), bold the existing 1px double draw. Detail: `theme-collection.md` §6.

---

## `.theme use` at a level (2026-11-19) ✅

**Hitlist #59.** A project's `.goatedit/config.yml` with its own `main.theme` won over `.theme use` after a restart,
with no word why. **Load-bearing decisions:** the live theme follows the config that wins - `.theme use` (user, the
default) in a project that sets another theme doesn't switch unless the project's value goes; the script asks
(`Editor.Confirm`: Remove it from the project / Keep the project's / Cancel), the C++ side only answers questions
(`Themes.Find`, `Themes.ProjectTheme`) and writes. `ConfigFilePatcher::RemoveScalar` takes one value's line out and a
section it leaves empty (`main:` alone would read as null), verified like `SetScalar` (re-parsed, must equal the
original minus the key) - shared checks `CheckKeyPath` / `AnalyzeLines`; `ConfigAPI::Unset` (+ `.cfg unset`) and
`ApplyEffective` (the tail `Set` had). `.theme list` ends with where `main.theme` comes from. Check `theme_project`.

---

## Dot-commands at the terminal prompt; the bash 5 quit hang (2026-11-19) ✅

**Hitlist #57.** Since #51 every line the editor prints goes above the shell's prompt - right for output nobody typed
a command for (the banner, plugins), wrong for a dot-command typed at that prompt: its output went above it and the
command itself was never on screen. **Load-bearing decisions:** `TerminalController::RunDotCommand` writes the command
onto the prompt line and moves to the row below, so `WriteLine` (still `WriteAboveCursorLine`) prints there; after
the command the cursor goes up onto the last row written, and the empty line the shell is sent moves it down and
draws the next prompt right below - no blank row, and the shell's relative cursor bookkeeping stays right (it is idle
at its prompt). A dot-command typed after Tab (readline owns the line) is unchanged. `TerminalScreen::MarkOutputEnd`:
the first dot-command after a shell command ends that command's output at its prompt row, so `.co` still copies it.
Found with it: quitting with Homebrew bash 5 hung - bash writes on SIGHUP and exits, `ConsumePty` has stopped reading
and waits in `ReapChild`, macOS keeps the exiting process (state `E`) until its tty output drains; `SIGKILL` can't end
that. `ReapChild` closes the master before waiting (`test_process_shellclose_output_on_hup` reproduces it with
`/bin/sh` and a HUP trap). Check `dot_at_prompt` (bash and zsh).

---

## Dark 2026 (from VS Code) shipped, generated from its source (2026-11-18) ✅

**Hitlist #20** (`theme-collection.md` §4.1). **Load-bearing decisions:**
- **The source is in git, untouched:** `Assets/ThemeSources/vscode/` keeps VS Code's upstream layout (`package.json`,
  `LICENSE.txt`, `themes/2026-dark.json` and its include chain), pinned to a commit in `SOURCE.md` - so the importer's
  provenance lookup fills `author` / `license` / `copyright` / `source` by itself, and an improved importer can
  convert them again.
- **Generated, then committed:** `scripts/gen-themes.py --exe <goatedit>` runs the editor's own `--import-theme` in a
  sandbox HOME and applies `Assets/ThemeSources/overlays/<file>.json` (our name, extra variables/colours - here VS
  Code's default dark ANSI palette, which the themes don't set). Not at build or install: the asset mirror drops what
  isn't in the source tree, and the dev build would have no themes. `--check` is the `themes_gen` check, so the
  committed theme can't fall behind the importer or the overlay.
- Named "Dark 2026 (from VS Code)", file `vscode-dark-2026`; `Themes/LICENSES.md` has the MIT text, the copyright line
  and the commit.

---

## Switch themes: `.theme list` / `.theme use` (2026-11-18) ✅

**Hitlist #19** (`theme-collection.md` §5 step 3). `.theme list` shows the themes `use` can switch to (the user's
`themes/`, the user dir, the shipped `Themes/` - a file name once, as `Theme::Locate` finds it), with author, licence
and the one in use marked; `.theme use <name>` takes the file name, it without `.theme.yml`, or the theme's own name.
**Load-bearing decisions:** live, not "restart to apply" - `Theme::Switch` loads into a fresh theme and swaps it in
(as `Reload` did), so every holder of the `Theme::Ref` sees the new colours and a broken file keeps the old ones;
`Editor::ApplyTheme` then pushes the UI and syntax colours, **highlights the open documents again** (the highlighter
writes colours into each line's attributes - only the terminal and the UI read the theme while drawing), refreshes
the terminal's palette and redraws. The choice is saved through `ConfigAPI::Set` (comments kept). The current one is
the theme in use, not `main.theme` (a start without the user config doesn't follow that). `.theme_reload` now goes
through `ApplyTheme` too - it only re-pushed the UI colours. Tests: `test_jsengine_themecmd` (list, use, colours,
saved config), check `theme_use` (background on screen, editor and terminal, after a restart).

---

## Themes in a folder of their own (2026-11-18) ✅

**Hitlist #18** (`theme-collection.md` §2). Shipped themes live in `Assets/Resources/Themes/` (the default moved there,
its colour file renamed `default.colors.json`), the user's in `themes/` next to the user config (the importer writes
there). **Load-bearing decisions:** one lookup, `Theme::Locate`, for the theme file and its `colorfile:` - an absolute
path as is; else next to the theme first (a colour file), user `themes/`, the user dir itself (themes imported before
still load), system `Themes/`, the system root (the old place) - so no config needs changing. No install rule: the
asset folder is mirrored and installed whole, and #24's stale-asset cleanup takes the old root files out of an install.
Tests: `test_theme_locate`; the import test checks the new folder and loads the theme by name.

---

## Usability gaps: word keys on macOS, drag to select (2026-11-18) ✅

**Hitlist #41.** Word left/right on macOS are `Opt+Left`/`Right` (CLion's default), `Shift` extends on both platforms;
drag to select in the editor and `Shift+click`; `Tab` leaves the project tree on Linux too (it was macOS-only);
`.openfolder` unregistered until #36. **Load-bearing decisions:**
- **Drag after a click, not instead of it.** `OnMouseDragStart` takes the whole gesture before focus and the view
  (splitters, minimap) - wrong for text, where the press must stay a click (focus, caret, `Cmd+click`). New toolkit
  hook `ViewBase::KeepsDragAfterPress`: the view that handled the press keeps the moves and the release through the
  same `OnMouseDrag` capture. `EditorView` follows a plain or Shift left click; each move is `Document::SelectTo`
  (past the edge: the line beyond, so `SetCursorPosition` scrolls). A click now drops the selection (it didn't).
- **Alt must match on special keys.** `ActionItem::MatchKeyPress` never checked Alt: a binding without it took
  `Alt+<key>` when it came first (`Opt+Left` -> `NavigateLeft`). Strict for arrows/Home/End/... only - AltGr arrives as
  Alt on Linux and types the characters quick mode's plain bindings use.
- Word left/right needed open-bugs #1 fixed first (`fix/attribute-at`, its own entry in `fixed-bugs.md`).

Tests: `test_document_dragselect`, `test_layout_drag_after_press`, `test_keymapping_usability_gaps`; check `drag_select`.

---

## Key reference generated from the keymaps (2026-11-18) ✅

**Hitlist #40.** `docs/keys.md` - every binding per keymap (editor, project tree, terminal, quick mode, dialogs, hex
view), macOS and Linux side by side, the `mouse:` bindings and every command (`src/Plugins/jsplugs.yml`, a missing
script flagged). **Load-bearing decisions:** generated, not written - `scripts/gen-keys.py` reads the files the editor
reads (the keymaps named in `config.yml`'s `macos:` / `linux:` sections), so the reference can't drift; `--check` is the
`keys_doc` check. No PyYAML: the keymaps' flow-style subset is parsed by the script, and anything outside it is an error,
not a guess. The Linux column is the terminal version on both platforms (#48). `usage.md` stays the hand-written short
guide. Found by it: `Tab` out of the project tree is macOS-only (usage.md said both - now #41), and `hexview_keymap` was
not in `config.yml` - the hex view got the default keymap through the not-found fallback.

---

## Terminal version on macOS takes the Linux keymaps (2026-11-18) ✅

**Hitlist #48 (and #9), open-bugs #20's keymap half.** The ANSI backend on macOS ran the `macOS/*_keymap.yml` files,
`Cmd` for every function - a terminal never sends `Cmd` (or keeps it, Ghostty's tabs). **Load-bearing decisions:** the
section that names the keymap files is plain data on `KeyMapping` (`SetConfigSection` / `ConfigSection`, the OS's by
default); `Editor` sets `linux` when it starts the ANSI backend on macOS and clears the `KeyMappingCache` - `KeyMapping`
knows no backend (the layering rule). All keymaps, no config override yet. Found with it, on the first all-green Mac
run of `checks.py`: several lines promoted to readline were always sent bracketed - macOS' bash 3.2 never asks for
bracketed paste and showed `00~` / `01~`; now `EncodePaste` with the modes the shell asked for. Tests:
`test_keymapping_section`, `test_terminalcontroller_paste` / `_promote` (a shell without ?2004).

---

## Start-up notices in the terminal panel (2026-11-18) ✅

**Hitlist #50.** What the user should know about a start - project settings not used (not a single-folder launch), a
theme or language file that can't load, a file argument that can't be opened, the legacy user dir still read, a cwd
outside home - went to stderr (and the log) only: nobody sees it when the app is started through `goat` (`open`) or a
launcher. **Load-bearing decisions:** one path, `Editor::Notice(text)` -> `StartupNotices` (`src/Core/StartupNotices.h`,
no Editor dependency): stderr at once (`goatedit: ...`, as before), held until `RunPostInitalizationScript` has run the
banner, then written to the terminal panel through `IOutputConsole::WriteLine` - so above the prompt (#51); later
notices go straight there. Fatal start-up errors (no theme at all, no config) stay stderr-only - there is no window.
Tests: `test_launch_notices`; `single_folder` / `missing_theme` check the panel.

---

## Console output above the shell's prompt (2026-11-18) ✅

**Hitlist #51.** On the Mac the terminal panel showed no shell prompt until the first key: bash 3.2 is quick, its
prompt arrived before the start-up banner (`hello.js` via `Console.WriteLine`), and `WriteLine` wrote the banner's ~30
lines below it - the prompt scrolled off the panel and nothing redrew it (zsh redraws on the resize, Linux bash is
slower than the banner). Not a shell bug: bash wrote the prompt into the pty every time (raw bytes checked).
**Load-bearing decisions:** the editor's output goes ABOVE the shell's line, as a shell prints async output -
`TerminalScreen::WriteAboveCursorLine` encodes the cursor's line (and the rows it wrapped from), erases it, writes the
text there (into scrollback as any output) and feeds the line back below with the cursor, pen and pending wrap as they
were; the shell is never told. The re-encoding is gvt's (`Terminal::EncodeRow` / `EncodePen`, gvttests
`encoderow`), not the editor's. On the alternate screen the text goes at the cursor, as before. The controller's
prompt anchor follows on its own (`IsPromptRow` re-anchors to a redrawn prompt). Tests: `test_terminalscreen_write_above_prompt`
(failed on the old behaviour), `missing_theme` passes on the Mac.

---

## Scripted app checks (2026-11-17) ✅

**`scripts/handtests/`** - the hitlist's 'To try in the app' items as scripts (`checks.py`, 16 checks; `--list`).
**Load-bearing decisions:** the editor runs on its ANSI backend in a pty and `goatpty.py` keeps a small VT screen
(chars + fg/bg per cell), so a check reads text, the status line (`| EDT | * cp1252 a.txt |`, `l: 42`), colours
(CMake token classes) and selection (bg) instead of looking at screenshots. Each check gets a sandbox HOME + XDG dirs
under `<tmp>/goatedit-handtests/` with its work folder *inside* that HOME (a project root must be below `$HOME`; a cwd
outside it is moved to `$HOME`) - never the user's config, session or shell history. Keys are kitty CSI u, which gansi
asks for. What it does not cover: the SDL backend's own keys and drawing. Found on the first run: Ctrl+/ uncommented
by removing 2 chars whatever the prefix (`#set` -> `et`; fixed, `test_document_comment_toggle`), and #49's stack
(SDL picks KMSDRM with no display; `SDL_QUIT` -> `exit(0)` -> `~SDLFontManager` after `SDL_Quit` segfaults).

---

## CMake highlighting (2026-11-17) ✅

**`cmake.yml`** (hitlist #11, step 1 of languages phase 2; detail in `support-languages.md` §7). **Load-bearing
decisions:** case folding is per language with a per-entry override - CMake's commands ignore case, its argument
keywords don't (`PUBLIC` vs a variable `public`); folding compares in place, no copy per token. Numbers are a generic
matcher a state asks for (`numbers: generic`); a number glued to a word isn't one. `CMakeLists.txt` comes in by a
language file's `filenames:` (`Editor::GetLanguageForFile`, name before extension). Commands are `function` - a
semantic class, which the code index's names pass reset to `regular` (no colour in the app, though every tokenizer
test passed): a language now records the classes its own syntax gives and the pass keeps them
(`test_codeindex_syntax_semanticclass` fails without it). Python is step 2.

---

## Promote to terminal (2026-11-17) ✅

**The selection to the terminal** (hitlist #47): for prompts written in a scratch document and sent to a chat
(`claude`) running in the terminal. **Load-bearing decisions:** `Cmd/Ctrl+Enter` (CopyPasteModifier - functionality;
"send" in chat apps; Cmd+X is cut). No selection sends the active line; the text never touches the clipboard
(`ClipBoardItem::AsText` on a throwaway item) and loses a trailing line break. `Editor::PromoteToTerminal` reads
`terminal.promote_sends_enter` (default yes) and calls `TerminalController::Promote`, which rides `Paste` and then
presses Enter where the text landed: `\r` to an app, `CommitLine` at the local prompt (history, output marking, a
dot-command runs as typed), the readline commit (extracted as `CommitShellOwnedLine`) for several lines. Focus and the
selection stay, so a prompt can be changed and sent again. In the terminal itself the key reaches an app as before.

---

## ANSI backend without a display; xterm Ctrl chords (2026-11-17) ✅

**No display, no window** (hitlist #46): the ANSI backend was the default only for `XDG_SESSION_TYPE=tty`, so a
container or an ssh login without pam_systemd tried SDL and failed, and macOS over ssh always did. **Load-bearing
decisions:** `XDGEnvironment::IsTerminalSession` also says yes on Linux when neither `DISPLAY` nor `WAYLAND_DISPLAY`
is set, and on macOS when `SSH_CONNECTION` / `SSH_TTY` is (the window server ignores `DISPLAY`; over ssh it isn't the
user's screen). ssh with X forwarding keeps the ANSI backend, as before (`tty` wins). The decision is a static taking
the environment, tested in `test_launch_no_display`; `ResolveBackend`'s order is unchanged (`--backend`, a desktop
launch, a terminal session, the config).

**xterm Ctrl chords** (hitlist #44, the uxterm tester): the legacy encoding has no `Ctrl+/`, folds `Ctrl+I` into Tab
and drops Shift from Ctrl+letters. **Load-bearing decisions:** gansi asks for xterm's modifyOtherKeys level 2 next to
the Kitty push, not instead of it - a terminal that speaks Kitty lets Kitty win, xterm uses this - and gates it off
only for Terminal.app and a dumb TERM (`CSI > 4;2 m` could read as SGR underline+dim there). Close sends `CSI > 4 m`,
the terminal's own setting, not `;0`. The parser shares the codepoint handling with Kitty's `CSI u`; for
`CSI 27;mod;cp ~` a Shift-only printable is plain text (xterm sends the shifted character) and Ctrl/Alt+Shift+`A`
becomes `a` + Shift, as Kitty and SDL give it. `Ctrl+[` now arrives as itself (it was Escape), as on SDL.

---

## Processes are stopped at exit; terminal Ctrl+C and xterm Alt (2026-11-17) ✅

**Exit use-after-free** (open-bugs #32, hitlist #38): a `Process` thread still running at `exit()` - the gutter's git
query on quit, or `test_workspace_reopen` at trun's teardown - ran its exit callback after the statics went and posted
into Runloop's freed queue. **Load-bearing decisions:** `Process` keeps a registry of the running ones (raw pointer +
`weak_ptr`, never destroyed); `Process::StopAll` kills them and waits for their last callback (2 s in all, skipping a
callback that is itself exiting). It is registered with `std::atexit` on the first `Start()`, so it runs before every
static built earlier - Runloop's queues, the Editor - in the app and at trun's exit alike, without Editor wiring. While
it runs (for good once the exit's has begun) `Start()` refuses, and it rounds until none is left: the first version
missed a chained process - killing `git show` made GitBase's callback start `git status`, which nothing waited for
(Linux ASan caught it, 4/4). Tests: `process_stopall_atexit` / `_chained` run `process-exit-helper` (a program of its
own - a forked trun runs trun's exit, whose logger joins threads the child doesn't have); each fails without its part.

**Terminal, for a tester in uxterm over ssh** (hitlist #45): `Ctrl+C` at the local prompt never reached the shell
(Linux binds it to Copy, the ANSI backend typed `c`). Now the shell gets `^C` and prints a fresh prompt; the typed
text is echoed into the grid by us, as the tty's SIGINT flushes it unread. gansi asks xterm for `metaSendsEscape`
(XTSAVE / `?1036h`, XTRESTORE on close): xterm sets the 8th bit for Alt by default, which uxterm sent as `ô`. Left:
the Ctrl chords legacy xterm can't carry (#44).

---

## Go to line, and line numbers that count from 1 (2026-11-16) ✅

For a new tester coming from vim. `.goto <line>` / `.g <line>` (`ActionHelper::GotoLine`): counts from 1, past the end
is the last line, the caret on the first non-blank (vim's `:N`); a file on disk goes through `JumpTo`, so `Cmd+[` comes
back. Quick mode's `g` was bound but never handled - it now types `.g ` (one path: the command). A `Line` drops
trailing whitespace on create, not on append, so the space is appended. The gutter and the status line printed the
0-based index while Find, Go to symbol and now `.goto` count from 1 - they count from 1 too. Checked by driving the
app in a pty (`--backend ansi`, pyte for the screen). Test: `reload_gotoline`. Also written: `docs/usage.md`, the short
user guide (config layers, keys per platform, mouse, quick mode, commands, terminal).

---

## Text encodings: a Latin-1 file is no longer cut on open and save (2026-11-16) ✅

Data loss (open-bugs #15): the load decoded each line as strict UTF-8, stopped at the first other byte, and nobody
checked - `/* Kolla om oj\xe4mn bredd */` loaded as `/* Kolla om oj`, and the save wrote it back that way.
**Load-bearing decisions:** `TextEncoding` (`src/Core/TextEncoding.{h,cpp}`) detects - a BOM is exact (UTF-8,
UTF-16 LE/BE), valid UTF-8 is UTF-8, anything else is the single-byte `main.encoding_fallback` (default `cp1252`;
single-byte encodings can't be told apart, so it's a setting, not a guess). cp1252's five undefined bytes keep their C1
code point, so every byte decodes and encodes back to itself. The buffer (UTF-32) remembers the encoding; the status
line shows it when it isn't plain UTF-8. **Save policy `main.convert_encoding: keep`** (the user's call, not the
`prompt` planned in October: the autosave fires a second after typing, so a prompt would pop up mid-edit) - a save
writes the file's own encoding, byte for byte; `auto` always writes UTF-8. The save encodes everything before it
opens the file, so a character the encoding can't hold (an emoji in a cp1252 file) stops the save with a message and
leaves the file as it was. `.encoding [name]` shows it / converts on purpose (a change to save, also for the
undo-clean check). Git's version (`SplitLines`, the gutter) and a reload decode the same way; the code index already
reads leniently (`ReplaceInvalidUtf8`). Not changed: CRLF still becomes LF. Tests: the `encoding` module (12 cases;
the round trip of the pcx.c line failed before).

---

## Undo clears the `*`: a buffer edited back to what was loaded/saved is clean again (2026-11-16) ✅

A mistyped `Cmd+<key>` and an undo left the file marked modified. **Load-bearing decisions:** no undo-position
bookkeeping (it misses typing a character and deleting it) and no disk read - the parse thread's existing hash sweep
after a pause in typing (the gutter's change marks; a line's hash is kept until it changes) is compared with a
baseline taken at load, save and reload (`TextBuffer::TakeCleanBaseline`). The sweep runs without a gutter handler
too, and skips hashing when it can't be clean (no baseline, another line count). The buffer state is the UI thread's:
a match posts "clean as of edit N" to it, and it is taken only if no edit came since the sweep started
(`editCounter`: a line's text, lines added/removed); the pending autosave is stopped and the UI redrawn - every `*`
(status line, header tabs) is read at draw time. Measured (sqlite3.c, release): baseline 3.9 ms at load/save (UI
thread), the check after a pause 1.4 ms (parse thread), a keystroke unchanged. Tests: `reload_undoclean`,
`reload_retypeclean` (both failed before), `reload_savedbaseline`. Hand-tested by the user (undo, retype, undo past a
save).

---

## Parse thread vs. the line vector: a cached copy, structural changes under a lock (2026-11-16) ✅

An intermittent test-run segfault (Linux, ~half the runs, `main` too; open-bugs #31) was a race on `TextBuffer::lines`:
the parse thread walked the vector (recolour, region/full parse, structure scan, line hashes) while the UI thread
inserted/appended, and an insert or a growth moves every `Line::Ref`. ASan (Linux) named it: a `kWorkspaceNames` job
queued on open, the test appending a line at once. The per-`Line` locks guard a line's text, not the vector; `CanEdit()`
is effectively always true. **Load-bearing decisions:** keep the `std::vector` (a `list` still races on its links and
makes every line-number lookup O(n); a `deque` moves elements on a middle insert) and keep locking fine-grained - only
the UI thread changes `lines`, under `linesLock`; the parse thread walks a copy of the Refs (`SnapshotLines`), each
line under its own lock. The copy is a shared const vector **made again only when `linesGeneration` moved** (bumped
by every add/insert/erase/clear) - copying per job made a keystroke's region parse O(file): 11 us -> 1.6 ms on
sqlite3.c. Measured (release, sqlite3.c 238k lines; `gedit-bench --recolour` now times 100 keystrokes and 100 Enters):
keystroke 11 us (as main), Enter 1.7 ms Mac / 3 ms Linux (the copy; main 60 us), full parse unchanged. The user
hand-tested typing (fine). Tests: `textbuffer_thstructural` (inserts/deletes during a region parse; 9/10 crashed
before, 0/10 after); Linux ASan clean (reopen x90). Found on the way: open-bugs #32, a git `Process` thread that
posts into Runloop's static queues after `exit()`.

---

## Small items: colours, stale assets, single-folder project, quick-mode switch, warnings (2026-11-16) ✅

Hitlist #21, #23, #24, #29, #22 (Mac half). **#23, load-bearing decision (the user's):** only a clean `goatedit <one folder>` is a
project - config, session, `.goatedit`. No argument, a file, two folders or a file and a folder: no project; each
folder argument whose `.goatedit` goes unused gets a stderr + log warning (two folders could disagree, so neither
wins). `Editor::IsProjectLaunch` decides; `ResolveProjectDir` / `UnusedProjectDirs` follow it; `EstablishProjectDir`
only runs for a project launch. Apart from that, `GetOrAddProjectRoot` never returns the default root, so
`goatedit a.txt .` gets a monitored folder root (open-bugs #21; `workspace_folderafterfile` failed before).
**#24:** the asset dir is a mirror (`cmake/MirrorAssets.cmake` - copy what changed, delete what the sources lack); the
bundle's `SharedSupport` is cleared before the copy, at build and at install. It found `blocktobuffer.js` /
`blocktoclipboard.js` still in the build tree. **#21:** CSS rules, `#rgb` doubles each digit (`themeimport_webcolor`
failed before). **#29:** `]` / `[` dropped from the quickmode keymaps. **#22:** 70 warnings on a clean Mac build, all fixed; sign-compare
by casting the signed side to the unsigned type (what the compiler already did - no behaviour change); one null deref
in `WorkspaceView`'s status line fixed on the way. SDL2 twins edited, not compiled - Linux recount open (open-bugs #19). Next idea from this: an "open folder" command (#36).
**Later the same day:** `-Wshadow` became our own flag (`gedit_warning_flags` for goatedit/utests/gedit-bench,
`-Wshadow-all` on clang), gnklog's options PRIVATE (pinned fbdb625); 72 shadows fixed, Mac + Linux build with no
warnings in our code. No `-Werror` - the user's call: warnings are swept, not enforced. Trap: a local `ext/gnklog`
clone overrides the pin (`cmake/CMakeDeps.cmake`) and must be at fbdb625 or the PUBLIC flags come back.

---

## The workspace tree read up front within a budget: `max_scan_entries` (2026-11-15) ✅

Opening `~/src/apus` (182k files, a Zephyr workspace, no repo at the top) left the editor stuck ~7 s at startup:
`max_scan_depth: 6` read ~46k entries on the main thread (`FolderScanner::Scan`, the `.gitignore` check per entry),
then `WorkspaceView::CreateTree` built a row per node. The code index was not it - it runs on its own threads.
**Load-bearing decision:** a budget of entries, not a lower depth and not a background scan (the node tree is the main
thread's; the view, monitor, session and git status all touch it). `FolderScanner::Options::maxEntries` is turned into a
depth: whole levels from the top while they fit (the top one always), counted level by level, then the same
depth-first walk - so the callbacks nest as before and the Workspace adapter is unchanged. A git-ignored folder is one
entry, not looked into. `workspace.max_scan_entries: 5000`: this repo (746 entries) is read as before; apus stops at
four levels (1,592). Measured (debug, under gdb): main thread 612 -> 165 ticks before idle. Tests:
`folderscanner_budget_levels` / `_firstlevel` (failed before), `_fits`, `_ignored`.
**Also: quitting while the index starts.** `.q!` right after opening apus took 7.3 s to exit (an `-O0` build):
`Indexer::Stop` joined a worker that was inside `ScanRoot`'s walk of the whole tree - one work item, and `isStopping`
is only read between items. `FolderScanner::Options::cancel` (an atomic the walk reads at every entry) is set first
thing in `Stop`; now ~0.5 s. Test: `folderscanner_cancel` (failed before). Not done: freeing a full index at exit
still costs ~2.6 s in release on apus (20M symbols).

---

## Short-file scroll window (2026-11-15) ✅

- open-bugs #30: PageDown / Cmd+End in a file shorter than the view set the scroll window from the line count, so the
  editor drew a short view and scrolled it early (lines added after it pushed the top out, half the view empty, a
  resize put it right). One rule now: `LineCursor::ScrollToEnd(viewHeight, nLines)` - the window always spans the
  view, top clamped to `[0, nLines - height]` - used by every jump-to-the-end path (CLion page-down, the line-down
  clip, `OnActionGotoLastLine`). Cmd+PageDown (`OnActionGotoBottomLine`) no longer lands past the last line.
  Tests: `vnav pagedown_window`, `document shortfile_window`; `vnav pagedown` case 1 asserted the bug and was fixed.
  Found and checked headless (`--backend ansi` in a pty, rendered with pyte).

---

## Minimap contrast, theme fallback, theme licence keys (2026-11-13) ✅

- Minimap: the change marks 4 logical px from the text (`minimap.marks_gap`), the text damped to 0.4
  (`minimap.text_strength`, VS Code measured at ~0.3-0.4), the viewport box's alpha scaled by `minimap.overlay_strength`
  (0.5). Accepted by eye against VS Code (usability §7 follow-up).
- open-bugs #29: a theme that can't be loaded falls back to `default.theme.yml` with a reason on stderr instead of a
  segfault (`Theme::LoadOrDefault`; the colour accessors no longer insert and dereference a null).
- The theme importer writes `author` / `license` / `copyright` / `source` from the theme, its `package.json` and
  `LICENSE` - never a product licence above the package (`theme-collection.md` §3).

## Languages as data, phase 1: language files, `inherit:`, `json.yml` (2026-11-13) ✅

A language can be a YAML file in `Assets/Resources/Languages/`; `json.yml` now drives `.json`/`.js` in place of
`JSONLanguage` (equal over 30 texts / 5049 tokens; the class was then deleted, after the app check on Mac and Linux). **Load-bearing decisions:** `inherit: <name>` (keymap-style, not
include), resolved once per name by `LanguageFileSet` with its own merge - a list replaces the parent's whole, `key+:`
appends - never `YAMLMerge` (which merges lists by position, open-bugs #28); `default.yml` is the root, and
`config.yml` `languages.default` is laid over it, `languages.<name>` over the language; `DataLanguage` makes the same
tokenizer calls a C++ language makes (no second tokenizer); a bad file is rejected whole, with `file:line`; a built-in
wins an extension unless `override: yes`. yaml-cpp trap: `Node::operator=` writes through to a shared node - the
merge uses `reset()` / copy-construction only (`test_datalanguage_merge_inputsunchanged`). Detail:
[`support-languages.md`](support-languages.md).

## Mouse bindings: a `mouse:` section in the keymaps (2026-11-12) ✅

What a click stands for is now in the keymap files: `GotoToggle: "CopyPasteModifier + Click"` (default), `CommitLine:
DoubleClick` (workspace). **Load-bearing decisions:** the view always handles the press first (caret, selection), then
`Editor::DispatchMouseBinding` looks it up and dispatches the action as a key's - the view never runs it itself, so it
runs once; no binding means only the view's handling; clicks and modifiers match exactly (by kind); `mouse:` is NOT
inherited, since an inherited go-to click would jump in the editor from a tree row. Detail:
[`usability-round.md`](usability-round.md) §13.

## The git branch in the status line (2026-11-12) ✅

The status line shows the opened folder's branch at the right: `⎇ main`, `⎇ (a1b2c3d)` when detached. **Load-bearing
decisions:** the branch comes from `--branch` on the status query that already runs (not from reading `.git/HEAD` -
reftable keeps a placeholder there), so no extra process except the short SHA when detached; `ChangeSet::IsSameAs`
compares the branch, or a switch between branches with the same tree would never reach the screen (pinned by
`test_gitstatus_monitor_branch`); the first opened folder's branch, not the active document's. Detail:
[`usability-round.md`](usability-round.md) §9.

## YAML: Enter after `|-` / `>+` block headers indents (2026-11-12) ✅

`indent.yml`'s triggers look at a line's last char, so `run: |-`, `|+`, `>-`, `>+` and `run: |  # comment` didn't indent
the body. **Shipped:** a language hook, `LanguageBase::OpensBlockAtEnd` (-> `IndentEngine::Context::isOpenedByLanguage`),
beside `GetContinuationColumn`. **Load-bearing decisions:** a hook, not `-`/`+` in `indent_after` (that fires on `---`
and `1.0-`); the header must be the whole value (after `key:`, list markers or nothing), and uses the tokenizer's header
list so Enter agrees with the colouring. Detail: [`support-yaml.md`](support-yaml.md).

## Checked: GotoImplementation starts no stray selection (2026-11-12) ✅

An investigation (hitlist #3). `Cmd+Shift+B` / `Ctrl+Shift+B` is not tagged `Selection`: only an optional
`@SelectionModifier` tags a binding, and only on a special key, so `Document::OnAction` never begins a selection for
it (nor for `GotoBase`, `GotoType`, `ShowRelationships`, `PrevSearchResult`). No code change; two guard tests through
the real keymap, which fail if tagging takes in required modifiers or letter keys. The note in
[`usability-round.md`](usability-round.md) §11 that assumed otherwise is corrected.

## Cmd+click / Ctrl+click goes to definition (2026-11-12) ✅

A single click with `CopyPasteModifier` held (`Cmd` on macOS, `Ctrl` on Linux) moves the caret to the name and posts
`GotoToggle`, as `Cmd+B` does. **Load-bearing decisions:** the modifier comes from the keymap (`KeyMapping::ModifierMask`,
the `modifiers:` table kept after the build, a child's own winning), not a hard-coded key; the action is posted through
`Runloop::DispatchAction`, not called, so the jump lands on the navigation stack; only `clicks == 1`. A keymap `mouse:`
section is planned to make the binding itself data (usability-round §13). Detail: [`code-index.md`](code-index.md) §5.6.

## Folder double-click in the workspace tree (2026-11-12) ✅

A double-click on a folder did nothing (`OpenSelectedItem` returned early on a folder). **Shipped:** a double-click or
Enter on a folder expands or collapses it (`TreeView::ToggleExpanded`); an unscanned folder fetches its children on
the way, as the arrow key does. **Load-bearing decisions:** Enter toggles too - it shares the one open path with the
double-click; act on `clicks == 2` only, since SDL counts on and a triple-click would toggle back. Detail:
[`usability-round.md`](usability-round.md) §10.

## System font fallback (2026-11-11) ✅

After the theme font and `sdl.font_fallback`, the system is asked (CoreText / fontconfig) for a font with the
character, behind `sdl.system_font_fallback` (default on). Prompted by open-bugs #26 (Claude Code's `/context` grid).
Verified on macOS and Linux. Detail: [`system-font-fallback.md`](system-font-fallback.md).

## YAML language support (2026-11-11) ✅

`.yml`/`.yaml` highlighting and editing. **Shipped:** `YAMLLanguage` - comments and quoted strings in the
tokenizer; keys, list markers, document markers, typed scalars (`true`, `8080` - only as the whole value),
anchors/aliases/tags and one-line flow collections in a post-pass; block scalars (`run: |`) through two
general tokenizer additions (`kPushStateAtEOL`, indent-scoped states); Enter indents after `key:` and lines
up with an item's key after `- name: x`. Verified by the user in the app (ANSI backend). **Load-bearing
decisions:** a new class `kConfigKey` (not `kKey` - too generic; placed before the semantic block, which
`[kUserType, kLastTokenClass)` defines), teal like C++ class names (red read as errors); a block scalar's scope
is the key's column, not the line's indent (`- run: |` must end at its sibling `name:`); the indent-scope pop
runs before the depth stamp, in both `ParseLine` and `TokenizeText`; reformat leaves YAML alone (`reindent: no`)
- nothing closes a `key:`, so the opener walk would staircase the file. Detail: [`support-yaml.md`](support-yaml.md).

---

## Terminal emulation library `gvt` (2026-11-10) ✅

`VTermParser` rewritten as a Williams streaming state machine; parser and screen model are the standalone
`src/ext/gvt` library behind `IParserSink` / `ITerminalHost`, libvterm a test-only oracle. VT-0..VT-9 done and merged;
`TerminalScreen` runs on `gvt::Terminal`. What's left (the `claude` exit criterion, VT-10) is hitlist #32. Detail:
[`terminal-vt.md`](terminal-vt.md), gap analysis [`terminal-enhancements.md`](terminal-enhancements.md).

## Usability round (2026-11-08 - 2026-11-09) ✅

The change set in the workspace tree, reload on disk change, changed lines in the gutter, open files on `Cmd+E`,
splitters dragged with the mouse, type-to-find in the tree, `Cmd+P` quick open, close all and `!`, headless drawing on
a Gansi grid, glyphs beyond 255, the overview (minimap), select all. Still open, in the hitlist: §3 c text search
(#30), §7 step 3 the minimap in the terminal (#31), the notes §4, §14-§16. Detail: [`usability-round.md`](usability-round.md).

## Install layout and asset lookup (2026-11-09)

The Linux build only started from inside its build dir: system assets were looked up in `./resources/`
(the cwd) or an installed `/usr/share/goatedit`. **Shipped:** `InstallPaths` finds exactly one system asset
dir relative to the executable (build tree, macOS bundle, any install prefix, `$XDG_DATA_DIRS` last;
`--asset-dir` overrides); the user layer is `~/.config/goatedit` on both platforms, `~/.goatedit` read-only (not `~/.local/share/goatedit`: a `~/.local` install puts its system assets there);
log and user-scope terminal history in `~/.local/state/goatedit/`; `scripts/install.sh --user|--system`
with a manifest-driven `--uninstall`. **Load-bearing decisions:** never the cwd for assets (a stray
`resources/` could take over); a dev tree and an install are never mixed (one system dir); within a location
type the loader keeps registration order (the old hit-count reordering let a legacy dir overtake the
preferred one). Also: the terminal no longer hangs startup when the shell can't start, and the default
shell is `$SHELL`. Detail: [`install-layout.md`](install-layout.md).

---

## Code index - structural search over the code base (in progress)

The base for "search everywhere": a lexer-driven index of symbols and their relationships, not plain text
search. **Shipped:** step 0 (`TokenizeText` fast path, `gedit-bench`, tokenizer 3-4x faster), step 1 (C/C++
structure pass: symbols, inherits/holds/includes), step 1b (semantic highlighting from the file's own
declarations, kept per `Line`), step 1c (VS Code / Sublime theme import, `--import-theme`,
`.theme --import`), step 1d (library names), step 2b (smart go-to on `Cmd+B`, navigation stack on `Cmd+[` / `Cmd+]`,
kept across a restart), step 2 (in-memory index, background indexer, Go to symbol on Cmd+T / Ctrl+T, open files fed
from their buffer, disk changes through `FolderMonitor`, `ExclusionRules` with real `.gitignore`, cross-file colouring).
**Load-bearing decisions:** no compiler or language server; anything per-buffer rides
the highlighter's line/region machinery, never a whole-buffer copy or re-lex per edit; themes are converted
once, not read at runtime (D13); library names are shipped data, not a toolchain scan (D14); a name with several
meanings in the workspace stays plain, and without locals in the index, context decides the rest (§11.6); a library
never takes a name the project declares, and a bare C++ name needs a `using` in the file (§10.2). Step 1d shipped too: hand-made `c_std` and
`cpp_std` lists, `std::` by syntax, the `using` tiers, project lists in `.goatedit/libraries/` and lists that apply by header
(§11.8). Step 3 (2026-11-06): edges in the index, open files included; the relationship view (`Cmd+Shift+T`) with the code of each
relation in colour; go to implementation and base (`Cmd+Shift+B`, `Cmd+Shift+U`) (§11.10). The longer aim, keeping the architecture
in view while working with an agent, is in §10.3; a context menu at the caret (`Cmd+Return`) is an idea in §5.7.
**Paused 2026-11-08:** step 4 (modules) is next, once its design is thought through. Detail: [`code-index.md`](code-index.md).

## Project config — `<root>/.goatedit/config.yml` merged at startup ✅ (on `feat/project-config`)

Config now merges **system → user → project** (last wins). The project root is resolved *before* the
config load by `Editor::ResolveProjectDir(args, cwd, home)`: the LAST absolute folder argument, else the
cwd (after the outside-`$HOME` relocation, which moved ahead of the config load). Same guard as
`EstablishProjectDir` — strict subdir of `$HOME` only, and `.goatedit/` must already exist (resolving
never creates it; a folder open may still auto-create it later, but its config can only apply next
launch). Because the merge happens with the rest of the config, logging/theme/languages/views all see it.
- **Denied keys** (a cloned repo may carry `.goatedit/config.yml`, so opening a folder must not run its
  commands): `main.bootstrap_scripts`, `main.plugin_directory`, `terminal.shell`, `terminal.init`,
  `terminal.bootstrap`, `logging.logfile` — stripped by `ProjectConfigPolicy::StripDeniedKeys` (a denied
  section given as a non-map is dropped whole); a non-map document is ignored. (Now in
  `ProjectConfigPolicy` - see the `.cfg` entry above.)
- `--skip-user-config` skips the project config too.
- Sequences still merge **by index** (`YAMLMerge`) — a project can override/extend a list, not shorten it.
- Behaviour change: a folder argument outside `$HOME` no longer leaves the *launch dir's* `.goatedit`
  registered as `kProject` (it was, since the init-time registration used the launch cwd).

## `.cfg` — read/write config from the command line ✅ (on `feat/project-config`)

`.cfg [--user | --project] <action> [args]` (plugin `config`, short `cfg`; `Scripts/config.js` →
global JS `Config` (`ConfigAPIWrapper`) → `ConfigAPI`). Actions: `list [prefix]` (running config with the
level each value comes from, or the `--user`/`--project` file as-is), `get <key>`, `set <key> <value>`,
`edit` (opens the level's file in a buffer; creates it with a header comment if missing).
- **No `--global`:** the system config is never written from here (changing the default for all users
  is a deliberate, by-hand act).
- **Nothing is kept per level.** Each call loads the user/project files fresh from disk into temporaries
  (a hand-edit since startup is never clobbered). `set` writes the target file, then puts the EFFECTIVE
  value (project > user > system) into the running config - a user value the project overrides warns
  and leaves the running value alone. `--skip-user-config` instances only get the file written.
- **Comments are sacred** (they're used to toggle logging): `ConfigFilePatcher` edits the TEXT in place
  - replaces the one value (keeping a trailing `# comment`) or inserts missing keys at the end of the
  deepest existing section; every other line byte-identical. **No fallback:** the result is re-parsed
  and must equal "original + this one value", otherwise nothing is written and the user is pointed at
  `.cfg --<level> edit`. Refuses flow style, list/map targets, tabs, CRLF, unparseable files.
- Writes are atomic (tmp → rename) and go **through** a symlinked config (dotfiles) to its target.
- The project deny-list moved out of `Config` into `ProjectConfigPolicy` (used by the startup merge and
  by `set --project`).
- Live effect is limited to settings read after the set; startup-read settings (theme, languages,
  keymaps, a view's construction-time options) need a restart - `set` says so.

## CMake cleanup (2026-06-23) ✅

FetchContent for deps (SHAs pinned), the flat source list grouped into named variables (one target), per-compiler
flags, deps/flags/packaging in `cmake/*.cmake`. Two non-blocking follow-ups in `deferred.md`. Detail:
[`cmake-cleanup.md`](done/cmake-cleanup.md).

## Soft-wrap (word/char line wrap) ✅

Long lines continue on the following screen rows instead of clipping at the view edge. Off by default;
`editorview.wrap: none | char | word` in config. Deferred tail: [`deferred.md`](deferred.md) (Soft-wrap).

- **`WrapProjection` (`src/Core/WrapProjection.{h,cpp}`)** — pure, stateless, sibling of `HexProjection`
  (deliberately NOT "layout", to stay clear of the UI layout classes). `WrapLine` splits a line into row
  segments; `ProjectRows` gives the visible rows; `TextToScreen` / `TextToScreenClamped` / `ScreenToText`
  map between text (line, char) and screen (row, col). Tab stops stay LINE-relative across a wrap
  (`segment.visualStart` feeds `LineRender::ExpandTabs`'s `startCol`); every segment holds ≥1 char; word
  mode lets whitespace on the break hang off the row and hard-breaks words wider than the view.
- **Gutter and editor share ONE row list, via the Document — the views never link to each other.** The rows
  are per-view state: `DocumentViewState::ProjectVisibleRows` (+ `wrapMode`/`wrapWidth`, pushed in by
  `EditorView` from config and its content width on every `(Re)InitView`). Both views read
  `Document::GetVisibleRows()`. No cache — only the visible lines are wrapped, and `TextBuffer` has no
  revision counter to invalidate on.
- **Model stays logical; the view translates.** Cursor / selection / search stay in text coordinates.
  Caret, selection + search overlays and mouse clicks map through the rows. Overlay ends off-view clamp to
  row -1 / row N — the existing "rows strictly between start and end are fully covered" overlay rule (all
  three backends) then paints wrapped spans with no backend change. `LineRender::DrawLines` became
  `DrawRows` (one code path: without wrap each row is a whole line).
- **Scroll correction at one chokepoint.** Navigation still scrolls in LINES (`VerticalNavigationViewModel`);
  `DocumentViewState::ScrollActiveRowIntoView` advances `viewTopLine` until the caret's row fits, keeping
  `viewBottomLine = top + height` and `position.y = idxActiveLine - top` so the line-based nav stays
  consistent. Called from `Document::GetVisibleRows()` — every consumer reads rows there, so every caret
  move (keys, typing, goto, wheel, search from another view) is covered. No-op without wrap.
- Tests: `wrapprojection` module (tiling property, break rules, tabs, mapping round-trip, scroll
  correction); `test_document_wrap_caret_stays_visible` drives real navigation with wrap on (fails without
  the correction).

## ANSI/TTY graphics backend — `gansi` ✅ (merged to `main`, `e9fa64f`)

A modern-terminal rendering backend so the editor runs in a TTY / over SSH, selectable alongside SDL
(`--backend ansi` or `main.backend: ansi`; a plain terminal session now auto-selects it). NCurses was
removed entirely. Detail + full phased plan: [`done/ansi-graphics-backend.md`](done/ansi-graphics-backend.md).

- **Standalone library `gansi` (`gnilk::ansi`), NOT editor code** — its own folder
  (`src/ext/gansi/`), own `CMakeLists.txt` (static target `gansi`, alias `gnilk::ansi`,
  `add_subdirectory`'d by the editor — never source-listed), own trun tests (`gansitests`), zero editor
  dependencies. Think "our SDL": a cell grid + ANSI encoder + input parser + a single platform seam.
- **Cell model + damage-tracked encoder** (`CellGrid`, `AnsiEncoder`): double-buffered grid; the diff
  emits only changed cells, one CUP per contiguous run, an SGR only when the truecolor pen changes
  (full repaint only on resize). Byte-for-byte unit-tested.
- **Input parser** (`InputParser`): split-read-tolerant byte→`Event` state machine — UTF-8, C0
  controls, CSI/SS3 special keys with xterm modifier params, **Kitty keyboard (`CSI u`)** (fixes
  Shift-Arrow + Alt-as-nav, the SDL/macOS pain), SGR mouse (1006), bracketed paste, focus.
- **Platform seam** = `ITerminalIO`; `PosixTerminalIO` is the only termios/poll/SIGWINCH file (macOS +
  Linux share it; Windows would be the sibling impl). `MockTerminalIO` (public header) drives everything
  headlessly — the whole library + backend are unit-tested with no real TTY.
- **Backend `gedit::Gansi`** mirrors the SDL layout: `GansiScreen`/`GansiWindow`/`GansiDrawContext`/
  `GansiKeyboardDriver`. One shared back-buffer; windows are clip rects; `DrawContext` writes
  `origin+local` cells (no pixels/scaling). Inverse swaps fg/bg, underline/bold→SGR, `DrawHRule`→box
  row, overlays→invert covered cells. Real terminal hardware cursor; OSC 52 clipboard out + bracketed
  paste in (honours the **E.18** external round-trip). `CopyToTexture`/`ClearWithTexture` = grid
  snapshot/restore.
- **Load-bearing decisions:** library stays under `gnilk::` (the signal it's not editor code); coexists
  with SDL (a third backend, *not* the SDL2-XOR-SDL3 split) under `GEDIT_BUILD_ANSI`; no
  `WireScreenGeometry` (a TTY owns its own size). TDD throughout — each unit's tests written first
  against a stub (RED) then implemented (GREEN); ~90 tests across `gansitests` + `test_gansibackend`,
  plus a real-pty smoke confirming the init/teardown escape sequences.
- **Terminal capability detection** (`gansi/Capabilities.{h,cpp}`): `Detect(TERM/TERM_PROGRAM/COLORTERM)`
  → feature profile; `Terminal::Open/Close/Flush/SetClipboard` gate every emitted sequence on it, and
  `AnsiEncoder` degrades truecolor→xterm-256 (`Rgb256`, cube + grayscale-ramp nearest) when 24-bit isn't
  advertised. Fixes the macOS **Terminal.app** stray-glyph: it was the Kitty-keyboard enable
  (`CSI > 1 u`) printed verbatim by a terminal that can't parse it. Apple_Terminal now gets
  Kitty/focus/SGR-mouse/OSC 52 gated **off** + 256-color; capable terminals (Ghostty/kitty/foot) keep
  full fidelity. Env-heuristic for now (runtime DA/XTVERSION query is a possible later refinement).
  Verified end-to-end via the pty harness (Apple_Terminal vs ghostty startup streams).
- **Post-ship hardening on real terminals (macOS Ghostty/Terminal.app/iTerm + Linux):** bracketed paste
  now *inserts* — the terminal handles Cmd+V / Ctrl+Shift+V itself and delivers a paste *event* (not a
  keypress), so it's routed through a new shared `Runloop::DispatchAction` (the action peer of
  `ProcessKeyPress`/`ProcessMouseEvent`, one dispatch chokepoint) as a normal `PasteFromClipboard`.
  Keyboard-modifier-reporting limits (iTerm pre-3.5 drops Shift+Arrow, Terminal.app composes Option to a
  glyph) are terminal-side and unrecoverable app-side — documented in §9, with the low-level OS
  interception "fix" (`CGEventTap`/IOKit) recorded as a rejected dead end. Kitty CSI-u functional-key
  decoding deferred ([`deferred.md`](deferred.md)). Verified on macOS+Ghostty and Linux.

---

## Mouse double-click — open a workspace file ✅ (merged to `main`, `7d4bcca`)

Double-click a file row in `WorkspaceView` to open it + focus the editor (like Enter), via a pure
`MouseClickTracker` (click-count on `MouseEvent.clicks`, `mouse.dbl_click_speed` config; no backend or
`Timer` edits — detection sits at the `Runloop::ProcessMouseEvent` chokepoint, runloop-thread, lockless).
Fixed along the way: `RootView::DispatchMouse` focus-follows-click now fires on press/wheel, not the
trailing release (which was clobbering the editor focus the double-click just set). Detail:
[`mouse-dblclick.md`](done/mouse-dblclick.md).

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

## Terminal scrollback + command blocks — Phases 0–3 + 5 ✅ (Phase 4 deferred) — blocks REMOVED 2026-11-10

> **2026-11-10, owner's call: command blocks are gone** (branch `feat/gvt`). Never used in practice, and the
> selected-block highlight jumping between short blocks made wheel-scrolling flicker. What is left is plain
> scrollback (line-granular trim and persistence, `session.yml` keeps only the `.bin` name) plus one marker -
> where the last command's output starts (`TerminalScreen::MarkOutputStart`, set at commit or OSC 133;C) - for
> `Terminal.GetLastOutput / OpenLastOutputAsDocument / CopyLastOutputToClipboard` (a stack trace to copy). Removed:
> `CommandBlock`, block eviction/persistence, the highlight, end-of-block rules, Alt+Up/Down prompt jumps, OSC 7
> cwd, the block JS API and the `blocktobuffer`/`blocktoclipboard` plugins. The text below is history.

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
- **Phase 4 — per-block syntax highlighting — EXTRACTED** to its own doc,
  [`syntax-blocks.md`](syntax-blocks.md) (optional, independent, not started). The only seam the shipped
  work had to preserve — the `language` field on `CommandBlock` + the persisted `TerminalBlockSession` —
  is in place. Listed under Planned / not started below.
- **Found along the way:** [`open-bugs.md`](open-bugs.md) #11 — a raw TAB byte from the pty was silently
  dropped, garbling multi-column shell output (e.g. plain `ls`); pre-existing, unrelated to this feature,
  documented but not fixed.

Detail + phase-by-phase status: [`terminal-scrollback.md`](done/terminal-scrollback.md).

---

## Alpha normalization — `ColorRGBA::a` on a single 0..1 convention ✅ (2026-06-28)

`ColorRGBA::a` is a 0..1 fraction everywhere; the lone violation was the theme's `alpha(224)` — a stray
0..255 magnitude (hand edit) that leaked into the `selection` color, overflowing the Gansi overlay blend
(the green-selection bug 10) and surviving in SDL only by an accidental Uint8 wrap. **Load-bearing
decision:** the Sublime spec says `alpha()` is 0..1 and the theme already authored every *other* alpha
0..1, so the *value* was the defect, not the reader — fix was to correct `alpha(224)` → `alpha(0.12)`
(both `colors.json`), make `ExecuteAlpha` a faithful 0..1 boundary (store verbatim + clamp + warn), and
drop both Gansi stopgaps. SDL/JS/serialization unchanged (alpha ≤ 1 ⇒ no wrap). Pinned by
`test_theme_alpha` + the updated `test_gansibackend_overlay`. Detail: [`alpha-normalization.md`](alpha-normalization.md).

## Planned / not started

Moved to [`hitlist.md`](hitlist.md) (2026-11-13) - the one index of what's next, open bugs worth fixing included.
