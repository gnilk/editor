# gvt fixtures

Byte streams exactly as a program wrote them to its terminal. Copied to `<build>/gvt-fixtures/` by the
`gvt-fixtures` target; tests open them relative to the working directory (run trun from the build dir).

Recorded 2026-11-09 on Linux with `tools/capture.py` (pty, `TERM=xterm-256color`, no `COLORTERM`), in a scratch
folder holding a copy of `src/Parser.cpp`, a `notes.txt` (`hello\nwörld ▛ 😀 中文\n`) and an empty `subdir/`.

| File | Size | Command | Exercises |
|---|---|---|---|
| `vim_edit.bin` | 80x24 | `vim --clean Parser.cpp`, `jjjwwcwRENAMED<Esc>`, `Gdd`, `:q!` | alt screen, scroll region, DSR/DA/OSC 10/11 queries, `CSI >4;2m`, DCS, bracketed paste/focus modes |
| `less_page.bin` | 80x24 | `less Parser.cpp`, space, `/State`, `q` | alt screen, overstrike-free paging, search highlight, BS |
| `git_log_graph.bin` | 100x30 | `git log --graph --color=always --decorate -n 40 ... 7b95bc3` (editor repo) | SGR colours, graph glyphs |
| `ls_color.bin` | 80x24 | `ls --color=always -F . subdir` | `CSI 0m` / `01;34m` |
| `dialog_menu.bin` | 80x24 | `dialog --menu ...` with UTF-8 items, Down Down Enter | DEC Special Graphics (`ESC ( 0`), `CSI d`, mouse modes, wide chars |
| `cat_utf8.bin` | 40x10 | `cat notes.txt` | UTF-8 incl. wide (CJK, emoji) |

Re-record with the same command line (see `tools/capture.py --help`). Check new captures for private data
(paths, user and host names, account details) before committing; prefer hand-built streams when a capture
would need scrubbing. The `claude` capture from `docs/terminal-enhancements.md` §7 is not here for that reason.
