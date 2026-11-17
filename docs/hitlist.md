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

- **Undo clears the `*`** (#39)
  - [ ] Linux [x] Mac - mistype, undo: the `*` goes after a pause; type a char + backspace: same; edit, save, undo: `*` stays
- **Text encodings** (#16, open-bugs #15)
  - [ ] Linux [x] Mac - open a Latin-1 file: å/ä/ö right, status line `cp1252`; edit + save: `git diff` shows only the edit
- **Go to line, line numbers from 1** (#42, #43)
  - [ ] Linux [x] Mac - `Esc g 42 Enter` and `.g 42`: on the line the gutter shows as 42; `Ctrl+[` comes back; status `l: 42`
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
  - [ ] Linux [x] Mac - a `.json` file highlights as before; `.ll` lists the language files with their paths
  - [ ] Linux [x] Mac - `languages: { default: { tabsize: 8 } }` in the user config reaches json and cpp
- **Theme that can't load** (open-bugs #29 fix) - Linux built + tests green (774)
  - [ ] Linux [x] Mac - `main.theme: nosuch.theme.yml`: starts, stderr says "no such theme file - using default.theme.yml"
- **Minimap contrast** (usability §7 follow-up) - Linux built + tests green (774)
  - [ ] Linux [x] Mac - a changed line's mark stands clear of the text; the viewport box is subtle
- **Big folder** (`max_scan_entries`, indexer cancel) - Linux built + tests green, measured by script, not by hand
  - [ ] Linux [ ] Mac - `goatedit .` in a big tree (`~/src/apus`, an npm project with `node_modules` not ignored): the window answers at once
  - [ ] Linux [ ] Mac - folders below the fourth level open when expanded; a session file deep in the tree reopens
  - [ ] Linux [ ] Mac - `.q!` right after start-up: exits at once
- **Single-folder project** (#23) - Mac checked in a pty (`--backend ansi`), not in the SDL app
  - [ ] Linux [ ] Mac - `goatedit a.txt .` in a repo with a `.goatedit`: stderr says "not a single-folder launch - project settings in '<dir>/.goatedit' not used"; no session restored; a file created in the terminal shows in the tree
  - [ ] Linux [ ] Mac - `goatedit .`: the session restores as before, no warning
- **Quick mode without `]` / `[`** (#29)
  - [ ] Linux [ ] Mac - Esc, then `]`: no view switch; `Alt/Option+]` still cycles
- **Stale assets** (#24) - Mac build + scratch install checked
  - [ ] Linux - `scripts/install.sh --user` after a file is removed from `Assets/Resources`: gone from `~/.local/share/goatedit`
- **Terminal Ctrl+C / xterm Alt** (#45)
  - [x] Linux [ ] Mac - terminal: type `echo abc`, `Ctrl+C`: the text stays, a fresh prompt below, nothing runs
  - [x] Linux - `goatedit --backend ansi` in `uxterm`: `Alt+T` / `Alt+E` switch views (no `ô` typed); Alt works in uxterm after quitting as before
- **Short-file scroll window** (#35, open-bugs #30)
  - [ ] Linux [x] Mac - a new file, a dozen lines, PageDown, then Enter in the middle: the top line stays until the view is full

## Next - suggested order

| # | Item | Where | Size | Notes |
|---|---|---|---|---|
| 47 | Promote the selection to the terminal: one key sends the editor's selection to the terminal - not via the clipboard - and presses Enter after it (`terminal.promote_sends_enter`, default yes). For AI chats run in the terminal (`claude`): a prompt written in a scratch document goes to the chat in one key | - | half a day | key not decided (`Cmd/Ctrl+<x>`, functionality group; `Cmd+X` is cut). Ride `TerminalController::Paste`: bracketed for an app that asked (the chat gets it as one paste, then `\r`), the local prompt as today. Decide: no selection = the current line? focus stays in the editor? |
| 38 | A git process thread outlives exit and posts into the destroyed Runloop queue | open-bugs #32 | hours | ASan only, at exit (test teardown; the app on quit with git running); main too |
| 40 | User documentation: the keyboard layout per platform (the shipped keymaps, modifier groups), the shipped dot/quick commands, the mouse bindings | `docs/usage.md` (a short first version, 2026-11-16) | a day | discussed, never written down; generate from the keymap YAML where possible so it can't drift; ties in with #8 |
| 44 | ANSI backend in a plain xterm (no Kitty keyboard): chords the legacy encoding can't carry - `Ctrl+/` (comment), `Ctrl+I` (is Tab: reformat block), `Ctrl+]` (jump forward), every `Ctrl+Shift+<letter>` | `src/ext/gansi` (`InputParser`, `Terminal::Open`) | hours | xterm's modifyOtherKeys (`Caps::EnableModifyOtherKeys` exists, unused) + parse `CSI 27;mod;cp ~`; only where Kitty isn't spoken. Reported by the beta tester (uxterm over ssh) |
| 46 | The ANSI backend is picked automatically only when `XDG_SESSION_TYPE` is `tty` (logind sets it for ssh). Without it (containers, ssh without pam_systemd) the default is SDL, which fails with no display; macOS over ssh always picks SDL | `Editor::ConfigureSubSystems`, `XDGEnvironment::IsTerminalSession` | an hour | fall back to ANSI when neither `DISPLAY` nor `WAYLAND_DISPLAY` is set (macOS: when `SSH_TTY` is set); `--backend` / `main.backend` keep overriding |
| 41 | Gaps found writing `usage.md`: no word left/right on macOS; no drag-to-select in the editor; `.openfolder` is registered in `jsplugs.yml` but `Scripts/openfolder.js` doesn't exist (see #36) | `docs/usage.md` | hours | small, each on its own |
| 17 | A config list overwrites the default list by position | open-bugs #28 | half a day | idea: an immutable merge that returns a new tree; `LanguageFileSet::Merge` is the model |
| 11 | Languages phase 2: CMake and Python (case-insensitive lists, generic numbers, the prefix rule) | `support-languages.md` | 1-2 days | used daily |
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
| 22 | Build warnings: `-Wshadow` ours, gnklog PRIVATE; Mac + Linux clean in our code (open-bugs #19) | 2026-11-16 |
| 39 | Undo back to the loaded/saved text clears the `*` (a hash sweep after a pause, compared with a baseline) | 2026-11-16 |
| 37 | Intermittent test-run segfault: the parse thread walked `lines` while the UI thread changed them (open-bugs #31) | 2026-11-16 |
| 35 | PageDown / Cmd+End in a short file kept a short scroll window, the view scrolled early (open-bugs #30) | 2026-11-15 |
| - | Workspace tree read up front within a budget (`max_scan_entries`); quitting doesn't wait for the index's root walk | 2026-11-15 |
| 21 | The colour script reads `#rgb` / `#rgba` / `#rrggbbaa` (open-bugs #16) | 2026-11-16 |
| 23 | Only `goatedit <one folder>` is a project, other launches warn; a folder opened after a file gets a monitored root (open-bugs #21) | 2026-11-16 |
| 24 | Build and install remove an asset deleted from the repo (open-bugs #22) | 2026-11-16 |
| 29 | The legacy quick-mode view switch (Esc then `]` / `[`) removed | 2026-11-16 |
