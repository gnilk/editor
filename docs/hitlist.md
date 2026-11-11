# Hitlist - what's planned, easiest first

Made 2026-11-11. Planned work from the design docs, ordered from easy to hard. Each item links to where it's described.
When one is done, strike it through here and add a line in `work-log.md`.

## 0. First, on Linux (SDL2) - checks of what is already merged

Nothing has been tried on the Linux box since these landed:
- [ ] Build `goatedit` and run the verified-green set (701) with SDL2.
- [ ] YAML highlighting, Enter auto-indent, `.fmf` in a `.yml` printing "Reformat is off" (`support-yaml.md`).
- [ ] Select all with `Ctrl+A`, then `Ctrl+I`; `Shift+Down` taking in the last line (usability §8, former bug #27).
- [ ] `.fmf` / `.fmb` on a C++ file.
- [ ] The system font fallback on Linux (fontconfig) - still open from `system-font-fallback.md`.

## 1. Small - an hour or two each

| # | Item | Where | Notes |
|---|---|---|---|
| 1 | Double-click (and Enter) on a folder expands or collapses it | usability §10 | `TreeView::ToggleExpanded`; act on `clicks == 2` only |
| 2 | `Cmd+click` (`Ctrl+click`) jumps as `Cmd+B` does | code-index §5.6 | posts `GotoToggle` from `EditorView::OnMousePressedEvent`; the modifier comes from the keymap |
| 3 | Check: does `GotoImplementation` (tagged `Selection` by its binding) start a stray selection? | usability §11 | an investigation; a repro test if it does |
| 4 | YAML: `\|-` / `>+` headers don't auto-indent the next line | `support-yaml.md` | a gap noted at merge |

## 2. Half a day to a day

| # | Item | Where | Notes |
|---|---|---|---|
| 5 | The git branch in the status line: `⎇ main` | usability §9 | `status --branch` on the query that already runs; `IsSameAs` must compare the branch |
| 6 | Gansi copy fallback: `pbcopy` / `xclip` / `wl-copy` when local (Apple Terminal has no OSC 52) | open-bugs #20 notes | small on its own; fits with #20 |
| 7 | Image view phase 0: `BufferBase`, `TextBuffer` / `ImageBuffer`, Document holds one buffer | `image-view.md` §2.4 | no behaviour change for text; the green set + a test per guarded site |

## 3. A few days, decisions needed first

| # | Item | Where | Decide first |
|---|---|---|---|
| 8 | Key binding groups: `FunctionModifier`, `CodeNavigationModifier`, combined modifier values, `docs/keybindings.md` | usability §11 | the `CodeNavigationModifier` key (`Cmd+Shift`, `Ctrl+Option`, or keep `Cmd`) - after listing what macOS, GNOME/KDE and Ghostty take |
| 9 | The terminal version on macOS uses the Linux keymaps | open-bugs #20 | your research on how other tools do it; easier after 8 (four modifier values instead of every binding) |
| 10 | Languages as data, phase 1: `DataLanguage`, the loader, `json.yml` equal to `JSONLanguage`, `.ll` metadata | `support-languages.md` | where the default indent/autopairs live (its §9, question 1) |
| 11 | Languages phase 2: CMake and Python, with case-insensitive lists, the generic number matcher, the prefix rule | `support-languages.md` | - |
| 12 | Languages phase 3: the user directory, `.lang_reload`, `override: yes` | `support-languages.md` | - |

## 4. Large

| # | Item | Where | Notes |
|---|---|---|---|
| 13 | Image view phases 1-3: open and show, zoom and pan, reload/ANSI summary/session | `image-view.md` | after 7; open: unmodified zoom keys, the `file_kinds` home, zoom in the session |
| 14 | Languages phase 4: every language from a file (`engine: builtin` for C++/Markdown/YAML); `indent.yml` / `autopairs.yml` move into the files | `support-languages.md` §2.2 | touches registration, `IndentCache`, autopairs and config |

## Order suggested

0 (on Linux) -> 1, 2, 3, 4 -> 5 -> 10, 11, 12 -> 8 -> 9 -> 7 -> 13 -> 14.

- The languages go before the key bindings: they need no decisions that aren't taken, and CMake/Python are used daily.
- The bindings (8) go before #20 (9), which leans on them.
- Image view waits; it was parked on 2026-11-11.
