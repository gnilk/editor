# GoatEdit 🐐

[![Build](https://github.com/gnilk/editor/actions/workflows/cmake.yml/badge.svg)](https://github.com/gnilk/editor/actions/workflows/cmake.yml)

**A hackable, console-flavoured code editor written in modern C++20.**

Someone (Peter Norton?) once said that to really become a programmer you have to write an
editor. This started as that playground — and grew into something I actually use every day.

GoatEdit pairs a real text editor with a first-class **embedded shell**. Hit a key and the
bottom of the window becomes a live terminal — run `make`, `git`, `vi`, anything — and the same
prompt doubles as a command line for the editor itself. Think Amiga AsmOne or a game console's
drop-down terminal, but driving a syntax-highlighting, multi-backend editor.

![GoatEdit editing its own source, with the embedded terminal below](screenshots/GoatEdit_2026-06-05.png?raw=true)

---

## Why you might like it

- **The terminal is a feature, not an afterthought.** A genuine PTY-backed shell lives inside the
  editor (`bash`/`zsh`/whatever you fancy). It handles colours, full-screen apps like `vi` and
  `less`, command history, tab-completion, and window-size reporting — the real deal, not a fake
  prompt.
- **One prompt, two brains.** Type a normal command and it goes to the shell. Prefix it (configurable,
  default `.`) and it runs an *editor* command instead — save, load, search, or any plugin you've
  written. No mode-juggling.
- **Pluggable rendering backends.** SDL3 (the primary, "real application" frontend with TrueType
  rendering via stb_truetype) and SDL2 for CI/fallback. The core knows nothing about pixels, so a
  fresh backend is a self-contained job — a modern-terminal backend (no NCurses baggage) is on the
  roadmap.
- **Terminal Compatible** Modern ANSI Terminal rendering (no NCurses) - 256/truecolor terminals supported,
  runs well over SSH.
- **Scriptable in JavaScript.** An embedded Duktape engine exposes the editor through a clean JS API
  (`Editor`, `Document`, `View`, `Theme`, `Console`…). The startup banner goat? That's a plugin.
- **Its own syntax engine.** A compact stack-based tokenizer drives highlighting (C++, JSON, Make,
  and a default fallback), tokenizing in the background so big files don't stall the UI.
- **Runs on Linux and macOS.**

---

## A quick tour

### The embedded terminal is a *real* terminal
Build your project without leaving the editor — the output streams into the same window, with
colours intact.

![Running a build inside the embedded terminal](screenshots/GoatEdit_term_2026-06-05.png?raw=true)

### Quick-command mode (vi/Sublime flavour)
A fast overlay for search, navigation, and plugin commands without reaching for the full terminal.
Results are written straight to the terminal pane — here, a search across the buffer.

![Quick-command search results in the terminal pane](screenshots/GoatEdit_Search_2026-06-05.png?raw=true)

---

## Building

The build is driven by **CMake 3.28+**. Source dependencies (json, gnklog, dukglue, fmt) are fetched
by CMake itself via FetchContent on first configure — no manual step; *system* dependencies you
install yourself.

### Linux

```sh
sudo apt-get update
sudo apt-get install -y libyaml-cpp-dev libsdl2-dev

# Configure (SDL3 is on by default)
cmake -B ./cmake-build-debug -DCMAKE_BUILD_TYPE=Debug

# ...or build the SDL2 backend instead (handy on CI / older boxes)
cmake -B ./cmake-build-debug -DCMAKE_BUILD_TYPE=Debug -DGEDIT_BUILD_SDL3=OFF -DGEDIT_BUILD_SDL2=ON

# Build the editor
cmake --build ./cmake-build-debug --config Debug --target goatedit -j
```

### macOS
Same flow, swap `apt-get` for `brew`.

### Dependencies
The following must be installed on the system
* libsdl2-dev or libsdl3-dev
* libyaml-dev
* testrunner (from https://github.com/gnilk/testrunner) - for unit testing

```shell
sudo apt install libsdl2-dev libsdl3-dev libyaml-dev
```

The following are installed during build
* nlohmann/json
* fmtlib
* dukglue
* gnklog

The following are bundled directly with the source
* stb (various parts)
* duktape 2.7.0

> Duktape ships pre-configured in the repo (`src/ext/duktape-2.7.0`) so you don't have to fight its
> build — a scar from an old GitHub Actions battle.

---

## Project layout

Everything lives in the `gedit` namespace. A few landmarks:

- `main.cpp` — picks a backend, then `Editor::Instance().Initialize()` / `OpenScreen()`.
- `src/Core/Editor.*` — the application singleton (owns the workspace, model, plugin engine, theme,
  keymap, runloop).
- `src/Core/TextBuffer / Line / Workspace` — the data model (a buffer is a vector of
  `Line`s; a `Line` is a `std::u32string` plus token attributes).
- `src/Core/UI/` - UI base with graphics, input and controllers
- `src/Core/Editor/UI/` — Controller and Application views (workspace, editor, terminal) + status bars etc..
- `src/Core/Controllers/` — input logic decoupled from views (`EditController`, `TerminalController`,
  `QuickCommandController`).
- `src/Core/Graphics/` — SDL2/3, GAnsi backends behind the `ScreenBase` / `DrawContext` interfaces.
- `src/Core/Language/` — the tokenizer and per-language configs.
- `src/Core/JSEngine/` — Duktape host + the JS API wrappers; plugin scripts live in `src/Plugins/`.
- `src/Core/Config/` — YAML config and theming.
- `src/ext/gansi/` - Terminal rendering library, written as a stand alone library

There's a much deeper architecture/coding-standards write-up in [`CLAUDE.md`](CLAUDE.md) — worth a
read before a first PR.

---

## Want to hack on it?

**Contributions are very welcome** — this is a personal project that has outgrown one person's spare
evenings, and there's a lot of low-friction surface area to jump in on:

- 🧩 **Write a plugin.** The JS API is the easiest on-ramp — add a cmdlet, a theme tweak, a goat.
- 🎨 **Add a language.** Implement a `LanguageBase` + a tokenizer config (see `CPPLanguage` /
  `JSONLanguage` as templates).
- 🐚 **Terminal/VT corner cases.** The VT parser (`VTermParser`) handles a healthy subset of
  xterm — origin mode, bracketed paste, and friends are still open.
- 🧪 **Tests.** The suite runs under [trun](https://github.com/gnilk/testrunner); modules live in
  `utests/`. Run them from `cmake-build-debug/`:
  ```sh
  cd cmake-build-debug && trun --sequential ./libutests.so
  ```

If you're picking something up, open an issue or draft PR early so we can compare notes. Style and
conventions are documented in [`CLAUDE.md`](CLAUDE.md); the short version is C++20, RAII everywhere,
4-space K&R, and code ordered caller-before-callee so files read top-to-bottom.

---

## Building for Distribution

**1) Build** (release recommended for distribution):

```shell
cmake --preset release
cmake --build --preset release --target goatedit
```

**2a) `.deb`** — the CPack `package` target (uses the `DEB` generator on Linux); the package lands
in the build dir:

```shell
cmake --build ./cmake-build-release --target package
# -> cmake-build-release/goatedit-<ver>-linux-<arch>.deb
```

**2b) AppImage** — built separately; writes `goatedit-<arch>.AppImage` to the repo root by default:

```shell
./scripts/build-appimage.sh
```

Notes on the AppImage script:
- `./scripts/build-appimage.sh [BUILD_DIR]` — first arg (or `$BUILD_DIR`) overrides the build dir
  (default `cmake-build-release`); `$OUTPUT_DIR` overrides where the `.AppImage` lands; `$VERSION`,
  if set, is folded into the filename (`goatedit-<VERSION>-<arch>.AppImage`).
- Needs `curl` (fetches `linuxdeploy` once, cached in `<BUILD_DIR>/appimage-tools/`) and FUSE — the
  script sets `APPIMAGE_EXTRACT_AND_RUN=1` as a fallback if FUSE isn't available.
- It runs `cmake --install <BUILD_DIR> --prefix <AppDir>/usr` (reusing the project's install rules),
  bundling the binary, SDL + yaml-cpp, the assets, the desktop file, and the icon, then sets the
  custom `AppRun`.


## License

BSD 3-Clause © 2023 Fredrik Kling. See [`LICENSE`](LICENSE).
