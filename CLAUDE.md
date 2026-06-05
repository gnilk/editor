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
  `trun -m clipboard,edtmodel,vnav,cpplang,jsonlang,cppnumbers,linelayout,dcoverlay,layout,jsengine,workspace,terminalscreen,vtermparser --sequential ./libutests.so`.
  Note: trun forks per-test by DEFAULT (omit `--sequential`) — useful when a case may crash/segfault,
  so one bad case is isolated and the rest still report instead of aborting the run.
- **Do NOT run the full debug suite** — the sqlite3-parse test is intentionally excluded: ~1s in
  release but 13-15s in debug (syntax highlighter over a large file, no optimizations). Thread/timer
  tests are also excluded. All `test_textbuffer_*` cases pass.

### Session 2026-06-05 (cont.) — resume point (read this first)
TerminalScreen Step 3 (vi/full-screen app support) + shell mode rendering fixes.
All commits pushed to `main`. Build is clean; verified-green set above all passes.

**Commits this session (oldest→newest):**
- `f1fb2fa` NEW: Added SetInitialSplitterPos to HSplitView (user commit)
- `aca54fe` FEAT: TerminalScreen Step 3 — vi/full-screen app support
- `ffaa881` FIX: Shell mode rendering — bottom-anchor, WriteLine alignment, 256-colour

**Files modified this session:**
`src/Core/unix/Shell.{h,cpp}`, `src/Core/VTermParser.{h,cpp}`,
`src/Core/TerminalScreen.{h,cpp}`, `src/Core/Controllers/TerminalController.{h,cpp}`,
`src/Core/Views/TerminalView.cpp`.

#### What Step 3 delivered (commit `aca54fe`)
1. **Key passthrough in alt-screen** — `HandleKeyPressAltScreen` sends all printable chars (incl.
   Ctrl+key combos) and special keys directly to the pty as escape sequences. `ForwardActionToShell`
   covers actions that slip through the key handler path (Return → `\r`; navigation actions →
   sequences; all other actions swallowed so editor-level behaviour never fires inside vi).
   `TerminalView::OnAction` routes directly to `ForwardActionToShell` when `IsAltScreen()` — the
   old `CommitLine` path is bypassed entirely.
2. **Cursor key app mode** (`ESC[?1h/l`) — `cursorKeyAppMode` flag in `TerminalController`; arrows
   send `ESC OA/B/C/D` in app mode, `ESC[A/B/C/D` in normal mode.
3. **Terminal responses** — `CSI 6n` (DSR) replies `ESC[row;colR`; `CSI c` (Primary DA) replies
   `ESC[?1;2c`. Vi blocks waiting for these during init; `Shell::WriteBytes(const std::string&)`
   added to send raw byte strings to the pty master.
4. **New VT sequences** — `ESC M` (reverse index: scroll region down or move cursor up), `CSI L/M`
   (insert/delete line with scroll-region awareness), `CSI @/P` (insert/delete char), `CSI ?25h/l`
   (cursor show/hide consumed silently instead of hitting the unhandled log).
5. **`kCursorShow`/`kCursorHide`/`kCursorKeyModeApp`/`kCursorKeyModeNormal`/`kDeviceStatusReport`/
   `kPrimaryDA`/`kReverseIndex`/`kInsertLine`/`kDeleteLine`/`kInsertChar`/`kDeleteChar`** added to
   `VTermParser::kAnsiCmd` and wired through `ApplyCommand`.

#### What the shell rendering fix delivered (commit `ffaa881`)
1. **Bottom-anchor** (`TerminalView::DrawViewContents`) — `blankRows = max(0, (viewHeight-1) -
   totalHistory)` added before the history loop. When content is shorter than the view, blank rows
   appear at the TOP and content is anchored to the bottom above the prompt. Previously the JS
   bootstrap goat art appeared at row 0 with blank space below it.
2. **`WriteLine` alignment** — if `cursor.x > 0` when `Console.WriteLine` is called (shell prompt
   may have raced in before the JS bootstrap), a `CR+LF` is emitted first so output always starts at
   column 0 on a fresh line.
3. **256-colour SGR** — `ESC[38;5;Nm` / `ESC[48;5;Nm` emit new `kSetForeground256` / `kSetBackground256`
   commands resolved against `FG_BG_256[]`. Bright-colour codes `ESC[90-97m` / `ESC[100-107m` routed
   through the same path (palette indices 8–15) instead of being silently dropped. SGR loop is now
   index-based (was range-for) to support multi-param look-ahead for `38;5;N`.

#### Debug logging state
`Assets/Resources/config.yml` controls the logger `logsink: filesink`.
Log is at `~/.local/state/goatedit.log`. 

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
- **ANSI/VT parsing gotchas:** CSI parameter MODIFIER bytes `< = > ?` (0x3c–0x3f) sit inside the
  parameter byte range (0x30–0x3f) but are NOT numeric — never feed them to `stoi`; skip during param
  collection (`?` additionally flags a private sequence). Guard every grid mutation against a 0×0
  screen (sequences arrive before the first `Resize`). When debugging escape streams, dump the RAW
  bytes in readable form BEFORE parsing — pasting terminal data into chat mangles the escapes, so the
  log is the source of truth.
- **One model flag, view picks the render path:** `TerminalScreen` carries a single `isAltScreen` flag;
  `TerminalView::DrawViewContents` branches on `IsAltScreen()` (full-grid vs shell history+input). The
  model never grows two code paths — same spirit as "model stays logical, the view translates at draw."

### Recently completed (carry-forward state)
- **README rewrite (contributor-focused)** — replaced the old dev-diary `readme.md` with a pitch
  aimed at attracting contributors: hero shot + "why you might like it" selling points, a quick tour
  using the three new 2026-06-05 screenshots (`GoatEdit_2026-06-05.png` hero, `GoatEdit_term_*` build,
  `GoatEdit_Search_*` quick-command), modern CMake build (SDL3 default / SDL2 fallback flags), a
  dependency table (system vs auto-`ext/` vs vendored), a project-layout map, and a "Want to hack on
  it?" section with sized entry points. **NCurses fully removed** from the README (user removed it from
  the build) — the stated direction is a purpose-built modern-terminal backend with no NCurses legacy,
  explicitly tied to the bucket-list goal of running the editor over SSH (deferred, later session).
  Three new screenshots staged with the README (untracked before). `vi`-in-terminal bucket-list item
  is done; remote/SSH is the next big one.

- **TerminalScreen Step 3 — vi/full-screen app support** — key passthrough in alt-screen
  (`HandleKeyPressAltScreen` + `ForwardActionToShell`), cursor key app mode (`?1h/l`), terminal
  query responses (DSR `6n`, Primary DA `c`), new VT ops (`ESC M`, `CSI L/M/@/P`, `?25h/l`).
  vi renders and is usable. `Shell::WriteBytes` added. `cursorKeyAppMode` flag in
  `TerminalController`. Commit `aca54fe`.

- **Shell mode rendering fixes** — bottom-anchor (`blankRows` offset so content sits above the
  prompt rather than at row 0), `WriteLine` CR+LF guard when mid-line, 256-colour SGR (`38;5;N`,
  `48;5;N`, `90-97`, `100-107`) via `kSetForeground256`/`kSetBackground256` + `FG_BG_256[]`.
  Commit `ffaa881`.

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

- **Test suite audit** — all `test_textbuffer_*` pass (stale failure note removed). sqlite3-parse is
  intentionally excluded (~1s release, 13-15s debug). `test_timer`/`test_timer_exit` are pre/post
  module hooks, intentionally empty. Logger tests are obsolete (external library has its own suite).
  `test_workspace_fileref` was pointing at a deleted file (`test_src2.cpp`) — switched to
  `testfiles/ConvertUTF.cpp`. `test_workspace_openfolder` had a commented-out assertion replaced with
  real checks. `Workspace::Node::LoadData` FIXME clarified: returning `true` for non-existent paths is
  intentional — new documents have `kBuffer_FileRef` state (name assigned, not on disk yet) and
  `LoadData` must not fail for them. Commit `fa52c94`.

- **test_vnav_pageup** — was a stub. Implemented 4 cases symmetric with `test_vnav_pagedown`:
  already-at-top (no-op), near-top clips to first line, exactly-one-page-in returns to top
  (content-first), mid-buffer keeps caret screen row. Commit `3763816`.

- **TerminalScreen Step 1** — replaced `TerminalController` `historyBuffer + lastLine` model with a
  `TerminalScreen` cell grid (`cols × rows`, per-cell fg/bg/attrs, scrollback, pen state). Wired
  `HandleTerminalData` to write directly to the grid — eliminates idxString position-tracking.
  `WriteLine` uses CR+NL so each line starts at col 0. `TerminalView::DrawViewContents` renders
  scrollback+grid history via `DrawScreenRow` (run-length color batching), cursor row composed with
  inputLine at bottom. `Resize` is idempotent when dimensions match so view activation doesn't wipe
  content. 11 tests in `test_terminalscreen.cpp`. Commit `5d15c9d`.

- **TerminalScreen Step 2** — `VTermParser` extended with full CSI dispatch: cursor movement
  (A/B/C/D/H), erase (J/K all modes), scroll region (r), cursor save/restore (s/u + ESC 7/8),
  alternate screen (?1049h/l). `TerminalScreen` gains `EraseInLine(mode)`, `EraseInDisplay(mode)`,
  `SaveCursorPos`/`RestoreCursorPos`, `SetScrollRegion` (NewLine respects region boundary).
  `ApplyCommand` wired for all new commands; `kEnterAltScreen`/`kLeaveAltScreen` functional via
  `SaveScreen`/`RestoreScreen`. Cursor row now rendered with `DrawScreenRow` so prompt colors are
  preserved (was using `LineRender` which dropped per-cell color). 6 new vtermparser tests,
  3 new terminalscreen tests. Commit `ab01f5d`.

- **Shell PTY simplification** — the original `Shell.cpp` called `forkpty` then immediately
  `dup2`'d the child's stdin/stdout/stderr to regular pipes, leaving `amaster` completely unused.
  Consequences: `isatty()` false for all child processes (no auto-colors), ONLCR not applied
  (required simulation hack), 3 pipe pairs + polling 2 fds, separate stderr callback, leaked debug
  FILE*. New design: child does no `dup2` — pty slave IS its stdio. Parent reads/writes via
  `amaster` only (poll 1 fd). `Stream` enum removed, `SetStderrDelegate` removed. `SetWindowSize`
  added; called from `TerminalController::Resize` so vi/less see correct dimensions. ONLCR
  simulation removed from `HandleTerminalData`. Result: 306 lines deleted, 110 added, auto-colors
  work. Commit `7e96e93`.

### Remaining / deferred
- **vi polish** — vi renders and is usable for a showcase. Still missing for full correctness:
  origin mode `ESC[?6h/l`, `test_terminalscreen_savestate` under-tests `SaveScreen` (should assert
  clean-alt-screen behaviour: blank grid, cleared scrollback, cursor home, reset scroll region).
  Bracketed-paste `?2004`, focus `?1004` modes are consumed but not acted on (low priority).
- **Shell mode rendering** — the `promptLen = cursor.x` heuristic works when the cursor sits at the
  end of the prompt, but breaks during active command output. Good enough for now; a proper fix would
  track prompt boundaries explicitly (non-trivial).
- **Standard-color SGR mapping** — `kSetForegroundColor` still uses `(idx & 7) + 8` to map 30-37 to
  bright palette entries. This is intentional (bold=bright xterm convention) but might want a theme
  toggle later.
- **`.gitignore`** — add `claude.sessions.md` (user's private scratch file, currently tracked but
  never committed with content).

### Untracked (intentionally never committed)
`.idea/`, `cmake-build-release/`, `syntax_problem.cpp` — left alone every commit this session.
