# Hitlist - everything planned, in one place

**The one index of what to do next.** Each row is a pointer; the detail lives in the linked doc. Rules:
- New planned work - a design note, a "later" from a review, a bug we mean to fix - gets a row here when it is written
  down, whatever doc holds the detail.
- Bugs stay described in `open-bugs.md` (stable numbers, "open-bugs #N"); one gets a row here when it is worth fixing.
  Hitlist numbers are plain `#N` and stable too - a finished row moves to "Done" at the bottom, never renumbered.
- Done: strike it, move it to "Done", add a line in `work-log.md`.
- Parked work (explicitly deferred tails of shipped features) stays in `deferred.md`, not here.

Made 2026-11-11, cleaned up 2026-11-13. Sizes are estimates.

## To try in the app

Built and unit-tested, not yet tried by hand on that platform. Linux = SDL2 (Ctrl), Mac = SDL3 (Cmd).

**Scripted:** `python3 scripts/handtests/checks.py --exe cmake-build-debug/goatedit [check ...]` runs these in a pty on the ANSI
backend, in a sandbox HOME, and reads the screen back with its colours (`--list` names the checks, ~2.5 min for all). A box
ticked with `(scripted: <check>)` was ticked from that run, not by hand - it covers the editor, not the SDL backend's own
keys and drawing. Linux run 2026-11-17: all pass (`sdl_no_display` since the #49 fix).

- **Undo clears the `*`** (#39)
  - [x] Linux [x] Mac - mistype, undo: the `*` goes after a pause; type a char + backspace: same; edit, save, undo: `*` stays (scripted: `undo_star`)
    - with autosave on (default 1 s) edit, save, undo is autosaved and the `*` then goes - right; the last part holds with `autosave_timeout_ms: 0`
- **Text encodings** (#16, open-bugs #15)
  - [x] Linux [x] Mac - open a Latin-1 file: å/ä/ö right, status line `cp1252`; edit + save: `git diff` shows only the edit (scripted: `latin1`)
- **Go to line, line numbers from 1** (#42, #43)
  - [x] Linux [x] Mac - `Esc g 42 Enter` and `.g 42`: on the line the gutter shows as 42; `Ctrl+[` comes back; status `l: 42` (scripted: `goto_line`)
- **Workspace tree clicks** (#1 / #15, `CommitLine: DoubleClick` in `workspace_keymap.yml`)
  - [x] Linux [x] Mac - double-click a closed folder: opens; again: closes
  - [x] Linux [x] Mac - Enter on a folder: toggles
  - [x] Linux [x] Mac - triple-click a folder: toggles once
  - [x] Linux [x] Mac - double-click a git-ignored folder (`cmake-build-debug`): its contents show
  - [x] Linux [x] Mac - double-click a file: opens
  - [x] Linux [x] Mac - Ctrl/Cmd+click a tree row: nothing happens in the editor (`mouse:` is not inherited)
- **Go-to click** (#2 / #15, `GotoToggle: "CopyPasteModifier + Click"` in `default_keymap.yml`)
  - [x] Linux [x] Mac - Ctrl/Cmd+click a function used in another file: jumps to its definition
  - [x] Linux [x] Mac - Ctrl/Cmd+`[`: back
  - [x] Linux [x] Mac - Ctrl/Cmd+click a declaration in a header: toggles to the definition
  - [x] Linux [x] Mac - plain click: only moves the caret; Ctrl/Cmd+click on whitespace: nothing
  - [x] Linux [x] Mac - Ctrl/Cmd+double-click a name: no second jump
  - [x] Linux [x] Mac - Ctrl/Cmd+Shift+click: no jump (modifiers match exactly now)
- **YAML Enter** (#4), in a `.yml`, Enter at the end of:
  - [x] Linux [x] Mac - `run: |-` -> next line indented 2; `  - run: >+` -> 6; `run: |  # x` -> 2
  - [x] Linux [x] Mac - `---` and `version: 1.0-` -> no indent
- **Git branch in the status line** (#5)
  - [x] Linux [x] Mac - `goatedit .` in this repo: `⎇ main` at the right of the status line
  - [x] Linux [x] Mac - `git switch -c tmp` in the terminal panel: `⎇ tmp` within ~1 s, no keypress
  - [x] Linux [x] Mac - `git switch --detach`: `⎇ (<sha>)`; `git switch main`: back
  - [x] Linux [x] Mac - opened on a folder outside any repo: nothing shown
- **Languages as data** (#10) - Linux built + tests green (743), not tried in the app
  - [x] Linux [x] Mac - a `.json` file highlights as before; `.ll` lists the language files with their paths (scripted: `json_ll`)
  - [x] Linux [x] Mac - `languages: { default: { tabsize: 8 } }` in the user config reaches json and cpp (scripted: `tabsize`)
- **Theme that can't load** (open-bugs #29 fix) - Linux built + tests green (774)
  - [x] Linux [x] Mac - `main.theme: nosuch.theme.yml`: starts, stderr says "no such theme file - using default.theme.yml" (scripted: `missing_theme`)
- **Minimap contrast** (usability §7 follow-up) - Linux built + tests green (774)
  - [ ] Linux [x] Mac - a changed line's mark stands clear of the text; the viewport box is subtle
- **Big folder** (`max_scan_entries`, indexer cancel) - Linux built + tests green, measured by script, not by hand
  - [x] Linux [x] Mac - `goatedit .` in a big tree (`~/src/apus`, an npm project with `node_modules` not ignored): the window answers at once (scripted: `big_folder` - 60000 files, view switches ~120 ms while scanning)
  - [x] Linux [x] Mac - folders below the fourth level open when expanded; a session file deep in the tree reopens (scripted: `big_folder`)
  - [x] Linux [x] Mac - `.q!` right after start-up: exits at once (scripted: `big_folder`)
  - Mac 2026-11-17: responsive in `~/src`, but every quit crashed - the terminal's pty reader outlived its Shell. Fixed on `fix/shell-exit-crash` (808d5fb); re-checked 2026-11-17: the three above hold, quitting is instant, no crash
- **Single-folder project** (#23) - Mac checked in a pty (`--backend ansi`), not in the SDL app
  - [x] Linux [x] Mac - `goatedit a.txt .` in a repo with a `.goatedit`: stderr says "not a single-folder launch - project settings in '<dir>/.goatedit' not used"; no session restored; a file created in the terminal shows in the tree (scripted: `single_folder`)
  - [x] Linux [x] Mac - `goatedit .`: the session restores as before, no warning (scripted: `single_folder`)
  - Mac: not seen through `goat` - it launches via `open`, stderr goes nowhere. See #50
- **Quick mode without `]` / `[`** (#29)
  - [x] Linux [x] Mac - Esc, then `]`: no view switch; `Alt/Option+]` still cycles (scripted: `quick_bracket`)
- **Stale assets** (#24) - Mac build + scratch install checked
  - [x] Linux - `scripts/install.sh --user` after a file is removed from `Assets/Resources`: gone from `~/.local/share/goatedit` (scripted: `install_stale`, sandbox HOME)
- **Terminal Ctrl+C / xterm Alt** (#45)
  - [x] Linux [x] Mac - terminal: type `echo abc`, `Ctrl+C`: the text stays, a fresh prompt below, nothing runs
  - [x] Linux - `goatedit --backend ansi` in `uxterm`: `Alt+T` / `Alt+E` switch views (no `ô` typed); Alt works in uxterm after quitting as before
- **ANSI backend when there is no display** (#46)
  - [x] Linux - over ssh (no `-X`), plain `goatedit`: the ANSI backend
  - [x] Linux - in the desktop session: a window (scripted: `desktop_window`)
  - [x] Linux - `--backend sdl` with no display: fails with a message, no crash dump (#49; scripted: `sdl_no_display`)
  - [ ] Mac - `--backend sdl` over ssh: fails with a message (SDL3 has the same #49 fix)
  - [x] Mac - the window quits cleanly through `SDL_QUIT` (no `exit()` any more): SIGTERM, Cmd+Q and the close button - exit 0, no crash report (scripted by hand, 2026-11-17)
  - [x] Mac - `ssh localhost`, plain `goatedit`: the ANSI backend; from Terminal.app: a window
  - Mac 2026-11-17: `goat` over ssh opened a window - `open` starts the app with launchd's environment, no `SSH_CONNECTION`. Fixed on `fix/shell-exit-crash` (5658fc0): over ssh `goat` runs the binary in the terminal; needs `install-cli` to regenerate `~/.local/bin/goat`
- **xterm Ctrl chords in the ANSI backend** (#44)
  - [x] Linux - `goatedit` in `uxterm` (TERM=xterm) over ssh: `Ctrl+/` comments the line, `Ctrl+I` reformats the block (not a Tab), `Ctrl+]` jumps forward, `Ctrl+[` comes back; Tab, Enter, Backspace, `Ctrl+C` in the terminal as before; after quitting, the shell's `Ctrl+R` / `Ctrl+A` still work
    - Linux 2026-11-17: after `--backend ansi`, `Ctrl+R` in uxterm drew its search fg == bg. The editor left its last
      colours (the status line's) set on exit and xterm's `?1049l` doesn't restore colours. Fixed: `Terminal::Close` resets
      SGR before leaving the alt screen (`test_terminal_close`); checked by replaying the exit in uxterm. The rest of the
      item checked by hand in uxterm (2026-11-17)
- **Promote to terminal** (#47) - `Cmd/Ctrl+Enter` in the editor
  - [x] Linux [x] Mac - a line, no selection: it runs in the terminal, focus stays in the editor; a selection of several lines: all run, the selection stays (send again) (scripted: `promote`)
  - [x] Linux [x] Mac - `claude` in the terminal: a selected prompt arrives as one paste and is sent; `promote_sends_enter: no` in the user config: it waits in the chat unsent (scripted: `promote_paste` - a raw-mode program with bracketed paste on stands in for claude: `\e[200~...\e[201~` then `\r`, no `\r` with the setting)
- **CMake highlighting** (#11 step 1)
  - [x] Linux [x] Mac - open `CMakeLists.txt` and a `*.cmake`: commands blue (any case), `if`/`foreach`/`function` as keywords, `PUBLIC` / `REQUIRED` / `VERSION` red (not `public`), `${VAR}` (nested too, in strings too), `$<...>`, `#[[ ]]` comments over lines, `ON`/`OFF`; `.ll` lists `cmake` with `CMakeLists.txt`; `Cmd/Ctrl+/` comments with `#` (scripted: `cmake`)
    - Linux 2026-11-17: Ctrl+/ twice turned `#set(X 1)` into `et(X 1)` - uncommenting always removed 2 chars (any one-char
      prefix: CMake, YAML). Fixed in `Document::AddLineComment` (`test_document_comment_toggle`)
- **Esc in the ANSI backend** (a lone ESC waited out the 250 ms poll; now 25 ms - 253 -> 27 ms measured in a pty)
  - [x] Linux - `goatedit` in uxterm / over ssh: Esc into quick mode and a command feel immediate; Alt+T / Alt+E still switch views
- **Short-file scroll window** (#35, open-bugs #30)
  - [x] Linux [x] Mac - a new file, a dozen lines, PageDown, then Enter in the middle: the top line stays until the view is full (scripted: `short_scroll`)

## Next - suggested order

| # | Item | Where | Size | Notes |
|---|---|---|---|---|
| 50 | Start-up warnings ("not a single-folder launch", a missing theme, ...) only go to stderr - lost when started through `goat` (`open`) or from a launcher. Show them in the terminal panel too, or in a status message | `Editor` start-up, `TerminalController` | half a day | found in the #23 hand-test 2026-11-17 |
| 48 | ANSI backend on macOS: the keymap's `Cmd` shortcuts (`CopyPasteModifier: KeyCode_Cmd` - copy/paste, undo, find, comment, save...) can't be pressed - a terminal never sends Cmd (Terminal.app, iTerm2 legacy), only Kitty's `super` bit where the terminal forwards it (Ghostty/kitty, usually not) | `Assets/Resources/macOS/*_keymap.yml`, `KeyMapping` (modifier names), `GansiKeyboardDriver` | hours | options: a per-backend modifier map (in ANSI, `CopyPasteModifier` = Ctrl on macOS too, as on Linux), or an ANSI keymap that inherits the Linux one. Check what Ctrl chords Terminal.app eats; `Ctrl+Space` etc. Found writing #44 |
| 40 | User documentation: the keyboard layout per platform (the shipped keymaps, modifier groups), the shipped dot/quick commands, the mouse bindings | `docs/usage.md` (a short first version, 2026-11-16) | a day | discussed, never written down; generate from the keymap YAML where possible so it can't drift; ties in with #8 |
| 41 | Gaps found writing `usage.md`: no word left/right on macOS; no drag-to-select in the editor; `.openfolder` is registered in `jsplugs.yml` but `Scripts/openfolder.js` doesn't exist (see #36) | `docs/usage.md` | hours | small, each on its own |
| 17 | A config list overwrites the default list by position | open-bugs #28 | half a day | idea: an immutable merge that returns a new tree; `LanguageFileSet::Merge` is the model |
| 11 | Languages phase 2, step 2: Python (the prefix rule for decorators, triple-quoted strings; numbers and case folding are in since step 1) | `support-languages.md` §6 | a day | CMake (step 1) is done |
| 12 | Languages phase 3: the user directory, `.lang_reload`, `override: yes` | `support-languages.md` | a day | user language dirs aren't read yet |
| 18 | Theme collection step 2: `Themes/` folder, lookup order, install rule, default theme moved | `theme-collection.md` §2 | half a day | the same themes on Mac and Linux from git |
| 19 | `.theme list` / `.theme use <name>` | `theme-collection.md` §5 | half a day | after 18 |
| 20 | Ship Dark 2026 (from VS Code) + `Themes/LICENSES.md`, its own terminal palette | `theme-collection.md` §4 | half a day | copyright line from the `microsoft/vscode` repo first |
| 6 | Gansi copy fallback: `pbcopy` / `xclip` / `wl-copy` when local | open-bugs #20 notes | half a day | fits with #20 |
| 25 | `Cmd` held underlines a name a click would jump from | usability §14 | half a day | decided: only names the index resolves, an underline; needs the hover plumbing (as 26, 27) |
| 26 | Button widget in the UI toolkit | usability §16 | a day | needed by 27 |
| 27 | Close button per file in the header (`*` / `x`, CLion style) | usability §15 | half a day | after 26 |
| 8 | Key binding groups: `FunctionModifier`, `CodeNavigationModifier`, `docs/keybindings.md` | usability §11 | days | decide the `CodeNavigationModifier` key first |
| 9 | The terminal version on macOS uses the Linux keymaps | open-bugs #20 | days | after 8 |
| 28 | Key bindings for script commands (`ReformatLine` as a quick command) | usability §4 | ? | note, not scheduled |
| 30 | Workspace text search on `Cmd+Shift+F` | usability §3 c | days | design first |
| 7 | Image view phase 0: `BufferBase`, `TextBuffer` / `ImageBuffer` | `image-view.md` §2.4 | a day | parked 2026-11-11 |
| 13 | Image view phases 1-3 | `image-view.md` | days | after 7 |
| 14 | Languages phase 4: every language from a file; `indent.yml` / `autopairs.yml` move in | `support-languages.md` §2.2 | days | |
| 31 | The minimap in the terminal (Gansi graphics: kitty / sixel) | usability §7 step 3 | days | |
| 32 | Terminal: run `claude` inside goatedit (the gvt exit criterion); copying older command output | `terminal-vt.md` "Next" | ? | the copy modal is an idea, not decided |
| 33 | Code index step 4: modules | `code-index.md` | days | paused 2026-11-08 until the design is thought through |
| 34 | Per-block syntax highlighting in the terminal | `syntax-blocks.md` | days | deferred |
| 36 | An "open folder" command (a dot-command and/or a key) - open a folder as the project after start-up | - | ? | no command for it yet; since #23 only `goatedit <one folder>` is a project, so this is the other way in. Design: what happens to the open files and the session |

Smaller bugs not ranked yet: open-bugs #1 (`AttributeAt`, read before touching it), #24 (readline history extras),
#26 (`/usage` glyphs).

## Done

| # | Item | When |
|---|---|---|
| 1 | Double-click (and Enter) on a folder expands or collapses it (usability §10) | 2026-11-12 |
| 2 | `Cmd+click` / `Ctrl+click` jumps as `Cmd+B` does (code-index §5.6) | 2026-11-12 |
| 3 | Checked: `GotoImplementation` starts no stray selection (usability §11) | 2026-11-12 |
| 4 | YAML: `\|-` / `>+` headers auto-indent the next line | 2026-11-12 |
| 5 | The git branch in the status line (usability §9) | 2026-11-12 |
| 10 | Languages as data, phase 1: `DataLanguage`, `json.yml`, `JSONLanguage` deleted | 2026-11-13 |
| 15 | Mouse bindings: a `mouse:` section in the keymaps (usability §13) | 2026-11-12 |
| - | Minimap contrast: marks gap, damped text, `overlay_strength` (usability §7 follow-up) | 2026-11-13 |
| - | A theme that can't load falls back to the default (open-bugs #29) | 2026-11-13 |
| - | Theme import writes author / license / copyright / source (`theme-collection.md` §3) | 2026-11-13 |
| 42 | Go to line: `.goto <line>` / `.g <line>`, quick mode `g` types `.g ` (a jump - `Cmd+[` comes back) | 2026-11-16 |
| 43 | Line numbers count from 1 in the gutter and the status line (they showed the 0-based index; `.goto`, Find and Go to symbol already counted from 1) | 2026-11-16 |
| 16 | Data loss: a Latin-1 file was cut on open and save - now detected, decoded, saved back as it was (open-bugs #15) | 2026-11-16 |
| 45 | Terminal `Ctrl+C` at the prompt abandons the line (a fresh prompt below, as a shell does; was ignored on Linux, typed `c` in the ANSI backend); gansi asks xterm for `metaSendsEscape` so Alt works in uxterm (it set the 8th bit) | 2026-11-17 |
| 11 | CMake highlighting (languages phase 2, step 1): `cmake.yml` for `CMakeLists.txt` (a language file's `filenames:` now register) and `*.cmake`. The engine gained `case_sensitive` (language and entry), `numbers: generic` (`GenericNumberMatcher`), and the `function` / `macro` / `enum_value` classes for a data file - kept by the semantic-names pass when the language's syntax gives them | 2026-11-17 |
| 47 | Promote to terminal: `Cmd/Ctrl+Enter` (`PromoteToTerminal`) sends the editor's selection - or the line, with none - to the terminal, not via the clipboard, and Enter after it (`terminal.promote_sends_enter`, default yes). An app gets a paste (bracketed when it asked) + `\r`; one line at the local prompt runs as typed; several go to readline and run. Focus stays in the editor, the selection stays | 2026-11-17 |
| 44 | xterm Ctrl chords in the ANSI backend: gansi asks for modifyOtherKeys level 2 alongside Kitty (`CSI > 4;2 m`, back to the terminal's own on close with `CSI > 4 m`) and parses `CSI 27;mod;cp ~` - `Ctrl+/`, `Ctrl+I` apart from Tab, `Ctrl+]`, `Ctrl+Shift+<letter>`. Not for Terminal.app or a dumb TERM | 2026-11-17 |
| 49 | SDL with no display fails with a message: a video driver that puts no window on a desktop (kmsdrm, offscreen, dummy - what SDL falls through to with no X/Wayland) is refused unless `SDL_VIDEODRIVER` asks for it (`Core/Graphics/WindowingVideoDriver.h`); `SDL_QUIT` (SIGTERM, window close) goes through `Runloop::StopRunLoop` and main's close instead of `SDL_Quit(); exit(0)`, which ran `~SDLFontManager` on a dead renderer (SIGSEGV). SDL2 + SDL3 | 2026-11-17 |
| 46 | No display, no window: a launch without `--backend` picks the ANSI backend when neither `DISPLAY` nor `WAYLAND_DISPLAY` is set (as before, also for a `tty` session); on macOS when logged in over ssh (`SSH_CONNECTION` / `SSH_TTY`). `--backend` and a desktop launch still win | 2026-11-17 |
| 38 | A git process still running at exit no longer posts into the destroyed Runloop queue: `Process::StopAll` at exit kills and waits (open-bugs #32) | 2026-11-17 |
| 22 | Build warnings: `-Wshadow` ours, gnklog PRIVATE; Mac + Linux clean in our code (open-bugs #19) | 2026-11-16 |
| 39 | Undo back to the loaded/saved text clears the `*` (a hash sweep after a pause, compared with a baseline) | 2026-11-16 |
| 37 | Intermittent test-run segfault: the parse thread walked `lines` while the UI thread changed them (open-bugs #31) | 2026-11-16 |
| 35 | PageDown / Cmd+End in a short file kept a short scroll window, the view scrolled early (open-bugs #30) | 2026-11-15 |
| - | Workspace tree read up front within a budget (`max_scan_entries`); quitting doesn't wait for the index's root walk | 2026-11-15 |
| 21 | The colour script reads `#rgb` / `#rgba` / `#rrggbbaa` (open-bugs #16) | 2026-11-16 |
| 23 | Only `goatedit <one folder>` is a project, other launches warn; a folder opened after a file gets a monitored root (open-bugs #21) | 2026-11-16 |
| 24 | Build and install remove an asset deleted from the repo (open-bugs #22) | 2026-11-16 |
| 29 | The legacy quick-mode view switch (Esc then `]` / `[`) removed | 2026-11-16 |
