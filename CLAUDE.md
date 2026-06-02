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

## Session Notes — C++ Preprocessor Tokenization (2026-06)

Work on syntax highlighting for C++ preprocessor directives via the stack-based
`LangLineTokenizer`. Three commits on `main`: `fa21303` (ConfigNode cleanup),
`4873dd0` (preprocessor tokenization), `bb3803a` (state-leak fix).

### What was done
- **ConfigNode cleanup** (`src/Core/Config/ConfigNode.h`): merged the two early-return
  guards in `GetSequence` into one `||` condition; `GetSequenceOfStr` now delegates to
  `GetSequence<std::string>`.
- **Preprocessor states** (`src/Core/Language/LanguageSupport/CPPLanguage.cpp`): replaced
  the old whole-word `#include` match with a `#`-triggered `in_preprocessor` state.
  `#include` delegates to the existing `in_include`/`in_include_angle` sub-states;
  `#ifdef`/`#ifndef`/`#undef` delegate to a shared `in_pp_macro` sub-state.
- **New token classes** (`src/Core/Language/LanguageTokenClass.h`): `kPreProcessor` (the
  `#` and directive keyword) and `kMacroIdentifier` (the operand of ifdef/ifndef/undef).
  Both must be added to the `tokenNames` map in `LangToken.cpp` (an unmapped class calls
  `exit(1)` at startup) and to `Assets/Resources/colors.json` under both `globals` and
  `content` (themed `pink` and `orange3`). Token-class enum values must stay contiguous
  0..`kLastTokenClass`-1 (the color-config loop in `Editor.cpp` iterates them).
- **Tokenizer fixes** (`src/Core/Language/LangLineTokenizer.cpp`): (1) EOL flush now pops
  *all* nested line-terminal states in a `while` loop, not a single pop — required for
  nested directive states and also fixes a latent leak on unterminated `#include <foo`.
  (2) An empty/stuck token now `break`s (was `return`) so the EOL flush still runs.
- Tests in `utests/test_cpplang.cpp`: `test_cpplang_include` (updated), `test_cpplang_ppmacro`,
  `test_cpplang_ppnoleak`.

### Tokenizer mechanics (learned this session)
- Each `State` has identifier lists (per token class), optional actions (push/pop on a
  matched token), an `eolAction`, a `regularTokenClass` (fallback for unrecognized text),
  and optional `postfixIdentifiers` (tokens that break greedy text collection).
- `GetNextToken` order: number matcher → partial (non-whole-word) identifier longest-match
  → greedy text collection (stops at whitespace or a postfix identifier) → whole-word
  identifier match. Actions fire afterwards in `CheckExecuteActionForToken`.
- **Postfix footgun**: a `postfixIdentifiers` set containing a char that is NOT also an
  identifier/action in that state yields an empty token (collection breaks immediately,
  iterator doesn't advance). That's why `in_preprocessor` has NO postfix — operands collect
  greedily to whitespace. Don't add a postfix set to a state unless those chars are also
  matched as identifiers/actions (as `main`/`in_string`/`in_include_angle` do).

### Decisions / patterns established
- `#` and the directive keyword → `kPreProcessor`; the macro operand → `kMacroIdentifier`
  (a distinct new class, chosen over reusing `kImport`, aligning with a future
  "known/user identifiers" class). Include path content stays `kString` via `in_include`.
- Single-line directives use the pattern: `regularTokenClass` + `SetEOLAction(kPopState)`,
  no postfix. The EOL action is the guaranteed unwind; rely on it rather than trying to
  match every operator inside the directive.
- Run tests with a **targeted module selection** from `cmake-build-debug/` (resources path
  is cwd-relative), always `--sequential`, lib is `libutests.dylib`. Do NOT run the full
  suite in debug — the sqlite3-amalgamation parse test and thread/timer dev tests make it
  hang. Verified set: `trun -m cpplang,jsonlang,cppnumbers --sequential libutests.dylib`.

### Remaining / deferred
- **EOL handling is naive** (binary pop/none, no line continuation). Blocks proper
  `#define` multi-line bodies (trailing `\`). Fix: a conditional EOL action / per-state
  continuation token that suppresses the pop when the line ends with `\`; can reuse the
  existing "state survives EOL → stack-depth >1 → reparse region extends" path that block
  comments use (`FindParseRegionStart`/`FindParseRegionEnd`).
- **Not yet implemented**: `#define` (object/function-like, params, body, continuation —
  hard tier), `#if`/`#elif` expressions (`defined`, operators, numbers — moderate),
  `#line` (number + filename), `#error`/`#warning` (trivial message-to-EOL sub-state,
  deliberately skipped as low value).
- `#include<vector>` (no space) no longer enters the angle sub-state (the postfix that
  enabled it was removed); spaced form works. Considered an acceptable trade.
- Test cleanup wanted: the slow sqlite3-parse and thread/timer tests should be gated so
  the full suite is runnable in debug (the user has local stubs in `test_textbuffer.cpp`
  and `test_timer.cpp`, uncommitted).
