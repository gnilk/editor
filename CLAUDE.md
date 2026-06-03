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

## Session Notes — Build/Backend cleanup, Language parser, Tab rendering (2026-06-02)

Three areas this session: startup/argument plumbing, the language tokenizer, and a full
tab-rendering implementation. All merged to `main` and pushed (`git@github.com:gnilk/editor.git`,
remote switched to SSH this session).

### 1. Build / backend / argument parsing
- **Headless tests**: `test_main` now passes `--backend headless` (argv) so the layout/editor
  tests run without a display. The `--backend` flag overrides the config value.
- **Argument parsing consolidated**: `PreParseArguments` → `ParseArguments` (single pass).
  Added members `argBackend` and `pendingFiles` (alongside `keepConsoleLogger`/`loadUserConfig`).
  `Initialize` iterates `pendingFiles` for file opens; `ConfigureSubSystems` prefers `argBackend`
  over config instead of mutating `Config`.
- **SDL backends are mutually exclusive** (SDL2 and SDL3 share `SDL_*` C symbols → link collision).
  Config + `--backend` now use a single `sdl` identifier; the binary picks whichever SDL it was
  built with (`GEDIT_USE_SDL3` preferred, else `GEDIT_USE_SDL2`). `CMakeLists.txt` uses
  `if (GEDIT_BUILD_SDL3) ... elseif (GEDIT_BUILD_SDL2)` so only one compiles, with a warning if both
  flags are set. `Assets/Resources/config.yml` default is `backend: sdl`.

### 2. Language tokenizer (`src/Core/Language/`)
- **`LangLineTokenizer`**: renamed `StartParseRegion`/`EndParseRegion` →
  `FindParseRegionStart`/`FindParseRegionEnd` (made `const`); added public
  `ComputeParseRegion(lines, start, end)` returning the mapped `{start,end}` (used by `ParseRegion`
  and by tests to verify region extension without exposing internals). Removed a stale "doesn't work
  at top-of-file" comment (the `idxStart != 0` guard handles it).
- **`State::identifiers` was `std::unordered_map` → now `std::map`.** The unordered map made
  prefix-match order hash-dependent and non-deterministic. Switching to `std::map` (ordered by
  `kLanguageTokenClass` enum value) exposed that matching relied on hash luck. **Fix: longest-match**
  in both `GetNextToken` (prefix loop) and `State::ClassifyToken` — the longest matching token wins,
  so `/*` (2-char `kBlockComment`) beats `/` (1-char `kOperator`) regardless of enum order. This is
  the key tokenizer invariant now: **declare nothing twice; rely on longest-match, not ordering.**
- **Operator-list conflicts fixed**: removed `{ }` from `cppOperators` and `{ } [ ]` from
  `jsonOperatorsFull` (they shadowed `kCodeBlockStart/End` / `kArrayStart/End`, breaking indentation
  non-deterministically). Removed `bool` from `cppKeywords` (it lived in both keywords and
  `cppTypes`; keywords won under `std::map`). `cppOperators` reordered strictly 3-char → 2-char →
  1-char; added `<=>`, `->*`, `::`; removed duplicate `:` `<` `>`. `cppOperators` and
  `cppOperatorsFull` are now identical — open question whether to merge them.
- **`LineAttrib` default `tokenClass`**: was uninitialized → `kUnknown` (0). Now the `Line.cpp`
  ctor defaults it to `kRegular`. `kUnknown` should mean "something went wrong", never a default.
- **Content additions**: expanded `cppTypes` (bool, `int8_t..uint64_t`, `size_t`/`ptrdiff_t`/
  `intptr_t`/etc., C++23 `float16_t..float128_t`, `bfloat16_t`); fixed `cppKeywords`
  (`conteval`→`consteval`, removed `reflexpr`/`synchronized` which never made the standard).
- **`kImport` token class** (language-agnostic include/import; distinct from a future macro class —
  note the sibling separately added `kPreProcessor`/`kMacroIdentifier`). `#include` classifies as
  `kImport`; `in_include`/`in_include_angle` states colour both `"path"` and `<path>` as `kString`.
- **Number support — the reusable pattern**: numbers can't live in a static identifier list (the
  grammar must be executed). Added `NumberMatcherBase` (interface, `int Match(view)` → chars consumed,
  0 = not a number). A `State` may hold a `numberMatcher`; `GetNextToken` consults it *before* the
  identifier loop (so `.5` is a number, not `.`+`5`). null = state doesn't classify numbers (so
  strings/comments are unaffected). `CPPNumberMatcher` does C/C++ decimal/hex/binary/float/exponent/
  suffixes/`'` separators; assigned to the `main` state only. Each language ships its own matcher.

### 3. Tab rendering (non-destructive — tabs stay tabs on disk)
Core insight: **two coordinate spaces, equal today only because every char was one cell** —
character index (buffer/editing/tokenizer/undo/selection truth) vs visual column (on-screen grid).
A tab is one char index but spans up to `tabSize` columns. Fix the mapping only at the render
boundary and in the vertical-nav anchor; never mutate the buffer.
- **Phase 0** — `Line::CharToVisualColumn(charIdx, tabSize)` and `Line::VisualToCharIndex(visualCol,
  tabSize)` (pure, lock-free const; callers in the render path already hold the line lock).
  `test_linelayout.cpp` covers both + round-trip + edges.
- **Phase 1** — `LineRender::ExpandTabs(in, startCol, tabSize)` (static, render-only) expands tabs to
  stops in all four draw paths; `tabSize` threaded via the `LineRender` ctor. `EditorView` passes
  `editorModel->GetTextBuffer()->GetLanguage().GetTabSize()`; terminal/command use the default 4.
- **Phase 2a** — `EditorView::SetWindowCursor` translates the caret's char index → visual column on
  a *copy* before `window->SetCursor`. Model cursor stays char-index; `AddCharToLine` unchanged.
  This is what fixed "inserts land in the wrong place" (the caret was being drawn left of the glyph).
- **Phase 2b** — `wantedColumn` redefined from char index to **visual column** so up/down preserves
  the on-screen column. Centralized into `EditorModel::CaptureWantedColumn` (char→visual) and
  `ApplyWantedColumn` (visual→char, clamped); the single consumer (`UpdateModelFromNavigation`) and
  all in-model writers route through them. `BaseController` (shared with tab-agnostic terminal/command
  input) got an `editTabSize` member defaulting to **1** (visual == char, a no-op); `EditController`
  sets it from the language each keypress. Status-bar `c(...)` readout now shows the visual column
  (kept 0-based — a 1-based line+column sweep is deferred).

### Test infrastructure / conventions
- **Test fixtures**: new `Assets/testfiles/` (contains `ConvertUTF.cpp` — mixed tabs+spaces, good
  tab-render fixture). Copied to `cmake-build-debug/testfiles/` (NOT `EDITOR_ASSET_DIR` — these are
  dev-only, not redistributable). `utests` depends on the `testfiles` copy target.
- **Running tests**: `trun -m <modules> --sequential cmake-build-debug/libutests.so`. `--sequential`
  disables forking for synchronized log output during dev. `-t` takes a list, supports wildcards,
  `!name` to exclude, and `-` meaning "all the rest" (e.g. `-t case1,case2,-` runs those first then
  the rest). Verified-green set this session:
  `trun -m cpplang,jsonlang,cppnumbers,linelayout --sequential ...`.
- **Do NOT run the full debug suite** — 7 pre-existing failures (`test_edtmodel_delete_text`,
  `test_edtmodel_text_linefunc`, `test_textbuffer_{flatten,parsefull,parseregion,thparsefull,
  thparseregion}`) confirmed present at baseline *before* this session's changes (the "faulty tests"
  with uncommitted local stubs); plus the sqlite3-parse and thread/timer tests hang. Gating these is
  outstanding.

### Remaining / deferred
- **Tab Phase 3**: mouse hit-testing (`VisualToCharIndex` is written + tested but unused until
  click-to-position exists); horizontal-scroll clip (`nCharToPrint` clips by char count, slightly off
  on tabbed lines); selection-rectangle tab-awareness.
- **1-based line/column visualization sweep** (lines are currently 0-based too — do them together).
- **Gate the 7 failing + slow tests** so the full suite is runnable in debug.
- **Merge `cppOperators`/`cppOperatorsFull`?** — now identical.
- Language (from the sibling's list): `#define` multi-line (`\` continuation), `#if`/`#elif`
  expressions, `#line`.

## Session Notes — Test-suite cleanup (Timer, parse tests) (2026-06-03)

Goal: a debug suite that runs green without hanging. Three commits on `main`:
`d80e8e6` (Timer fix), `ca2f4ee` (parse-test hygiene), `6fe3b16` (flatten off-by-one).

### What was done
- **Timer concurrency fix** (`src/Core/Timer.{h,cpp}`) — `Timer` is production code (`TextBuffer`
  uses it for the async syntax parser). It used predicate-less `condition_variable` waits and mutated
  `wakeupReason`/`hasExpired` without the mutex, so a `Stop()`/`Restart()` notify that raced ahead of
  the wait was **lost** → worker blocked forever → `HasExpired()` never flipped and `~Timer()`'s
  `join()` hung. That's why the timer tests were commented out. Fix: all shared state under `mymutex`;
  added `commandPending` as the wait predicate (kills lost wakeups); `hasExpired` is now
  `std::atomic<bool>` (lock-free `HasExpired()`); the handler runs with the lock released so it can
  call back into the timer.
- **Timer tests re-enabled** (`utests/test_timer.cpp`) with a bounded `WaitForExpiry(timer, budget)`
  helper instead of infinite spin — a regressed timer now *fails* the test instead of hanging the
  suite. Covers expiry, `Restart()`, `Restart(dur)`, `Stop()`.
- **sqlite3 parse test gated to release** — `test_textbuffer_parselarge` wrapped in `#ifdef NDEBUG`
  (it's ~6s and needs an untracked 8.4MB `sqlite3.c`). Compiles/runs only in release builds.
- **Parse tests use a tracked fixture** — `parsefull`/`parseregion`/`thparsefull`/`thparseregion`
  now load `testfiles/ConvertUTF.cpp` (tracked, copied to the build dir by the `testfiles` target)
  instead of the untracked `test_src2.cpp`, so they pass on a clean checkout.
- **thparseregion async race fixed** — `ReparseRegion` is asynchronous and *returns a `Job`*; polling
  `GetParseState()` is racy (worker may still report `kState_Idle` before it dequeues). The test now
  waits on `job->WaitComplete()`. NOTE: `Reparse()` (full) is internally synchronous (it calls
  `WaitComplete()` itself), but `ReparseRegion` is not — callers must wait on the returned job.
- **flatten off-by-one** — `CreateEmptyBuffer()` seeds one empty line, so the buffer has 11 lines, not
  10. `test_textbuffer_flatten` now expresses expectations against `NumLines()`.

### Test-running gotchas (confirmed this session)
- Run from `cmake-build-debug/` (resources + `testfiles/` paths are cwd-relative), always
  `--sequential`, lib is `libutests.dylib`. No `timeout` on macOS — if you fear a hang, run the
  module as a background task rather than relying on `timeout`.
- Verified-green set (44 tests, 0 fail):
  `trun -m cpplang,jsonlang,cppnumbers,linelayout,timer,textbuffer --sequential libutests.dylib`.

### Remaining / deferred
- **Two baseline failures still red** (deliberately left this pass): `test_edtmodel_delete_text`,
  `test_edtmodel_text_linefunc` — real EditorModel logic, not test plumbing. Need a closer look.
