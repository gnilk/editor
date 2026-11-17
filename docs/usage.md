# Using GoatEdit - the short version

Keys are written for macOS / Linux: `Cmd` on macOS is `Ctrl` on Linux; `Opt` (Option) on macOS is `Alt` on Linux.
Rule of thumb: **`Cmd`/`Ctrl` = editing functions, `Opt`/`Alt` = moving around the UI** (views, sizes, panels).

## Starting

- `goatedit <folder>` - opens the folder as a **project** (its `.goatedit/` config + session are used).
- `goatedit <file>...` / `goatedit <file> <folder>` - opens them, but **not** as a project (a warning says so if a folder has a `.goatedit/`).
- `--skip-user-config` - start without the user and project config.

## In a terminal (ANSI backend)

GoatEdit runs inside a terminal when there is no display (Linux without `DISPLAY` / `WAYLAND_DISPLAY`, macOS over
ssh) or with `--backend ansi`. The keys are the same, but the terminal decides what a key sends:

- **`Opt`/`Alt` must reach GoatEdit as Alt (Meta).** On macOS, Option types characters by default (`Opt+E` is `´`,
  `Opt+T` is `†`) and no view switching works until the terminal is told otherwise:
  - **Ghostty:** `macos-option-as-alt = left` in `~/.config/ghostty/config` (left Option is Alt, right Option still
    types `é`, `[`, `{` - as in the window version); `true` for both.
  - **Terminal.app:** Settings -> Profiles -> Keyboard -> "Use Option as Meta key".
  - **iTerm2:** Settings -> Profiles -> Keys -> Left Option key: `Esc+`.
  - **xterm / uxterm:** nothing to do - GoatEdit turns on `metaSendsEscape` while it runs.
  - **Any other terminal:** look for "Option as Alt", "Option as Meta" or "Meta sends Escape" in its settings.
- **`Cmd` doesn't reach a terminal app** on macOS, so the `Cmd` shortcuts (copy/paste, undo, save, ...) can't be
  pressed in the ANSI backend yet (hitlist #48). Use the `.` commands meanwhile.
- **To see what your terminal sends:** `python3 scripts/keycapture.py` (from the source tree) prints the bytes per
  key with the keyboard modes GoatEdit turns on. `Opt+E` should show as `\x1be` or `\x1b[101;3u`, not `é`.

## Configuration

Three layers, merged in this order (later wins, key by key):

1. **System** - `config.yml` shipped with the app (`resources/`, `goatedit.app/Contents/SharedSupport/` or `share/goatedit/`).
2. **User** - `~/.config/goatedit/config.yml` (both platforms; the old `~/.goatedit/` is still read).
3. **Project** - `<project>/.goatedit/config.yml` - keys that run code (shell, bootstrap scripts, plugin dir, log file) are ignored here.

- `.cfg list [prefix]` / `.cfg get <key>` / `.cfg --user set <key> <value>` / `.cfg --user edit` - read/change it from inside (comments are kept).
- Note: a **list** in your config currently overrides the default list entry by entry, not as a whole (open-bugs #28).
- Keymaps are files named in the `macos:` / `linux:` sections of `config.yml` (e.g. `default_keymap: macOS/default_keymap.yml`); to change bindings, copy one into `~/.config/goatedit/`, give it a new name and point that key at it.
- Text encoding: files that aren't UTF-8 are read as `main.encoding_fallback` (cp1252) and saved back the same way (`main.convert_encoding: keep`); `.encoding` shows/changes it.

## Keys - editor

| Action | macOS | Linux |
|---|---|---|
| Copy / cut / paste / select all | `Cmd+C` / `X` / `V` / `A` | `Ctrl+C` / `X` / `V` / `A` |
| Undo | `Cmd+Z` | `Ctrl+Z` |
| Select | `Shift` + any movement | same |
| Line start / end | `Home`/`End`, `Cmd+Left`/`Right` | `Home`/`End` |
| Word left / right | (not bound yet) | `Ctrl+Left`/`Right` |
| Page up / down | `PgUp`/`PgDn`, `Cmd+Up`/`Down` | `PgUp`/`PgDn` |
| First / last line | `Cmd+Home` / `Cmd+End` | `Alt+Home` / `Alt+End` |
| Indent / unindent | `Tab` / `Shift+Tab` | same |
| Comment line | `Cmd+/` | `Ctrl+/` |
| Reformat line / block | `Cmd+L` / `Cmd+I` | `Ctrl+L` / `Ctrl+I` |
| Find; next / previous hit | `Cmd+F`; `Cmd+G` / `Cmd+Shift+G` | `Ctrl+F`; `Ctrl+G` / `Ctrl+Shift+G` |
| Quick open file | `Cmd+P` | `Ctrl+P` |
| Open files list | `Cmd+E` | `Ctrl+E` |
| Go to symbol | `Cmd+T` | `Ctrl+T` |
| Go to definition (toggle back) | `Cmd+B` | `Ctrl+B` |
| Go to implementation / base / type | `Cmd+Shift+B` / `U` / `I` | `Ctrl+Shift+B` / `U` / `I` |
| Relationships (bases, derived, ...) | `Cmd+Shift+T` | `Ctrl+Shift+T` |
| Jump back / forward | `Cmd+[` / `Cmd+]` | `Ctrl+[` / `Ctrl+]` |
| Buffer 1-9 (9 = last) | `Cmd+1..9` or `Opt+1..9` | `Alt+1..9` |
| Next / previous buffer | `Opt+\` / `Opt+'` | `Alt+\` / `Alt+'`, `Alt+Right`/`Left` |
| Quick mode (see below) | `Esc` | `Esc` |

## Keys - views and layout

| Action | macOS | Linux |
|---|---|---|
| Focus terminal / editor / project tree | `Opt+T` / `Opt+E` / `Opt+P` | `Alt+T` / `Alt+E` / `Alt+P` |
| Next / previous view | `Opt+]` / `Opt+[` | `Alt+]` / `Alt+[` |
| Next editor | `F3` | `F3` |
| View wider / narrower | `Opt+D` / `Opt+A` | `Alt+D` / `Alt+A` |
| View taller / shorter / maximise height | `Opt+S` / `Opt+W` / `Opt+M` | `Alt+S` / `Alt+W` / `Alt+M` |
| Text / hex view of the file | `Opt+R` / `Opt+H` | `Alt+R` / `Alt+H` |
| Project tree: next view, show ignored files | `Tab`, `Opt+H` | `Tab`, `Alt+H` |
| Dialogs (find, open, ...): close / pick | `Esc` / `Enter` | same |

## Mouse

- Click places the caret and focuses the view; the wheel scrolls; drag a divider to resize views. (No drag-to-select yet - use `Shift`+keys.)
- `Cmd+click` (`Ctrl+click`) on a name - go to its definition (as `Cmd+B`).
- Double-click a row in the project tree - open it.
- Mouse bindings live in the `mouse:` section of each keymap file.

## Quick mode (`Esc`)

`Esc` opens a one-line prompt at the bottom; `Esc` again leaves it.

| Key | Does |
|---|---|
| `/text` | search as you type |
| `?` | repeat the last search |
| `n` / `p` | next / previous hit |
| `g` | types `.g ` for you: add a line number, `Enter` |
| `\` / `'` | next / previous buffer |
| `.command` + `Enter` | run a command (below) |

## Commands (`.name` or `.short`)

Typed in quick mode (`Esc`), **or in the terminal** - a terminal line that starts with `.` and names a command runs it
in the editor; anything else (`./build.sh`) goes to the shell. The prefix is `main.cmdlet_prefix`.

| Command | Short | Does |
|---|---|---|
| `.goto <line>` | `.g` | go to a line (counts from 1, lands on its first non-blank; `Cmd+[`/`Ctrl+[` comes back) |
| `.save` / `.saveas <file>` | `.s` / `.sa` | save (asks if the file changed on disk; `.s!` saves anyway) |
| `.openfile <file>` | `.o` | open a file |
| `.newbuffer` | `.nb` | new buffer |
| `.reload` | `.rl` | reload from disk (`!` drops unsaved changes) |
| `.closeactive` / `.closeall` | `.cb` / `.ca` | close buffer(s) (`!` = without asking) |
| `.listbuffers` | `.lb` | list open buffers |
| `.quit` | `.q` | quit (`.q!` quits with unsaved changes) |
| `.encoding [name]` | `.enc` | show / set the file's encoding (`utf-8`, `cp1252`, `latin1`, ...) |
| `.setlanguage <id>` / `.listlanguages` | `.sl` / `.ll` | language of the buffer |
| `.fm_file` / `.fm_block` | `.fmf` / `.fmb` | reindent file / block |
| `.copyoutput` / `.outputtobuffer` | `.co` / `.ob` | last terminal command's output: to clipboard / to a new buffer |
| `.theme --import <file>` | | convert a VS Code / Sublime theme |
| `.theme_reload` / `.theme_list_colors` | `.thr` / `.thc` | theme |
| `.cfg ...` | | configuration (above) |
| `.listviews` / `.showview` / `.hideview <name>` | `.lv` / `.sv` / `.hv` | views |

## Terminal

- `Opt+T` / `Alt+T` focuses it; it's a real shell (a pty), and full-screen programs (`vim`, `less`, ...) run in it.
- `Tab` hands line editing to the shell (completion); `Ctrl+C` takes it back.
- `.co` / `.ob` grab the output of the last command.
