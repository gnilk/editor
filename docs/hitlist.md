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

- **Build messages** (#52) - the terminal version scripted (`build_messages`: `cc` on a broken file, `F8` / `Shift+F8` / `Ctrl+F8`)
  - [ ] Linux [x] Mac - the window: `make` / `cmake --build cmake-build-debug` with an error in a file - "build: N errors, M warnings" on the status line when it ends; `F8` opens the first error at its line and column ("error 1/N"), again goes on (errors, then warnings), `Shift+F8` back; `Cmd/Ctrl+[` comes back from each
  - [ ] Linux [x] Mac - `Cmd/Ctrl+F8`: the list (the command and the counts in the caption), type to filter, Enter goes there, `F8` goes on from that one
  - [ ] Linux [x] Mac - a long build (this repo, `-j`): `Cmd/Ctrl+F8` while it still runs lists what has come so far ("(running)"); an `ls` after the build keeps the list; the next build replaces it
    - Mac 2026-11-19: `make -j` on testrunner in the panel listed 1 of its warnings - clang wraps messages to the panel's width; fixed (dev 3293995a), every warning listed now, each once though several targets compile the file
  - [ ] Linux [x] Mac - trun: a failing test run in the panel (`trun --sequential ./libutests.so`) - "build: N errors" counts each failure once; `F8` goes to the failed `TR_ASSERT` line, "error 1/N: test_x_y: <exp>" (scripted: `build_messages_trun`)
- **Build messages, step 4** (#52) - scripted: the code under a message (`messages_tab`), the gutter's colour (`python`)
  - [ ] Linux [ ] Mac - the window: the list (`Cmd/Ctrl+F8`, the Messages tab) shows each message's code on the line under it, in its colours
  - [ ] Linux [ ] Mac - the window: after a build, the lines with an error have their number red in the gutter, a warning's orange / yellow
  - [ ] Linux [ ] Mac - the window: `Cmd+click` (Linux `Ctrl+click`) a `file:line` in the terminal - a gcc / clang error, rustc's ` --> `, a Python traceback's `File "x.py", line 5` - opens it there; `Cmd/Ctrl+[` comes back; a plain word does nothing
- **Python** (#11, #52 step 4) - scripted (`python`: colours, `if x:` + Enter indents, `python3 crash.py` -> `F8` to line 2)
  - [ ] Linux [ ] Mac - the window: a `.py` file - keywords, `def` / `class` names, strings (also `f"..."`, `"""` over
    lines), numbers, comments, `@decorator` coloured; Enter after a line ending in `:` indents 4
  - [ ] Linux [ ] Mac - the window: `python3 x.py` that raises in the panel - "build: 1 error"; `F8` goes to the program's own
    line (not a line in Python's library); a SyntaxError too
- **Terminal tabs** (#53) - scripted (`terminal_tabs`: Ctrl+Shift+T / W, Alt+1/2, the job question, a restart)
  - [x] Linux [x] Mac - the window: `Cmd+T` / `Ctrl+Shift+T` in the terminal opens a second tab (the row shows), `Cmd+W` / `Ctrl+Shift+W` closes it; `Cmd/Opt+1`, `2` / `Alt+1`, `2` and a click on a tab switch; a restart brings the tabs back, the same one shown; `make` in one tab, `ls` in the other: `F8` still has the build's errors
- **Configured tabs, an agent's tab** (#53 step 4) - scripted (`agent_tab`: `panel.tabs`, a section inheriting `terminal`, its bootstrap, a project's not run, Cmd/Ctrl+Enter to `promote_target`, kept in the session)
  - [ ] Linux [ ] Mac - the window: `panel.tabs` with Shell and Claude (`ai_agent: { inherit: terminal, bootstrap: ["claude"], promote_target: yes }`, the commented example in `config.yml`) in the user config, a project with no session: both tabs, `claude` running in the second; `Cmd/Ctrl+Enter` on a line in the editor shows the Claude tab with the line in it; a restart: both back
- **Messages tab** (#53 step 5) - scripted (`messages_tab`: the count in the label, Ctrl+F8 to the tab, Down + Enter, F8 on from there, Ctrl+Shift+W closes it - not the editor's file - then Ctrl+F8 is the modal)
  - [ ] Linux [ ] Mac - the window: `- view: messages` in `panel.tabs` (a project with no session), `make` with errors in a Shell tab: 'Messages (N)' while it runs; `Cmd/Ctrl+F8` shows the tab; a double-click on a row goes there; `F8` moves the selection in the tab
- **`.theme use` at a level** (#59) - scripted (`theme_project`: a project config with its own theme, Keep / Remove, `--project`)
  - [ ] Linux [x] Mac - the window: in a project whose `.goatedit/config.yml` sets `main.theme`, `.theme use <other>` asks; Keep leaves this project's theme and says so, Remove takes it out of the project config and switches; `.theme list` names where the theme comes from
- **Text weight** (#54) - `sdl.text_gamma` (default 1.2, the user's pick - VS Code's 1.5 match was too dark; 1.0 = as before)
  - [x] Linux [x] Mac - the window, Dark 2026 or Mariana: text heavier than before, edges not blurred or blocky; `.cfg --user set sdl.text_gamma 1.0` + restart = the old look; side by side with VS Code (Mac 2026-11-19: 1.5 matches VS Code, 1.2 kept as the default)
- **Unbound Ctrl keys go to the shell** (decided 2026-11-19) - scripted: `terminalcontroller ctrl_to_shell`
  - [ ] Linux [ ] Mac - the window, at the local prompt: type `echo one two`, `Left` x4, `Ctrl+W` - the shell deletes `one`; `Ctrl+U` / `Ctrl+K` / `Ctrl+Y` work; `Ctrl+D` on an empty line does nothing (the shell stays); `Ctrl+T` (Linux) still opens Go to symbol
- **Font styles in themes** (#55)
  - [x] Linux [ ] Mac - the window: `.theme use vscode-dark-2026`, open a `.md` file - `# headings`, `**strong**` and `---` bold, `*emphasis*` italic, links underlined; C++ looks as before (no bold names); `.theme use default theme` - code plain, Markdown headings / `**strong**` bold and `*emphasis*` italic (added by hand 2026-11-19)
- **Italic in the SDL window** - a synthetic slant (no italic face; theme font styles are #55)
  - [x] Linux [x] Mac - in the terminal: `printf '\e[3mitalic\e[0m \e[1;3mbold italic\e[0m plain\n'` - the first two lean right, descenders lean left, the last letter of a run isn't cut by the next cell
- **A save key** (#58)
  - [ ] Linux [x] Mac - the window: `Cmd+S` (Mac) / `Ctrl+S` (Linux) saves the active file, the `*` goes; with the terminal or tree focused it still saves the active file; another program rewrote the file: it asks (Overwrite / Reload / Cancel) as `.s` does (scripted: `save_key`, the Linux keymaps in a pty - Mac 2026-11-19 pass)
  - [ ] Linux - `nano` in the terminal panel: `Ctrl+S` is nano's save, not the editor's (an app owns the terminal - the key goes to it)
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
  - [x] Linux [x] Mac - a changed line's mark stands clear of the text; the viewport box is subtle
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
  - [x] Mac - `--backend sdl` over ssh localhost: a window, as asked - the user is logged in at the desktop, so `cocoa` is there. It opens behind everything (in the Dock, works when picked): macOS' cooperative activation refuses focus to an app not started by the frontmost one - the reason `goat` launches through `open`. Not ours to fix. (Started as `/Applications/goatedit.app/Contents/MacOS/goatedit --backend sdl .`; without `--backend` the same start gives the ANSI backend, correctly)
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
- **Shell prompt at start-up** (#51) - checked in a pty on the ANSI backend, not in the SDL app
  - [x] Linux [x] Mac - start the app: the goat banner, then the shell's prompt below it at once, no key needed (scripted: `prompt_above`, `missing_theme`; Linux SDL2 checked too)
  - [x] Linux [x] Mac - type half a command in the terminal, then `.ll` in the editor: the list lands above the line, the half-typed command stays and runs when finished (scripted: `prompt_above`)
  - [x] Linux [x] Mac - the SDL app: same start-up, a coloured `PS1` keeps its colours after the banner
- **A dot-command typed at the terminal prompt** (#57)
  - [x] Linux [x] Mac - `.theme list` at the prompt: the command stays on its prompt line, the list right below it, then the next prompt - no blank rows; `.co` after `make` + a dot-command still copies make's output (scripted: `dot_at_prompt`, bash and zsh; unit `dotcommand_output_below`, `dotcommand_keeps_output`)
  - [ ] Linux [x] Mac - the window version, your own shell and prompt (the scripted run used a test prompt in a pty): type `.theme list` + Enter in the terminal panel - `you@host dir % .theme list` stays on its line, the list right below it, then ONE new prompt; no blank line, no empty prompt line (before: the command was gone and the list sat above the prompt, `screenshots/prompt_and_theme_problem.png`)
  - [x] Linux [x] Mac - quit with bash 5 as the shell (`SHELL=/opt/homebrew/bin/bash` on the Mac): the editor exits at once (scripted: `dot_at_prompt` quits; unit `process_shellclose_output_on_hup`)
    - Linux 2026-11-19: 865/865, checks 23/23 (bash 5.2; zsh not installed there, skipped) - every check quit clean
- **Start-up notices in the terminal panel** (#50)
  - [x] Linux [x] Mac - `goatedit a.txt .` in a repo with a `.goatedit`: "goatedit: not a single-folder launch ..." in the terminal panel, the prompt below it (scripted: `single_folder`)
  - [x] Linux [x] Mac - `main.theme: nosuch.theme.yml`: "goatedit: theme nosuch.theme.yml: no such theme file ..." in the panel (scripted: `missing_theme`)
  - [x] Mac - through `goat` (started with `open`, no stderr): the same notice in the panel - the case #50 was for
- **Terminal version on macOS with the Linux keymaps** (#48) - the scripted checks pass in a pty (all 14 that run on the Mac); the terminals themselves need a hand
  - [x] Mac - Terminal.app, `goatedit --backend ansi`: `Ctrl+S` saves, `Ctrl+Z` undoes, `Ctrl+F` finds (`Ctrl+[` is `Esc` there - a legacy collision); with "Use Option as Meta key" on: `Option+T` / `Option+E` switch views
    Mac [Gnilk 2026-11-19]: CTRL+S was never bound to save - right, no keymap binds a save (only `.s` and autosave): row #58
  - [x] Mac - Ghostty (kitty protocol): the same, plus `Ctrl+/` comments, `Ctrl+Enter` promotes to the terminal, `Ctrl+[` comes back after a jump
    - Mac 2026-11-18, Ghostty: `Ctrl+<letter>` and `Alt+1..9` work; `Cmd+V` pastes (Ghostty's own paste, arrives as a bracketed paste); `Cmd+Z` does nothing - right, `Cmd` is not bound in the Linux keymaps. `Ctrl+Enter` promotes to the terminal (`Cmd+Enter` is Ghostty's full-screen); several selected lines ran in bash. Start with `goat --backend ansi .` (runs in the terminal since `fix/goat-ansi`; needs `install-cli`)
  - [x] Mac - the SDL app is unchanged: `Cmd+S`, `Cmd+Z`, `Cmd+1..9`
  - [x] Mac - several lines `Ctrl+Enter`-promoted to bash 3.2 in the terminal version: both run, no `00~` (scripted: `promote`); by hand in Ghostty with `Ctrl+Enter` too; the SDL app with `Cmd+Enter` and `SHELL=/bin/bash`: [x] (2026-11-19)
- **Usability gaps** (#41) - the terminal version scripted (`drag_select`); Mac window checked by hand 2026-11-18
  - [ ] Linux [x] Mac - drag over text selects it, from where it was pressed (backwards too); dragging below the view scrolls it; a click drops the selection; `Shift+click` extends it
  - [ ] Linux [x] Mac - `Cmd/Ctrl+click` still jumps (no selection), double-click in the tree still opens
  - [x] Mac - `Opt+Left`/`Right` move by word, `Opt+Shift+Left`/`Right` select; `Opt+[`/`]` still cycle views
  - [x] Linux - `Tab` in the project tree goes to the next view; `Ctrl+Shift+Left`/`Right` select by word; `Alt+Left`/`Right` still cycle buffers (2026-11-18, the terminal version in a pty; also `Alt+Home`/`End`, `Alt+T`/`E` unchanged)
- **Themes folder** (#18)
  - [ ] Linux [x] Mac - after installing: starts with the default theme as before; `.theme --import <a VS Code theme>` lands in `~/.config/goatedit/themes/`, `main.theme: <it>.theme.yml` uses it; a theme imported before (in `~/.config/goatedit/` itself) still loads
    - Linux 2026-11-18: `install.sh --user` (sandbox HOME) installs `Themes/`, a reinstall over a pre-#18 install removes the old root `default.theme.yml` / `colors.json`; the installed binary starts with the default theme, no warning. The import part not tried by hand yet
- **Switch themes** (#19) - the terminal version scripted (`theme_use`)
  - [ ] Linux [x] Mac - the window: `.theme list`, `.theme use <one>` - editor, gutter, status line, tree, terminal and minimap all change at once; `.theme use default theme` back; restart keeps the choice
- **Dark 2026 (from VS Code)** (#20) - the terminal version scripted (`vscode_theme`: editor background, terminal red)
  - [ ] Linux [x] Mac - the window: `.theme use vscode-dark-2026` - looks like VS Code's Dark 2026 side by side (a C++ file: keywords, strings, comments, types); the terminal's `ls --color` / `git diff` colours read well
- **Short-file scroll window** (#35, open-bugs #30)
  - [x] Linux [x] Mac - a new file, a dozen lines, PageDown, then Enter in the middle: the top line stays until the view is full (scripted: `short_scroll`)

## Next - suggested order

### The agentic baseline (reprioritised 2026-11-21, again after the tool pattern)

**The next couple of weeks (decided 2026-11-21): 1) the UI change - #77 (layouts: a right panel for the agent tab;
`usability-round.md` §19), 2) the integrated agent - #78 (`agent-integration.md`, `project-profile.md`), 3) the tools -
#76 and the rows below.** Unaware tools and the integrated agent both. #77 later: the Mac moves between a laptop screen and
a large monitor - the same layout in both places may not be wanted (`auto` by window shape may cover it; cross that bridge
then).

What the agentic features (`agentic-features.md`: graphs, type-diff, regular diff) stand on - the analysis is its "Baseline"
section. **Exploring, through separate tools** (its "Status" section): the editor writes a readable X, a tool (Python first)
makes Z, the editor shows Z. So the order is:
1. **The export** (#74) - the X.
2. **Tools over it** - type-diff (#69), extension points (#71), a module roll-up to try modules (#33) - Python, outside the
   editor; each moves into C++ once its shape has settled.
3. **The image view as the first graph view** (#7, #65, #66, #13) - a tool's Graphviz PNG, shown and refreshed; then running a
   tool from the editor (#75).
4. **The live diff** (#67, #68) - native: a tool can't see unsaved edits.
5. **Native views** (#70, #72, #73) once the explorations show what they should be.

Code index steps 5 and 7 are not on the path; step 6 (calls / refs) only for call graphs, later.

| # | Item | Where | Size | Notes |
|---|---|---|---|---|
| 77 | Portrait and landscape layouts: `main.layout: portrait \| landscape \| auto`. Portrait = today (one bottom panel, all tabs). Landscape = a full-height right panel for `panel_right`'s tabs (the agent - long scrollback, `scrollback_lines` per tab works already) next to the bottom one (the build). The status line stays on the splitter. `auto` by the window's shape, later (tab views moved between panels live - `TabbedView` needs a take-without-delete). Promote / Messages / focus / session learn of two panels | usability §19 | a day or two (+ `auto`) | idea 2026-11-21 (the shapes are the user's); a focus key for the right panel to pick |
| 78 | Agent integration: Unaware / Integrated (the editor's own question agent in its own tab) per project, a guided setup in modals (the agent surveys, proposes a manifest of where the truth lives, the user approves), channels (IDE lock file, an MCP server, the PTY, headless), consent - setup always asks, after it the user's settings - a question agent in its own tab, a shadow repo per agent turn | `agent-integration.md`, `project-profile.md` | ? | design, exploring - open: where the meta lives (sidecar repo?), AGENTS.md vs MCP instructions. Its settings: a project profile in `.goatedit/` (trust / description / editor settings), trust approved by hash outside the folder |
| 76 | Questions about code, probes first: one rough script per kind of question (why, how invoked, tested, changed), run on a real change, judged by "did it tell me something new?" - only then a feature. 3 done on the Panel refactor (type-diff, no test names it, why) | `agentic-questions.md` | ongoing | **before the rows below** - they wait for what the probes show |
| 74 | A readable export of the index - the "X" every tool reads: symbols and edges per file / tree, one fact per line (JSON lines or similar), sorted so two runs diff cleanly, `--at <rev>` scans a revision's text (`git show`). `gedit-bench --dump` is the start (aligned columns for a person today). The bench should also fail with a message, not a segfault, when its assets are missing (seen with a stale build) | `agentic-features.md` Status, `utests/bench/codeindex_bench.cpp` | half a day | first - nothing else on the tool path starts without it |
| 69 | Structure diff as data (type-diff): the changed files scanned at the base (`Scan(text)` on `git show`) and now, symbols and edges (`inherits`, `holds` + ownership, `includes`) compared by qualified name; touched types from the hunks by extents. A dot-command + `gedit-bench --typediff <rev>` so an agent can ask | `agentic-features.md` Type-diff, `code-index.md` §10.3 | a day or two | **A Python tool over #74 first**, C++ once its shape has settled. Base: the branch point (`merge-base`) - reviewing a change in progress (2026-11-21) |
| 71 | Extension points: a type with two or more derived types that someone holds (directly or in a container); the scanner records pure virtuals (`= 0`). Marked in the relationship trees and in a type-diff (added / removed) | `agentic-features.md` Graphs | half a day | a Python tool over #74 first; the `= 0` flag goes into the scanner (and the export) |
| 33 | Code index step 4: modules | `code-index.md` | days | paused 2026-11-08 until the design is thought through. On the agentic path (2026-11-21): a type-diff's "new dependency between modules" needs it. Prototype the roll-up as a Python tool over #74 first - try modules before designing step 4 in C++ |
| 7 | Image view phase 0: `BufferBase`, `TextBuffer` / `ImageBuffer` | `image-view.md` §2.4 | a day | parked 2026-11-11 |
| 65 | The view container maps mode -> view (`std::map<DocumentViewMode, ViewBase *>` replaces primary / alternate); Text and Hex move onto it. Split out of #13 - every new mode (Image, Diff, maybe Graph) needs it | `image-view.md` §2.5, `agentic-features.md` Baseline | hours | |
| 66 | Pixel drawing in `DrawContext`: `DrawImagePixels` (source crop, pixel destination, scale mode) + a texture per image (image-view §3), and filled rect / line / polygon and text at a pixel position. SDL2 + SDL3; ANSI no-ops, each view has a character fallback | `image-view.md` §3, `agentic-features.md` Baseline | a day+ | |
| 13 | Image view phases 1-3 | `image-view.md` | days | after 7, #65, #66. Phases 1-2 and phase 3's re-decode on change on disk are on the agentic path: the image view is the **first graph view** - a tool's Graphviz PNG, refreshed when the tool rewrites it. The rest of phase 3 is not |
| 75 | Run a tool from the editor: a dot-command runs a configured tool on the export and opens what it writes (a PNG in the image view, a text report as a document); the view refreshes when the tool rewrites it. `Process` exists. Until then: run it in the terminal, open the result by hand | `agentic-features.md` Status | half a day | after 74, #13 |
| 67 | Revisions and a line diff: a file's text at any revision (`GitBase`, today HEAD only), the files changed between two revisions / the work tree, a read-only TextBuffer from a text, and an aligning line diff (Myers / histogram) over the line hashes - the right side is the buffer, unsaved edits included | `agentic-features.md` Baseline | a day | decided 2026-11-21: both - own diff for the live buffer, `git diff` parsed for history; one hunks / aligned-rows model the Diff view reads |
| 68 | Diff view mode: side by side, gap rows where the other side added (`DiffProjection`), scrolled together, both sides coloured, changes within a line, the joining shapes between the panes (#66; ANSI: coloured bands), a hunk with a few lines around, next / previous hunk | `agentic-features.md` Regular diff | days | after 65, 66, 67. Any two revisions: HEAD vs a point X, X stepped towards HEAD to watch a section evolve; the default base deferred |
| 70 | Type-diff view: #69 as a tree - added / removed / changed, a changed type expands to its holders and derived types (step 3 queries), rows jump | `agentic-features.md` Type-diff | days | after 69 (#33 for module lines). Waits for the tool's text report, opened as a document, to show what the view should be |
| 72 | Graph view: a layered layout (pure, headless tests) of a type's or module's neighbourhood (inherits, holds, module deps), drawn with #66, zoom / pan from the image viewport (#13 phase 2), nodes jump, `\|+\|` folds out an interface's members. D11 revised: drawn first | `agentic-features.md` Graphs, Decisions | days | **Waits for the PNG version** (#13 + #75 + Graphviz) to show what we want. | decided 2026-11-21: a generated document in the editor area; the layout a library + wrapper binary (own layered layout, grid-snapped; Graphviz `dot -Tplain` optional - look at OSS first). ANSI boxes / lines second |
| 73 | Graph diff: #72 over #69's before / after, what was added / removed coloured | `agentic-features.md` Type-diff | days | after 69, 72; a Graphviz PNG with colours first |

### Other - not on that path

Still wanted; done between the baseline steps as they come up. #53's remaining steps, #17 / #62 (bugs and a blocker) first.

| # | Item | Where | Size | Notes |
|---|---|---|---|---|
| 53 | Tabs in the lower panel (CLion style): a generic `TabbedView` in the UI toolkit first, then several terminals (one for an AI agent - the `PromoteToTerminal` target), a Messages tab (#52), a CMake / build tab | `panel-tabs.md` | days | steps 1-3 built 2026-11-19: `TabbedView`, the lower panel is one, several terminals (`Cmd+T`/`Cmd+W`, Linux `Ctrl+Shift+T`/`W`; tabs kept across a restart). step 4 built 2026-11-20 (configured tabs, the AI terminal - `panel.tabs`, `inherit:` for config sections, settings handed over, `PanelView`, `promote_target`). step 5 Messages tab built 2026-11-20 (`view: messages`). Next: a CMake / build tab, 6 close buttons (#26). Idea 2026-11-18; decisions in §6 (one container replaces "the terminal", the `SwitchToBuffer` keys (Mac `Cmd/Opt+1..9`, Linux `Alt+1..9`) per focused container, default tabs from config, tab row hidden with one tab); close buttons need #26 |
| 17 | A config list overwrites the default list by position | open-bugs #28 | half a day | idea: an immutable merge that returns a new tree; `LanguageFileSet::Merge` is the model. **Blocks #61** (a project's `panel.tabs`, a user's `bootstrap:` over a shipped one) |
| 62 | `ConfigFilePatcher` (`.cfg`) appends to / removes from a list in place, comments kept - today it writes one scalar | `ConfigFilePatcher` | half a day | **blocks #61** (`.panel add` appends to `panel.tabs`) |
| 61 | Ready-made panel tabs: sections (`claude:`, `codex:`, ... with a `label`) ship in the default config; `.panel add / del / list [--project] <section>` adds / removes `{ name, config }` in `panel.tabs` and opens / closes the tab now | usability §17 | a day | idea 2026-11-20; **after #17 and #62** |
| 56 | `--geometry WxH[+X+Y]` (X11 style) for the window version: size, and position if given - wins over the session's saved geometry. For test windows and screenshots (they open at the display's default size), and anyone who wants a fixed start | `Editor` args -> `ScreenBase::SetRequestedWindowGeometry` (the session already feeds it, `Editor.cpp` ~1226) | hours | asked 2026-11-18; not saved back into the session? (decide) |
| 60 | The minimap follows soft wrap: one minimap line per wrapped row, as VS Code draws it (today a wrapped paragraph is one long dense line - VS Code's map of a Markdown file is ~25% longer and airier, `screenshots/Minimap_Editor_vs_VSCode.png`). `OverviewMap` / `Minimap::Render` over the `WrapProjection` rows; the viewport box and clicks in rows | `Minimap`, `OverviewView`, `WrapProjection` | half a day | found 2026-11-19 |
| 63 | Build errors / warnings as marks in the minimap (`OverviewMap` gets the kinds, from `BuildMessages::LineMarks` as the gutter). Low - the Messages tab is what is used | `usability-round.md` §18 | hours | note only 2026-11-20 |
| 64 | Terminal: copy an older command's output (split from #32) - `.co` copies only the last *shell* command's, never a dot-command's (2026-11-19: a `.theme list` result could only be shared as a screenshot) | `terminal-vt.md` "Next" | ? | idea, not decided - a copy modal? |
| 12 | Languages phase 3: the user directory, `.lang_reload`, `override: yes` | `support-languages.md` | a day | user language dirs aren't read yet |
| 6 | Gansi copy fallback: `pbcopy` / `xclip` / `wl-copy` when local | open-bugs #20 notes | half a day | fits with #20 |
| 25 | `Cmd` held underlines a name a click would jump from | usability §14 | half a day | decided: only names the index resolves, an underline; needs the hover plumbing (as 26, 27) |
| 26 | Button widget in the UI toolkit | usability §16 | a day | needed by 27 |
| 27 | Close button per file in the header (`*` / `x`, CLion style) | usability §15 | half a day | after 26 |
| 8 | Key binding groups: `FunctionModifier`, `CodeNavigationModifier`, `docs/keybindings.md` | usability §11 | days | decide the `CodeNavigationModifier` key first |
| 28 | Key bindings for script commands (`ReformatLine` as a quick command) | usability §4 | ? | note, not scheduled |
| 30 | Workspace text search on `Cmd+Shift+F` | usability §3 c | days | design first |
| 14 | Languages phase 4: every language from a file; `indent.yml` / `autopairs.yml` move in | `support-languages.md` §2.2 | days | |
| 31 | The minimap in the terminal (Gansi graphics: kitty / sixel) | usability §7 step 3 | days | |
| 34 | Per-block syntax highlighting in the terminal | `syntax-blocks.md` | days | deferred |
| 36 | An "open folder" command (a dot-command and/or a key) - open a folder as the project after start-up | - | ? | no command for it yet; since #23 only `goatedit <one folder>` is a project, so this is the other way in. Design: what happens to the open files and the session |

Smaller bugs not ranked yet: open-bugs #24 (readline history extras),
#26 (`/usage` glyphs).

## Done

| # | Item | When |
|---|---|---|
| 79 | Quit or a closed terminal tab no longer hangs for good: the pty master is close-on-exec (later shells held earlier tabs' masters), the child closes inherited fds, the foreground job gets the hangup, the reap is bounded (open-bugs #36, `fixed-bugs.md`) | 2026-11-24 |
| 32 | ~~Terminal: run `claude` inside goatedit (the gvt exit criterion)~~ - works; closed by the user | 2026-11-20 |
| 52 | ~~Build messages~~ - errors / warnings read from a build in the terminal as it runs (gcc / clang, CMake, make, trun, python, rustc / cargo, ld / lld): `F8` / `Shift+F8`, the `Cmd/Ctrl+F8` list or the Messages tab with the source line under each message, the line's number coloured in the gutter, `Cmd/Ctrl+click` a `file:line` in the terminal, `Terminal.GetMessages()` | 2026-11-20 |
| 11 | ~~Languages phase 2, step 2: Python~~ - `python.yml` (`.py .pyw .pyi`), the tokenizer's prefix rule (`@decorator`), `def` / `class` names, every string prefix, `:` indents; a lone `\` in a string no longer stops the line (also `cmake.yml`). With #52's Python matcher: `python3 x.py` in the panel, `F8` to the line that raised | 2026-11-20 |
| 54 | Text weight: `GlyphFont::CoverageCurve` (alpha = coverage^(1/gamma)) applied when a glyph page is uploaded (both SDL backends, nothing per frame); `sdl.text_gamma`, default 1.2 (1.5 matches VS Code - the user prefers lighter) | 2026-11-19 |
| 55 | Font styles in themes: `font_styles` in colors.json (bold / italic / underline / strikethrough by content key), a token takes its class's style with its colour; the importer resolves VS Code's `fontStyle` / Sublime's `font_style` on its own (never from a context rule); Dark 2026 regenerated - bold headings, italic emphasis (`theme-collection.md` §6). The SDL window draws italic as a slant (same day) | 2026-11-19 |
| 59 | `.theme use [--user \| --project] <name>` (`--user` the default): the theme follows the config that wins. `--user` in a project whose config sets its own `main.theme` asks - remove it from the project (`ConfigFilePatcher::RemoveScalar`, comments kept), keep the project's (the choice is saved for other projects, this one stays and says so), or cancel. `.theme list` says where `main.theme` comes from. With it: `.cfg --user\|--project unset <key>` | 2026-11-19 |
| 58 | A save key: `Cmd+S` (Mac window) / `Ctrl+S` (Linux, the terminal version) - a `Save` action in `default_keymap.yml`, handled globally (any view) by running the `.s` command (`PluginExecutor::Execute`), so a file changed on disk still asks. In the terminal an app (nano) still gets the key | 2026-11-19 |
| 57 | A dot-command typed at the terminal prompt reads like a shell command: the command stays on its prompt line, the output below it, then the next prompt (since #51 the output went above the prompt it was typed at, and the command was gone). `.co` still copies the shell command's output before it (`TerminalScreen::MarkOutputEnd`) | 2026-11-19 |
| - | Quit hung with bash 5 as the shell (Mac): bash writes on the hangup and exits, the reader no longer reads the pty, and macOS keeps the exiting shell until its output drains - `SIGKILL` doesn't help. `Shell::ReapChild` closes our end of the pty first | 2026-11-19 |
| 20 | Dark 2026 (from VS Code) shipped: VS Code's theme files kept as source (`Assets/ThemeSources/vscode/`, MIT, commit pinned), converted by the editor's own importer + an overlay (our name, VS Code's terminal colours) with `scripts/gen-themes.py`, the result committed; `Themes/LICENSES.md`. `--check` is the `themes_gen` check | 2026-11-18 |
| 19 | `.theme list` (yours and the shipped ones, the current one marked) and `.theme use <name>` - switches live (the open documents are highlighted again, the terminal's palette follows) and saves `main.theme` in the user config. `.theme_reload` now updates the syntax colours too | 2026-11-18 |
| 18 | Themes in a folder of their own: shipped ones in `Themes/` (the default moved there, `default.colors.json`), the user's in `~/.config/goatedit/themes/` (the importer writes there); themes imported before still load (`Theme::Locate`) | 2026-11-18 |
| 41 | Gaps from `usage.md`: word left/right on macOS (`Opt+Left`/`Right`, CLion's; `Shift` extends, both platforms); drag to select in the editor (and `Shift+click`; the terminal version too); `Tab` out of the Linux project tree; `.openfolder` unregistered (comes with #36). Found with it: Alt wasn't matched on special keys (`Opt+Left` took `NavigateLeft`) | 2026-11-18 |
| - | `Line::AttributeAt` gave the first span for any position in the last one (open-bugs #1); word left from a line's last token went to column 0. Word left/right step between span starts now | 2026-11-18 |
| 40 | User documentation: `docs/keys.md`, every binding (per keymap, both platforms), mouse binding and command, generated from the shipped keymaps and `jsplugs.yml` by `scripts/gen-keys.py` (`--check`; the `keys_doc` check) - it can't drift. `usage.md` stays the short guide, links to it. `hexview_keymap` named in `config.yml` (it fell back to the default keymap by accident) | 2026-11-18 |
| 48 | ANSI backend on macOS takes the Linux keymaps (`Ctrl` for functionality, `Alt` for the UI): a terminal never sends `Cmd`. `KeyMapping::SetConfigSection`, set by `Editor` with the backend. Also #9 (the same change). Found with it: several lines promoted to a shell that never asked for bracketed paste (macOS bash 3.2) got the brackets as text (`00~`) - now the modes the shell asked for | 2026-11-18 |
| 50 | Start-up notices (a project's settings not used, a theme or language file that can't load, a missing file argument, the old user dir, a cwd outside home) go to the terminal panel too, after the banner - stderr is lost through `goat` / a launcher (`StartupNotices`, `Editor::Notice`) | 2026-11-18 |
| 51 | Console output (the start-up banner, plugins) goes above the shell's prompt, which is drawn again below it - on the Mac bash's prompt arrived before the banner and was pushed off the panel until a key (`TerminalScreen::WriteAboveCursorLine`, gvt `EncodeRow`) | 2026-11-18 |
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
