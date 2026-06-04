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

**Always run trun from the `cmake-build-debug/` directory** — the AssetLoader resolves paths relative to the working directory, so running from the project root picks up the system-installed assets (`/usr/share/goatedit/`) instead of the local build tree.

```sh
# Run all tests
cd cmake-build-debug && trun --sequential ./libutests.so

# Run a specific module
cd cmake-build-debug && trun -m textbuffer --sequential ./libutests.so

# Run a specific test case
cd cmake-build-debug && trun -m textbuffer -t insert --sequential ./libutests.so

# List available tests
cd cmake-build-debug && trun -l ./libutests.so
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
- Code should be written top - to - bottom, function A calling function B should have A before B in the source
- Declarations should almost always be ordered like: public -> protected -> private
- Types first, Functions second, variables at the end - don't mix functions and variables in same declaration scope
- Types declared should have their own public/protected/private block

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
- **Running tests**: always run from `cmake-build-debug/` (the AssetLoader resolves paths relative to
  the working directory — running from the project root silently picks up the system-installed assets
  in `/usr/share/goatedit/` instead of the local build tree). Use `--sequential` to disable forking
  for synchronized log output during dev. `-t` takes a list, supports wildcards, `!name` to exclude,
  and `-` meaning "all the rest" (e.g. `-t case1,case2,-` runs those first then the rest).
  Verified-green set (run from `cmake-build-debug/`):
  `trun -m clipboard,edtmodel,vnav,cpplang,jsonlang,cppnumbers,linelayout,dcoverlay,layout,jsengine --sequential ./libutests.so`.
  Note: trun forks per-test by DEFAULT (omit `--sequential`) — useful when a case may crash/segfault,
  so one bad case is isolated and the rest still report instead of aborting the run.
- **Do NOT run the full debug suite** — pre-existing failures in `test_textbuffer_{flatten,parsefull,
  parseregion,thparsefull,thparseregion}` confirmed present at baseline; plus the sqlite3-parse and
  thread/timer tests hang. Gating these is outstanding.

### Session 2026-06-04 (cont.) — resume point (read this first)
Completed the last deferred item (jsengine loadbuffer/listbuffers) and did two housekeeping
refactors. All work is **local commits on `main`, NOT pushed**. Build is clean (`goatedit` + `utests`);
verified-green set above all passes (now includes `jsengine`).

**Commits this session (oldest→newest):**
- `75609d3` REFACTOR: Split DrawViewContents into focused helpers
- `ff45ff7` FIX: Reinstate EditorAPI::LoadDocument (was LoadBuffer/TextBufferAPI)

**Files modified:** `src/Core/Views/EditorView.{h,cpp}` (DrawViewContents split),
`src/Core/API/EditorAPI.{h,cpp}` (LoadDocument), `src/Core/JSEngine/Modules/EditorAPIWrapper.{h,cpp}`
(LoadDocument wrapper + registration), `src/Plugins/Scripts/loadbuffer.js` (use LoadDocument),
`utests/test_jsengine.cpp` (un-stub loadbuffer/listbuffers), and this file.

**Patterns / decisions established (reuse these):**
- **Model stays logical, the VIEW translates at draw.** Cursor columns are CHAR INDICES everywhere in
  the model/`Selection` (copy/delete depend on it); anything drawn in screen space (caret, overlays,
  status col) converts via `Line::CharToVisualColumn(x, tabSize)` at render time. Don't push visual
  columns into the model. (This is why the overlay fix lives in `EditorView`, not `UpdateSelection`.)
- **Clamp/guard at the single chokepoint.** `*SplitView::SetSplitterPos` is the one place all resize
  paths funnel through → clamp there once rather than at each caller. Same spirit: `ClipBoardItem`
  resolves segments once in `ResolveSegments()` shared by both paste and line-count queries.
- **Split-view action routing:** stack views (`H/VStackView`) delegate all four resize actions up;
  each splitter handles ONE axis and must delegate the OTHER axis up its layout-handler chain
  (guarded → top-level is a no-op). A resize action's effect lives at the nearest enclosing splitter
  of the matching axis, not at the view that received the keystroke.
- **JS API layer pattern:** to expose a new C++ capability to JS scripts, add the method to `EditorAPI`
  (or the relevant `*API` class), mirror it in the corresponding `*APIWrapper` (same signature but
  wrapping in the `Wrapper::Ref` type), register it in `RegisterModule` via `dukglue_register_method`,
  and update the `.js` script. `NewDocument` / `LoadDocument` are the canonical templates.
- **trun working directory:** always run from `cmake-build-debug/` — AssetLoader resolves paths
  relative to CWD; running from project root picks up system assets, not the local build tree.
- **GUI verification recipe (SDL2 build, real `:0` display):** launch from `cmake-build-debug`, find
  the window by matching `xdotool getwindowpid` to the launched PID (WM title shows "gedit"), drive
  with `xdotool key --clearmodifiers <keys>`, capture with `xwd -id <wid> -out f.xwd` then
  `ffmpeg -i f.xwd f.png`, crop/upscale with ffmpeg `crop=...,scale=iw*N:ih*N:flags=neighbor`. Kill
  ONLY your launched PID afterwards; the user often has other goatedit/CLion windows open. Key actions:
  `Alt+s`/`Alt+w` height, `Alt+d`/`Alt+a` width, `Alt+e` focus editor (`Assets/Resources/Linux/
  default_keymap.yml`, `UINavigationModifier == Alt`).
- **Tests assert the SAFETY PROPERTY, not magic numbers** where possible (e.g. "splitter stays in
  `(0, contentExtent)` and both views keep positive extent"), and the nested layout tests assert the
  action actually *reached* the far splitter (`sp > initialSp`) so they catch the delegation bug, not
  just the clamp.

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

- **PasteFromClipboard cursor advance** — `ClipBoardItem::PasteToBuffer` now RETURNS the caret end
  `Point` (end of pasted text) instead of `void`, and a new `ClipBoardItem::GetPasteLineCount()`
  exposes the resolved segment count (independent of destination, so it can be queried before pasting).
  Both factored through a shared `ResolveSegments()`. `EditorModel::PasteFromClipboard` now derives
  `linesAdded = GetPasteLineCount() - 1` and uses it for the cursor advance, the reparse region, AND
  the undo range — picking `kClearAndAppend` (1 line) for an in-place single-line splice (`linesAdded
  == 0`) and `kDeleteBeforeInsert` (range = `linesAdded`) otherwise. Caret x is set from the returned
  end point + `CaptureWantedColumn`. Full-line-at-col-0 paste is unchanged (range == old `nLines`).
  `test_clipboard.cpp` now also asserts the returned end `Point` per case and the invariant
  `(linesAfter - linesBefore) == GetPasteLineCount() - 1`; 21/21 clipboard + 8/8 edtmodel green.

- **dcoverlay `IsInside` boundary** — RESOLVED (test-only). The end COLUMN is exclusive by design and
  is correct: overlays are built from a selection (`end` == cursor) and from search results (`start =
  cursor_x`, `end = cursor_x + length`), so exactly `length`/selection-width cells must light up. The
  end ROW (`end.y`) is inclusive (multi-line ranges cover their last row up to `end.x`). Replaced the
  FIXME/commented assertion in `test_dcoverlay.cpp` with explicit boundary asserts locking the
  convention in (start-col inclusive, end-col exclusive, start/end-row handling).
- **dc-overlay tab expansion (selection/search highlight)** — overlays render in SCREEN space (tabs
  expanded) but the model stores columns as CHARACTER INDICES; `EditorView::DrawViewContents` built
  the selection/search overlays straight from those char-index x's, so any tab before the marked text
  pushed the highlight left of the glyphs. Fixed by expanding each overlay endpoint to a VISUAL column
  via `Line::CharToVisualColumn(x, tabSize)` against the line it sits on (start.x vs line[start.y],
  end.x vs line[end.y]; search `cursor_x` and `cursor_x+length` vs line[idxLine]). This mirrors the
  caret's existing handling in `SetWindowCursor` (model stays logical, the VIEW translates at draw).
  The `Selection` stays in buffer/char coords (copy/delete depend on that). Primitive is well-tested
  (`test_linelayout` char2vis/vis2char/roundtrip); the view-render path itself isn't unit-tested.

- **Split-view resize bounds (status bar off-screen / negative view extent)** — TWO bugs. Real app
  layout (main.cpp): `RootView > HSplitViewStatus[ upper = VSplitView[ workspace | VStack>HStack>editor ],
  lower = terminal ]`. The status line is drawn ON the HSplit splitter row (`HSplitViewStatus::DrawSplitter`
  at `GetSplitRow() == splitterPos`). `kActionIncreaseViewHeight` from the editor bubbles up the
  layout-handler chain (HStack/VStack delegate all four actions up).
  (1) **Delegation gap** — the splitters are the chain terminators: `HSplitView` overrides only the
  *height* actions, `VSplitView` only the *width* actions. So a height action arriving at the
  `VSplitView` fell through to `ViewBase::OnActionIncreaseHeight` → `SetHeight(h+1)` on the VSplit
  *itself* (unbounded) and NEVER reached the HSplit that owns the divide — the real cause of the status
  bar escaping. Fixed: `VSplitView` now delegates height actions UP to its layout handler, `HSplitView`
  delegates width actions UP (both guarded for a null top-level handler => no-op).
  (2) **No clamp** — once it reaches the right splitter, `SetSplitterPos` (the single chokepoint for
  every resize path) now clamps via `ClampSplitterPos` to `[kMinViewExtent(5), extent-5]` (degenerate
  fallback `[0, extent-1]`). `HSplitView::AdjustHeight` delegates to it; increase/decrease handlers
  record `splitterPosBeforeReset` from the *clamped* `GetSplitterPos()` so `RestoreContentHeight` can't
  reintroduce an out-of-range value. Tests: `test_layout_{height,width}_{max,min}` (direct splitter
  clamp) + `test_layout_nested_{height,width}` (fire the action from a view DEEP inside the *other*
  splitter and assert it bubbles up and clamps — pre-fix the divide stayed at its init pos). Verified
  in the GUI (Alt+s/Alt+w ×140 each): status bar stays on-screen at max, editor clamps to a few rows
  at min, no invert. Headless screen 100 => clamps to 5/95.

- **DrawViewContents refactor** — split into three functions in `src/Core/Views/EditorView.cpp`:
  `DrawViewContents` (top-level driver), `DrawSearchResultOverlays`, `DrawSelectionOverlay`. Functions
  ordered caller-before-callee so the file reads top-to-bottom. Declarations added to `EditorView.h`
  private section. Commit `75609d3`.

- **jsengine LoadDocument** — `EditorAPI::LoadBuffer` (old `TextBufferAPI`-based) was commented out;
  reinstated as `LoadDocument(filename) -> DocumentAPI::Ref` following the `NewDocument` pattern
  (`workspace->NewModelWithFileRef` + `OpenModelFromWorkspace` to load from disk and activate).
  Mirrored in `EditorAPIWrapper::LoadDocument` and registered in `RegisterModule`. Updated
  `loadbuffer.js` to call `Editor.LoadDocument` (single call replaces old `LoadBuffer`+`SetActiveBuffer`
  two-step). Un-stubbed `test_jsengine_loadbuffer` and `test_jsengine_listbuffers`; both green.
  Commit `ff45ff7`.

### Remaining / deferred
Nothing — deferred list is empty.

### Untracked (intentionally never committed)
`.idea/`, `cmake-build-release/`, `syntax_problem.cpp` — left alone every commit this session.
