# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

**GoatEdit** — a personal text/code editor written in C++20. Runs on Linux and macOS. Supports multiple rendering backends (NCurses terminal, SDL2, SDL3). Embeds a JavaScript plugin engine (Duktape) for scripting. The primary executable target is `goatedit`.

## Build

CMake 3.22+ is required. Build is configured per-platform with optional SDL2/SDL3 flags.

```sh
# Install system deps (Linux)
sudo apt-get install -y libyaml-cpp-dev libncurses-dev libsdl2-dev
./setup_deps.sh       # clones ext/ source deps (json, gnklog, dukglue, fmt)

# Configure (SDL3 on by default, SDL2 off)
cmake -B ./cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
# To build without SDL3 (e.g. CI):
cmake -B ./cmake-build-debug -DCMAKE_BUILD_TYPE=Debug -DGEDIT_BUILD_SDL3=OFF -DGEDIT_BUILD_SDL2=ON

# Build the main binary
cmake --build ./cmake-build-debug --config Debug --target goatedit -j

# Build the unit test shared library
cmake --build ./cmake-build-debug --config Debug --target utests -j
```

CMake auto-clones missing `ext/` dependencies on first configure (except duktape, which is pre-included in `src/ext/duktape-2.7.0`).

## Running Tests

Tests use the **trun** (TestRunner v3) framework. The test target builds `utests` as a shared library; `trun` loads and runs it.

Always use the '--sequential' parameter when running tests during development cycles. This will remove forking from unit-tests which may cause
log output to be unsynchronized.

The -m and -t options can both takes lists and wildcards, prefix '!' can be used to discard tests/modules and the special token '-' means everything.
This can be used to control the order of execution:
-t case1,case2,-,!case5
Execute case1 and case2 first then all other cases expect case5..
or
-t parse*
execute all cases starting with 'parse'

```sh
# Run all tests
trun cmake-build-debug/libutests.so

# Run a specific module
trun -m textbuffer cmake-build-debug/libutests.so

# Run a specific test case
trun -m textbuffer -t insert cmake-build-debug/libutests.so

# List available tests
trun -l cmake-build-debug/libutests.so
```

Test source files live in `utests/`. Each `test_<module>.cpp` corresponds to one module; `test_main.cpp` initializes the editor singleton for all tests.

## Architecture

### Namespace
All code lives in the `gedit` namespace.

### Entry Point
`main.cpp` — selects and initializes the rendering backend, then calls `Editor::Instance().Initialize()` and `Editor::Instance().OpenScreen()`.

### Core Singleton: `Editor` (`src/Core/Editor.h`)
The application singleton. Owns the `Workspace`, active `EditorModel`, `JSPluginEngine`, `Theme`, `KeyMapping`, and `Runloop`. The `Runloop` drives the message pump and dispatches `KeyPressAction` events.

### Data Model
- **`TextBuffer`** (`src/Core/TextBuffer.h`) — file content as a vector of `Line` objects. Handles async background tokenization/parsing.
- **`Line`** (`src/Core/Line.h`) — single line of text stored as `std::u32string` with syntax token attributes.
- **`EditorModel`** (`src/Core/EditorModel.h`) — pairs a `TextBuffer` with cursor position, undo history, and language.
- **`Workspace`** (`src/Core/Workspace.h`) — collection of open `EditorModel`s.

### View Hierarchy (`src/Core/Views/`)
Views form a tree rooted at `RootView`. Layout containers: `VStackView`, `HStackView`, `VSplitView`, `HSplitView`. All inherit from `ViewBase` which handles keypresses via `KeypressAndActionHandler`. Key concrete views:
- `EditorView` — renders text buffer contents
- `GutterView` — line numbers
- `TerminalView` — embedded shell terminal (uses `forkpty`)
- `WorkspaceView` — file browser panel
- Modal views (`ListSelectionModal`, `TreeSelectionModal`) overlay the main layout

### Controllers (`src/Core/Controllers/`)
Controllers hold input-handling logic decoupled from views:
- `EditController` — text editing actions
- `TerminalController` — terminal/command mode
- `QuickCommandController` — vi-like quick command overlay

### Actions & Key Mapping
`kAction` enum (`src/Core/Action.h`) defines all editor actions. `KeyMapping` (`src/Core/KeyMapping.h`) maps key presses → actions, loaded from config YAML. `KeypressAndActionHandler` is a mixin that views/controllers inherit to declare which actions they handle.

### Rendering Backends (`src/Core/SDL3/`, `src/Core/NCurses/`)
Each backend implements `ScreenBase`, `NativeWindow`, and `DrawContext` interfaces. SDL3 is the primary active backend; SDL2 is the CI/fallback backend. NCurses sources exist but are commented out of the build. `SDLFontManager` uses stb_truetype for TTF rendering.

### Language / Syntax Highlighting (`src/Core/Language/`)
`LanguageBase` is the interface. `LangLineTokenizer` uses a stack-based tokenizer configured per-language. Concrete languages: `CPPLanguage`, `JSONLanguage`, `MakeBuildLang`, `DefaultLanguage`. Language selection is driven by file extension via `Config`.

### Plugin / Scripting (`src/Core/JSEngine/`)
Embeds Duktape 2.7.0 (`src/ext/duktape-2.7.0/`). `JSPluginEngine` loads `.js` files from the `resources/Plugins/` directory. JS API wrappers live in `src/Core/JSEngine/Modules/` and expose `DocumentAPI`, `EditorAPI`, `ThemeAPI`, `ViewAPI`, etc. Plugin JS source lives in `src/Plugins/`.

### Configuration (`src/Core/Config/`)
YAML-based config loaded by `Config` singleton. `ConfigNode` provides typed access. `Theme` loads `.theme.yml` files. Runtime defaults are in `Assets/Resources/config.yml`.

### Platform-Specific
- Linux: `src/Core/Linux/LinuxFolderMonitor.cpp`
- macOS: `src/Core/macOS/MacOSKBEmulator.cpp`, `MacOSFolderMonitor.cpp`
- Unix shared: `src/Core/unix/Shell.cpp` (pty/shell spawning)

## Key Conventions

- All types use a `Ref = std::shared_ptr<T>` alias defined inside the class.
- `Logger` from `ext/gnklog` is used throughout; include `"logger.h"`.
- String literals for unicode content use `U"..."` (`char32_t`); `UnicodeHelper` handles conversions.
- `fmt` (fmtlib) is the string formatting library.
- Platform macros: `GEDIT_LINUX`, `GEDIT_MACOS`, `GEDIT_USE_SDL3`, `GEDIT_USE_SDL2`.


## Coding Standards

### General
- Always use curly braces for all control flow, even single-line bodies
- C++17 standard throughout
- RAII everywhere, no raw owning pointers

### Formatting
- 4-space indentation, no tabs
- Opening brace on same line as statement (K&R style)
- One blank line between methods

### Naming
- PascalCase for classes and structs: `GraphicsDevice`
- PascalCase for methods and variables: `InitDevice()`
- UPPER_SNAKE_CASE for constants: `MAX_BUFFER_SIZE`
- Never prefix member variables with `m_`: `m_width`
- Convey intent with variables: `bool isSomething`
- camelCase for variables: `isSomething`

### C++ Specifics
- Prefer `nullptr` over `NULL`
- Use `auto` sparingly, only when type is obvious from context
- Mark all single-argument constructors `explicit`
- Prefer range-based for loops

### Test infrastructure / conventions
- **Test fixtures**: new `Assets/testfiles/` (contains `ConvertUTF.cpp` — mixed tabs+spaces, good
  tab-render fixture). Copied to `cmake-build-debug/testfiles/` (NOT `EDITOR_ASSET_DIR` — these are
  dev-only, not redistributable). `utests` depends on the `testfiles` copy target.
- **Running tests**: `trun -m <modules> --sequential cmake-build-debug/libutests.so`. `--sequential`
  disables forking for synchronized log output during dev. `-t` takes a list, supports wildcards,
  `!name` to exclude, and `-` meaning "all the rest" (e.g. `-t case1,case2,-` runs those first then
  the rest). Verified-green set this session:
  `trun -m clipboard,edtmodel,vnav,cpplang,jsonlang,cppnumbers,linelayout --sequential ...` (67 cases).
  Note: trun forks per-test by DEFAULT (omit `--sequential`) — useful when a case may crash/segfault,
  so one bad case is isolated and the rest still report instead of aborting the run.
- **Do NOT run the full debug suite** — pre-existing failures in `test_textbuffer_{flatten,parsefull,
  parseregion,thparsefull,thparseregion}` confirmed present at baseline; plus the sqlite3-parse and
  thread/timer tests hang. Gating these is outstanding.

### Recently completed (carry-forward state)
- **Vertical page-nav (CLion/content-first)** — `VerticalNavigationCLion::OnNavigateDown` used to add a
  spurious +1 line on PageDown, breaking PageDown/PageUp symmetry (caret drifted +1 per cycle). Removed;
  PageDown now moves the view by `height-1` and keeps the caret's screen row (real CLion behaviour, which
  IS symmetric). Re-enabled the previously-stubbed `test_edtmodel_text_linefunc` /
  `test_edtmodel_delete_text` asserts and fixed `test_vnav_pagedown` cases 3 & 4 to the no-+1 values.
  Commit `52529d9`.
- **ClipBoard paste** — rewrote `ClipBoard::ClipBoardItem::PasteToBuffer` as a clean text-splice (split
  the target line once at the paste column into head|tail; first segment joins head, middle segments are
  their own lines, last segment carries tail). Fixes mid-line/region paste (no target-line split before),
  an OOB read on single-line partial clips, and trailing-line inconsistency. `utests/test_clipboard.cpp`
  is now a fine-grained matrix: copy shapes {fullline,partial,multifull,partial-start,partial-end,
  partial-both} × paste destinations {empty, col0, mid-overlap, end-append}, all asserting the correct
  splice result; 21/21 green. `MakeNumberedBuffer` drops the seeded `line[0]` so coords are 1:1.
  Full-line-at-col-0 paste is byte-identical to before (the editor's verified path). Commit `00e9eee`.

### Remaining / deferred (carried + new)
- **dcoverlay** `IsInside` boundary — confirm whether the right/bottom edge is inclusive; fix code or
  test accordingly.
- **jsengine loadbuffer/listbuffers** — reinstate `Editor.LoadBuffer`: add a `LoadDocument(filename)`
  to `EditorAPIWrapper` mirroring `NewDocument` (`editorApi->LoadModel(...)` wrapped in
  `DocumentAPIWrapper`, registered via `dukglue_register_method`), update `loadbuffer.js` to the
  Document API, then un-stub both tests. `Editor::LoadModel(const std::string&)` still exists.
- **clipboard PasteFromClipboard cursor advance** — `EditorModel::PasteFromClipboard`
  (`src/Core/EditorModel.cpp:930`) advances the cursor by `clipboard.Top()->GetLineCount()` (the count
  of stored WHOLE lines) and uses the same count for the undo range. After the `PasteToBuffer` rewrite
  the number of lines actually added depends on the selection shape (a single-line partial paste adds
  0 new lines; a full-line/region paste adds N or N+1). So for partial-region pastes the cursor can
  land on the wrong line and the undo range can be off. The correct advance is "number of lines the
  splice added" + "final segment length" for the column — `PasteToBuffer` should report what it did
  (e.g. return the end Point) instead of the caller guessing from `GetLineCount()`. Untested; not yet
  addressed.
