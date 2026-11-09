# Install layout and asset lookup

Where GoatEdit's files live, and how the editor finds them. One rule on every platform: **system assets
are found relative to the running executable**, never relative to the current directory.

## Three layers

| Layer | What | Who writes it |
|---|---|---|
| **System** | `config.yml`, themes, keymaps, plugins, fonts - what the build copies into `resources/` | the build / installer only |
| **User** | the user's overrides: `config.yml`, keymaps, themes (`.theme import`) | the user, `.cfg`, `.theme` |
| **Project** | `<root>/.goatedit/` - project config, session, scrollback, history | the editor |

State that is neither configuration nor an asset (logs, user-scope terminal history) lives in the user
**state** directory.

## System assets - exactly one directory

The first candidate that holds a `config.yml` wins, and it is the *only* system directory. A dev tree and an
installed copy are never mixed.

1. `--asset-dir <dir>` - explicit override; when given, nothing else is searched (tests use it).
2. `<exedir>/resources/` - a build tree (`bld/goatedit`, `cmake-build-debug/goatedit`).
3. `<exedir>/../SharedSupport/` - the macOS bundle (`GoatEdit.app/Contents/MacOS/goatedit`).
4. `<exedir>/../share/goatedit/` - any install prefix: `/usr`, `/usr/local`, `~/.local`, an AppImage's `$APPDIR/usr`.
5. `$XDG_DATA_DIRS/goatedit` - fallback for distro packaging that splits binary and data.

`<exedir>` comes from `/proc/self/exe` (Linux) or `_NSGetExecutablePath` (macOS), with symlinks resolved -
a `~/bin/goatedit -> ~/src/editor/bld/goatedit` symlink finds the build tree.

The old `./resources/` (current directory) lookup is gone: it only worked when launched from the build
directory, and any directory with a `resources/` folder in it could take over the system assets.

## User layer - one preferred directory, legacy read-only

Searched in this order, first match wins per file:

1. `$XDG_CONFIG_HOME/goatedit` (default `~/.config/goatedit`) - **preferred**, where new files are written.
   Same path on macOS (not `~/Library/Application Support`), for one layout everywhere.
2. `~/.goatedit` - legacy.

`$XDG_DATA_HOME/goatedit` (`~/.local/share/goatedit`) used to hold the Linux user config, but it is where a
per-user install (`~/.local` prefix) puts its *system* assets - read as a user layer, the installed config
would be merged over a build tree's and `.cfg --user` would edit the installed file. It is not read; move a
user config from there to `~/.config/goatedit`.

Legacy directories are still read so nothing breaks, and a warning in the log names the files to move. A file
that already exists in a legacy directory is written back there (`.cfg` edits it in place); new files go to the
preferred directory.

## State

`$XDG_STATE_HOME/goatedit/` (default `~/.local/state/goatedit/`): `goatedit.log`, user-scope `terminal_history`.
On macOS the log stays in `~/Library/Logs/GoatEdit`. A project with a `.goatedit` directory keeps its history
there instead, as before.

## Install modes

Both use the CMake install rules (`bin/`, `share/goatedit/`, `share/applications/`, `share/icons/`); the
exe-relative lookup finds the assets under either prefix with no wrapper or environment variables.

```sh
scripts/install.sh --user                 # ~/.local  (only me; ~/.local/bin must be on PATH)
scripts/install.sh --system               # /usr/local (all users; runs the install with sudo)
scripts/install.sh --uninstall --user     # removes exactly what the install wrote
```

Both also install `goat`, a symlink to `goatedit` in the same `bin/` (`GEDIT_CLI_NAME`; the Linux side of
the macOS `install-cli` wrapper - no wrapper needed, the asset lookup resolves the link), listed in the manifest.

Plain `make install` / `cmake --install <build>` works too: on Linux the default prefix is `~/.local` (no root),
set on a build dir's FIRST configure - an existing build dir keeps its cached prefix until given
`-DCMAKE_INSTALL_PREFIX=$HOME/.local`. It leaves no manifest in the prefix, so it can't be uninstalled by the script.

The install records its file list as `<prefix>/share/goatedit/install_manifest.txt`; `--uninstall` removes
those files and any directories left empty. User and state directories are never touched by uninstall.

The macOS bundle (`/Applications`) and the `.deb` (CPack, prefix `/usr`) are unchanged; both land under rule 3/4.

## Tests

`trun` is the executable, so the exe-relative rule can't see the build tree. `test_main` locates its own
`libutests` with `dladdr` and passes `--asset-dir <libdir>/resources`, so tests never pick up an installed copy.
Fixtures (`testfiles/`) are still resolved relative to the current directory - run `trun` from the build dir.
