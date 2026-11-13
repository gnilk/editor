# Hitlist - what's planned, easiest first

Made 2026-11-11. Planned work from the design docs, ordered from easy to hard. Each item links to where it's described.
When one is done, strike it through here and add a line in `work-log.md`.

## To try in the app (Mac done 2026-11-12, Linux open)

Built and unit-tested today, not yet tried by hand. Linux = SDL2 (Ctrl), Mac = SDL3 (Cmd) - the Mac needs `git pull` and
a build first. **Mac (SDL3, build 07f682b): all checked and working, 2026-11-12** (728/728 green on macOS).
Tick per platform; strike the hitlist item's "not yet tried" note when both are done.

- **#1 / #15 - workspace tree clicks** (now through `CommitLine: DoubleClick` in `workspace_keymap.yml`)
  - [ ] Linux [x] Mac - double-click a closed folder: opens; again: closes
  - [ ] Linux [x] Mac - Enter on a folder: toggles
  - [ ] Linux [x] Mac - triple-click a folder: toggles once
  - [ ] Linux [x] Mac - double-click a git-ignored folder (`cmake-build-debug`): its contents show
  - [ ] Linux [x] Mac - double-click a file: opens
  - [ ] Linux [x] Mac - Ctrl/Cmd+click a tree row: nothing happens in the editor (`mouse:` is not inherited)
- **#2 / #15 - go-to click** (now `GotoToggle: "CopyPasteModifier + Click"` in `default_keymap.yml`)
  - [ ] Linux [x] Mac - Ctrl/Cmd+click a function used in another file: jumps to its definition
  - [ ] Linux [x] Mac - Ctrl/Cmd+`[`: back
  - [ ] Linux [x] Mac - Ctrl/Cmd+click a declaration in a header: toggles to the definition
  - [ ] Linux [x] Mac - plain click: only moves the caret; Ctrl/Cmd+click on whitespace: nothing
  - [ ] Linux [x] Mac - Ctrl/Cmd+double-click a name: no second jump
  - [ ] Linux [x] Mac - Ctrl/Cmd+Shift+click: no jump (modifiers match exactly now)
- **#4 - YAML Enter**, in a `.yml`, Enter at the end of:
  - [ ] Linux [x] Mac - `run: |-` -> next line indented 2; `  - run: >+` -> 6; `run: |  # x` -> 2
  - [ ] Linux [x] Mac - `---` and `version: 1.0-` -> no indent
- **#5 - git branch in the status line**
  - [x] Linux [x] Mac - `goatedit .` in this repo: `⎇ main` at the right of the status line (Linux seen 2026-11-12)
  - [ ] Linux [x] Mac - `git switch -c tmp` in the terminal panel: `⎇ tmp` within ~1 s, no keypress
  - [ ] Linux [x] Mac - `git switch --detach`: `⎇ (<sha>)`; `git switch main`: back
  - [ ] Linux [x] Mac - opened on a folder outside any repo: nothing shown

**In progress (2026-11-13):** #10 (languages as data, phase 1). §9 question 1 decided: `Languages/default.yml` +
`inherit:`; `config.yml` overrides.

## 0. First, on Linux (SDL2) - checks of what is already merged

Nothing has been tried on the Linux box since these landed:
- [x] Build `goatedit` and run the verified-green set (701) with SDL2. (2026-11-11: 701 run, 0 failed)
- [x] YAML highlighting, Enter auto-indent, `.fmf` in a `.yml` printing "Reformat is off" (`support-yaml.md`).
- [x] Select all with `Ctrl+A`, then `Ctrl+I`; `Shift+Down` taking in the last line (usability §8, former bug #27).
- [x] `.fmf` / `.fmb` on a C++ file.
- [x] The system font fallback on Linux (fontconfig) (`system-font-fallback.md`).

All of section 0 checked on 2026-11-11 (SDL2). The YAML, select all and `.fmf`/`.fmb` checks were also tried over SSH in the ANSI backend.

## 1. Small - an hour or two each

| # | Item | Where | Notes |
|---|---|---|---|
| 1 | ~~Double-click (and Enter) on a folder expands or collapses it~~ | usability §10 | done 2026-11-12; not yet tried in the app |
| 2 | ~~`Cmd+click` (`Ctrl+click`) jumps as `Cmd+B` does~~ | code-index §5.6 | done 2026-11-12; not yet tried in the app |
| 3 | ~~Check: does `GotoImplementation` (tagged `Selection` by its binding) start a stray selection?~~ | usability §11 | checked 2026-11-12: no - letter bindings and required modifiers are never tagged; two guard tests |
| 4 | ~~YAML: `\|-` / `>+` headers don't auto-indent the next line~~ | `support-yaml.md` | done 2026-11-12 (and a header followed by a comment); not yet tried in the app |

## 2. Half a day to a day

| # | Item | Where | Notes |
|---|---|---|---|
| 5 | ~~The git branch in the status line: `⎇ main`~~ | usability §9 | done 2026-11-12; seen in the SDL2 app (`⎇ main`); switch and detached not yet tried in the app |
| 6 | Gansi copy fallback: `pbcopy` / `xclip` / `wl-copy` when local (Apple Terminal has no OSC 52) | open-bugs #20 notes | small on its own; fits with #20 |
| 7 | Image view phase 0: `BufferBase`, `TextBuffer` / `ImageBuffer`, Document holds one buffer | `image-view.md` §2.4 | no behaviour change for text; the green set + a test per guarded site |
| 15 | ~~Mouse bindings: a `mouse:` section in the keymaps (`GotoToggle: CopyPasteModifier + Click`, the tree's `DoubleClick`)~~ | usability §13 | done 2026-11-12; not inherited; not yet tried in the app |

## 3. A few days, decisions needed first

| # | Item | Where | Decide first |
|---|---|---|---|
| 8 | Key binding groups: `FunctionModifier`, `CodeNavigationModifier`, combined modifier values, `docs/keybindings.md` | usability §11 | the `CodeNavigationModifier` key (`Cmd+Shift`, `Ctrl+Option`, or keep `Cmd`) - after listing what macOS, GNOME/KDE and Ghostty take |
| 9 | The terminal version on macOS uses the Linux keymaps | open-bugs #20 | your research on how other tools do it; easier after 8 (four modifier values instead of every binding) |
| 10 | Languages as data, phase 1: `DataLanguage`, the loader, `json.yml` equal to `JSONLanguage`, `.ll` metadata | `support-languages.md` | decided 2026-11-13: `default.yml` + `inherit:` (§2.2); phase 1 built on `feature/languages-as-data`, not yet tried in the app |
| 11 | Languages phase 2: CMake and Python, with case-insensitive lists, the generic number matcher, the prefix rule | `support-languages.md` | - |
| 12 | Languages phase 3: the user directory, `.lang_reload`, `override: yes` | `support-languages.md` | - |

## 4. Large

| # | Item | Where | Notes |
|---|---|---|---|
| 13 | Image view phases 1-3: open and show, zoom and pan, reload/ANSI summary/session | `image-view.md` | after 7; open: unmodified zoom keys, the `file_kinds` home, zoom in the session |
| 14 | Languages phase 4: every language from a file (`engine: builtin` for C++/Markdown/YAML); `indent.yml` / `autopairs.yml` move into the files | `support-languages.md` §2.2 | touches registration, `IndentCache`, autopairs and config |

## Order suggested

0 (on Linux) -> 1, 2, 3, 4 -> 5 -> 15 -> 10, 11, 12 -> 8 -> 9 -> 7 -> 13 -> 14.

- The languages go before the key bindings: they need no decisions that aren't taken, and CMake/Python are used daily.
- The bindings (8) go before #20 (9), which leans on them.
- Image view waits; it was parked on 2026-11-11.
