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

- **Workspace tree clicks** (#1 / #15, `CommitLine: DoubleClick` in `workspace_keymap.yml`)
  - [ ] Linux [x] Mac - double-click a closed folder: opens; again: closes
  - [ ] Linux [x] Mac - Enter on a folder: toggles
  - [ ] Linux [x] Mac - triple-click a folder: toggles once
  - [ ] Linux [x] Mac - double-click a git-ignored folder (`cmake-build-debug`): its contents show
  - [ ] Linux [x] Mac - double-click a file: opens
  - [ ] Linux [x] Mac - Ctrl/Cmd+click a tree row: nothing happens in the editor (`mouse:` is not inherited)
- **Go-to click** (#2 / #15, `GotoToggle: "CopyPasteModifier + Click"` in `default_keymap.yml`)
  - [ ] Linux [x] Mac - Ctrl/Cmd+click a function used in another file: jumps to its definition
  - [ ] Linux [x] Mac - Ctrl/Cmd+`[`: back
  - [ ] Linux [x] Mac - Ctrl/Cmd+click a declaration in a header: toggles to the definition
  - [ ] Linux [x] Mac - plain click: only moves the caret; Ctrl/Cmd+click on whitespace: nothing
  - [ ] Linux [x] Mac - Ctrl/Cmd+double-click a name: no second jump
  - [ ] Linux [x] Mac - Ctrl/Cmd+Shift+click: no jump (modifiers match exactly now)
- **YAML Enter** (#4), in a `.yml`, Enter at the end of:
  - [ ] Linux [x] Mac - `run: |-` -> next line indented 2; `  - run: >+` -> 6; `run: |  # x` -> 2
  - [ ] Linux [x] Mac - `---` and `version: 1.0-` -> no indent
- **Git branch in the status line** (#5)
  - [x] Linux [x] Mac - `goatedit .` in this repo: `⎇ main` at the right of the status line
  - [ ] Linux [x] Mac - `git switch -c tmp` in the terminal panel: `⎇ tmp` within ~1 s, no keypress
  - [ ] Linux [x] Mac - `git switch --detach`: `⎇ (<sha>)`; `git switch main`: back
  - [ ] Linux [x] Mac - opened on a folder outside any repo: nothing shown
- **Languages as data** (#10) - Linux built + tests green (743), not tried in the app
  - [ ] Linux [x] Mac - a `.json` file highlights as before; `.ll` lists the language files with their paths
  - [ ] Linux [x] Mac - `languages: { default: { tabsize: 8 } }` in the user config reaches json and cpp
- **Theme that can't load** (open-bugs #29 fix) - Linux built + tests green (774)
  - [ ] Linux [x] Mac - `main.theme: nosuch.theme.yml`: starts, stderr says "no such theme file - using default.theme.yml"
- **Minimap contrast** (usability §7 follow-up) - Linux built + tests green (774)
  - [ ] Linux [x] Mac - a changed line's mark stands clear of the text; the viewport box is subtle

## Next - suggested order

| # | Item | Where | Size | Notes |
|---|---|---|---|---|
| 16 | Data loss: a Latin-1 file is truncated on open and save | open-bugs #15 | half a day | the only data-loss bug - first |
| 17 | A config list overwrites the default list by position | open-bugs #28 | half a day | idea: an immutable merge that returns a new tree; `LanguageFileSet::Merge` is the model |
| 11 | Languages phase 2: CMake and Python (case-insensitive lists, generic numbers, the prefix rule) | `support-languages.md` | 1-2 days | used daily |
| 12 | Languages phase 3: the user directory, `.lang_reload`, `override: yes` | `support-languages.md` | a day | user language dirs aren't read yet |
| 18 | Theme collection step 2: `Themes/` folder, lookup order, install rule, default theme moved | `theme-collection.md` §2 | half a day | the same themes on Mac and Linux from git |
| 19 | `.theme list` / `.theme use <name>` | `theme-collection.md` §5 | half a day | after 18 |
| 20 | Ship Dark 2026 (from VS Code) + `Themes/LICENSES.md`, its own terminal palette | `theme-collection.md` §4 | half a day | copyright line from the `microsoft/vscode` repo first |
| 21 | The colour script misreads `#rgb`, can't read `#rrggbbaa` | open-bugs #16 | an hour | before importing more themes |
| 22 | Build warnings, mostly `-Wshadow` | open-bugs #19 | half a day | Linux reported more in `Theme.cpp`, `Editor.cpp` (2026-11-13) |
| 6 | Gansi copy fallback: `pbcopy` / `xclip` / `wl-copy` when local | open-bugs #20 notes | half a day | fits with #20 |
| 23 | A file argument before the folder leaves the folder unmonitored | open-bugs #21 | hours | |
| 24 | Install never removes an asset deleted from the repo | open-bugs #22 | hours | matters once themes move (18) |
| 25 | `Cmd` held highlights the word under the pointer | usability §14 | half a day | note, not scheduled |
| 26 | Button widget in the UI toolkit | usability §16 | a day | needed by 27 |
| 27 | Close button per file in the header (`*` / `x`, CLion style) | usability §15 | half a day | after 26 |
| 8 | Key binding groups: `FunctionModifier`, `CodeNavigationModifier`, `docs/keybindings.md` | usability §11 | days | decide the `CodeNavigationModifier` key first |
| 9 | The terminal version on macOS uses the Linux keymaps | open-bugs #20 | days | after 8 |
| 28 | Key bindings for script commands (`ReformatLine` as a quick command) | usability §4 | ? | note, not scheduled |
| 29 | Remove the legacy quick-mode view switch (Esc then `]` / `[`) | - | hours | Option+T/P/E and mouse focus replaced it |
| 30 | Workspace text search on `Cmd+Shift+F` | usability §3 c | days | design first |
| 7 | Image view phase 0: `BufferBase`, `TextBuffer` / `ImageBuffer` | `image-view.md` §2.4 | a day | parked 2026-11-11 |
| 13 | Image view phases 1-3 | `image-view.md` | days | after 7 |
| 14 | Languages phase 4: every language from a file; `indent.yml` / `autopairs.yml` move in | `support-languages.md` §2.2 | days | |
| 31 | The minimap in the terminal (Gansi graphics: kitty / sixel) | usability §7 step 3 | days | |
| 32 | Terminal: run `claude` inside goatedit (the gvt exit criterion); copying older command output | `terminal-vt.md` "Next" | ? | the copy modal is an idea, not decided |
| 33 | Code index step 4: modules | `code-index.md` | days | paused 2026-11-08 until the design is thought through |
| 34 | Per-block syntax highlighting in the terminal | `syntax-blocks.md` | days | deferred |

Smaller bugs not ranked yet: open-bugs #1 (`AttributeAt`, read before touching it), #24 (readline history extras),
#26 (`/usage` glyphs).
Status to check: `partially_done/folder-monitor.md` says the monitor is disabled, but `config.yml` ships
`foldermonitor.enabled: yes` (`foldermonitor_fixes.md`) - the doc's status is likely stale.

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
