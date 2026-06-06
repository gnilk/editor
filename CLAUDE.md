# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

**GoatEdit** — a personal text/code editor written in C++20. Runs on Linux and macOS. Renders through
an SDL backend (SDL2 and SDL3 sources both exist; **the locally-built backend is SDL2** — see Build).
NCurses sources still exist in-tree but are **out of the build** (the stated direction is a
purpose-built modern-terminal backend, eventually able to run over SSH). Embeds a JavaScript plugin
engine (Duktape) for scripting. The primary executable target is `goatedit`.

## Build

CMake 3.22+ is required. Build is configured per-platform with optional SDL2/SDL3 flags.

```sh
# Install system deps (Linux)
sudo apt-get install -y libyaml-cpp-dev libncurses-dev libsdl2-dev
./setup_deps.sh       # clones ext/ source deps (json, gnklog, dukglue, fmt)

# Configure. NOTE: the CMake default flag is SDL3-on, but the actual local build dirs on this
# machine (cmake-build-debug AND cmake-build-release) are configured SDL2 ON / SDL3 OFF, and the
# running binary is the SDL2 backend (gedit::SDL2::*). Keep using the SDL2 flags below to match.
cmake -B ./cmake-build-debug -DCMAKE_BUILD_TYPE=Debug -DGEDIT_BUILD_SDL3=OFF -DGEDIT_BUILD_SDL2=ON

# Build the main binary
cmake --build ./cmake-build-debug --config Debug --target goatedit -j

# Build the unit test shared library
cmake --build ./cmake-build-debug --config Debug --target utests -j
```

CMake auto-clones missing `ext/` dependencies on first configure (except duktape, which is pre-included in `src/ext/duktape-2.7.0`).

## Running Tests

Tests use the **trun** (TestRunner v3) framework. The test target builds `utests` as a shared library; `trun` loads and runs it.

Always use the `--sequential` parameter when running tests during development cycles. This removes
forking from unit-tests, which may otherwise cause log output to be unsynchronized. (Without
`--sequential`, trun forks per-test — useful when a case may crash/segfault, so one bad case is
isolated and the rest still report instead of aborting the run.)

The `-m` (module) and `-t` (test) options both take lists and wildcards; prefix `!` discards a
test/module, and the special token `-` means "everything else". This controls execution order:
`-t case1,case2,-,!case5` runs case1+case2 first, then all others except case5; `-t parse*` runs all
cases starting with `parse`. **Test-case names drop the `test_<module>_` prefix** — e.g. the function
`test_terminalscreen_resize` is selected with `-m terminalscreen -t resize`.

**Always run trun from the `cmake-build-debug/` directory** — the AssetLoader resolves paths relative to the working directory, so running from the project root picks up the system-installed assets (`/usr/share/goatedit/`) instead of the local build tree.

```sh
cd cmake-build-debug && trun --sequential ./libutests.so            # all
cd cmake-build-debug && trun -m textbuffer --sequential ./libutests.so   # one module
cd cmake-build-debug && trun -m textbuffer -t insert --sequential ./libutests.so  # one case
cd cmake-build-debug && trun -l ./libutests.so                     # list
```

Test source files live in `utests/`. Each `test_<module>.cpp` corresponds to one module; `test_main.cpp` initializes the editor singleton for all tests. Test functions are `extern "C"` (or `DLL_EXPORT`) and discovered by exported symbol name — adding a new case is just a new function (+ a forward declaration where the file uses one).

**Verified-green set** (run from `cmake-build-debug/`):
`trun -m clipboard,edtmodel,vnav,cpplang,jsonlang,cppnumbers,linelayout,dcoverlay,layout,jsengine,workspace,terminalscreen,vtermparser,keymapping --sequential ./libutests.so`

**Do NOT run the full debug suite** — the sqlite3-parse test is intentionally excluded (~1s release, 13–15s debug: syntax highlighter over a large file, no optimizations). `test_timer`/`test_timer_exit` are pre/post module hooks (intentionally empty); logger tests are obsolete (external lib has its own suite).

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

Real app layout (`main.cpp`): `RootView > HSplitViewStatus[ upper = VSplitView[ workspace | VStack>HStack>editor ], lower = terminal ]`. The status line is drawn ON the HSplit splitter row (`HSplitViewStatus::DrawSplitter` at `GetSplitRow() == splitterPos`).

### Controllers (`src/Core/Controllers/`)
Controllers hold input-handling logic decoupled from views:
- `EditController` — text editing actions
- `TerminalController` — terminal/command mode
- `QuickCommandController` — vi-like quick command overlay

### Actions & Key Mapping
`kAction` enum (`src/Core/Action.h`) defines all editor actions. `KeyMapping` (`src/Core/KeyMapping.h`) maps key presses → actions, loaded from config YAML. `KeypressAndActionHandler` is a mixin that views/controllers inherit to declare which actions they handle. Keymaps support `inherit: <parent>` (resolved inside `KeyMapping`, multi-level, see below).

### Rendering Backends (`src/Core/Graphics/SDL2/`, `src/Core/Graphics/SDL3/`)
Each backend implements `ScreenBase`, `NativeWindow`, and `DrawContext` interfaces. **SDL2 is the backend actually built and run locally** (the CLAUDE.md previously claimed SDL3-primary — that's stale). SDL3 sources are kept in parallel. NCurses sources exist but are out of the build. `SDLFontManager` uses stb_truetype (`ext/stbttf.h`) for TTF rendering. The editor renders into a character grid: `SDLScreen::ComputeScalingFactors` derives `rows`/`cols` from pixel size and font metrics; views lay out in character cells.

### Language / Syntax Highlighting (`src/Core/Language/`)
`LanguageBase` is the interface. `LangLineTokenizer` uses a stack-based tokenizer configured per-language. Concrete languages: `CPPLanguage`, `JSONLanguage`, `MakeBuildLang`, `DefaultLanguage`. Language selection is driven by file extension via `Config`.

### Plugin / Scripting (`src/Core/JSEngine/`)
Embeds Duktape 2.7.0 (`src/ext/duktape-2.7.0/`). `JSPluginEngine` loads `.js` files from the `resources/Plugins/` directory. JS API wrappers live in `src/Core/JSEngine/Modules/` and expose `DocumentAPI`, `EditorAPI`, `ThemeAPI`, `ViewAPI`, etc. Plugin JS source lives in `src/Plugins/`.

### Configuration (`src/Core/Config/`)
YAML-based config loaded by `Config` singleton. `ConfigNode` provides typed access. `Theme` loads `.theme.yml` files. Runtime defaults are in `Assets/Resources/config.yml`. A **user config** is merged at startup from `~/.local/share/goatedit/config.yml` (e.g. terminal shell/bootstrap). Logger sink is `logsink: filesink` → `~/.local/state/goatedit.log` (only bootstrap lines reliably land there; SDL/runtime debug logs are flaky — prefer stderr instrumentation or ASan over the logfile when debugging).

### Terminal Emulation (`src/Core/TerminalScreen.{h,cpp}`, `VTermParser.{h,cpp}`)
`TerminalScreen` is a `cols × rows` cell grid (per-cell fg/bg/attrs) + scrollback + pen state + a single `isAltScreen` flag. `VTermParser` parses the pty byte stream into a command list; `TerminalController::HandleAnsiCmd` applies them to the grid. The pty is read on a **separate thread** (`Shell::ConsumePty`) that mutates `TerminalScreen` under `TerminalController::screenLock` — **every path that mutates the grid (including `Resize`) must hold that lock.**

### Platform-Specific
- Linux: `src/Core/Linux/LinuxFolderMonitor.cpp`
- macOS: `src/Core/macOS/MacOSKBEmulator.cpp`, `MacOSFolderMonitor.cpp`
- Unix shared: `src/Core/unix/Shell.cpp` (pty/shell spawning; reads via `amaster` only, pty slave IS the child stdio so `isatty()` is true and auto-colors work)

## Key Conventions

- All types use a `Ref = std::shared_ptr<T>` alias defined inside the class.
- `Logger` from `ext/gnklog` is used throughout; include `"logger.h"`.
- String literals for unicode content use `U"..."` (`char32_t`); `UnicodeHelper` handles conversions.
- `fmt` (fmtlib) is the string formatting library.
- Platform macros: `GEDIT_LINUX`, `GEDIT_MACOS`, `GEDIT_USE_SDL3`, `GEDIT_USE_SDL2`.

## Coding Standards

### General
- Always use curly braces for all control flow, even single-line bodies
- C++17/20 standard throughout
- RAII everywhere, no raw owning pointers
- Code reads top-to-bottom: a caller should appear before its callee in the source; entry point first
- Declarations ordered public → protected → private; each visibility its own block
- Types first, functions second, variables last — don't mix functions and variables in the same scope

### Formatting
- 4-space indentation, no tabs; K&R braces (opening brace on the same line); one blank line between methods

### Naming
- PascalCase for classes/structs and methods: `GraphicsDevice`, `InitDevice()`
- UPPER_SNAKE_CASE for constants: `MAX_BUFFER_SIZE`
- camelCase for variables; never an `m_` member prefix; convey intent (`bool isSomething`)

### C++ Specifics
- Prefer `nullptr` over `NULL`; `auto` only when the type is obvious; mark single-arg ctors `explicit`; prefer range-based for

### Test infrastructure / conventions
- **Test fixtures**: `Assets/testfiles/` (contains `ConvertUTF.cpp` — mixed tabs+spaces, a good
  tab-render fixture). Copied to `cmake-build-debug/testfiles/` (NOT `EDITOR_ASSET_DIR` — dev-only,
  not redistributable). `utests` depends on the `testfiles` copy target.
- **Tests assert the SAFETY PROPERTY, not magic numbers** where possible (e.g. "splitter stays in
  `(0, contentExtent)` and both views keep positive extent"; "scrollback stays empty when content
  still fits").
- **Reproduce-before-fix discipline (established 2026-06-06):** for a non-obvious bug, first write a
  *discriminating* unit test that FAILS on the current code (proving the mechanism), then fix and watch
  it pass. Used for the terminal-resize content-drift and splitter-collapse bugs.

---

## Established patterns (reuse these)

- **Model stays logical, the VIEW translates at draw.** Cursor columns are CHAR INDICES everywhere in
  the model/`Selection` (copy/delete depend on it); anything drawn in screen space (caret, overlays,
  status col) converts via `Line::CharToVisualColumn(x, tabSize)` at render time. Don't push visual
  columns into the model.
- **One model flag, the view picks the render path.** `TerminalScreen` carries a single `isAltScreen`
  flag; `TerminalView::DrawViewContents` branches on it (full-grid vs shell history+input). The model
  never grows two code paths.
- **Two orthogonal flags, not a nested state machine (TerminalController).** `screen.IsAltScreen()`
  (a *model* flag, flipped by ANSI alt-screen escapes) and `doesShellOwnLineEditing` (a *controller*
  flag, set on Tab/ShellCompletion) are independent. `HandleKeyPress` checks them in priority order:
  (1) alt-screen → forward raw to pty, (2) shell-owns-line → forward raw + Ctrl-C escape hatch,
  (3) local edit. Both forward paths share `ForwardKeyPressToShell`.
- **`splitterPos` is the AUTHORITY; child extents are DERIVED from it — never the reverse.** On a
  window resize a child view gets independently relaid-out to fill its parent; adopting that child's
  live size as the new `splitterPos` misreads it as a deliberate splitter drag and ratchets the divider
  into an edge. `*SplitView::ReInitView` keeps `splitterPos`, clamped into the new content area, and
  skips the transient height/width-0 pass during the resize traversal.
- **Clamp/guard at the single chokepoint.** `*SplitView::SetSplitterPos` is the one place all resize
  paths funnel through → clamp there once (`ClampSplitterPos`, `[kMinViewExtent(5), extent-5]`).
- **Split-view action routing:** stack views (`H/VStackView`) delegate all four resize actions up; each
  splitter handles ONE axis and must delegate the OTHER axis up its layout-handler chain (guarded →
  top-level is a no-op). A resize action's effect lives at the nearest enclosing splitter of the
  matching axis, not at the view that received the keystroke.
- **Terminal content is cursor-anchored.** In shell mode the live content is grid rows `[0..cursor.y]`;
  rows below the cursor are blank padding the view never shows. Anything that preserves content across
  a resize must anchor to the cursor (`contentRows = min(gridRows, cursor.y+1)`), not blindly keep the
  bottom N rows.
- **A threaded model's dimension fields must track its buffer.** `TerminalScreen::cols/rows` must equal
  the real grid dimensions at all times, because the pty-reader thread's grid mutators guard on
  `cols==0||rows==0`. Leaving them non-zero/negative while the grid is empty lets a write land in freed
  memory (heap UAF). When emptying the grid, zero the dimensions too.
- **ANSI/VT parsing gotchas:** CSI parameter MODIFIER bytes `< = > ?` (0x3c–0x3f) sit in the parameter
  byte range but are NOT numeric — never `stoi` them; skip during param collection (`?` also flags a
  private sequence). Guard every grid mutation against a 0×0 screen (sequences arrive before the first
  `Resize`).
- **JS API layer pattern:** to expose a new C++ capability to JS, add the method to `EditorAPI` (or the
  relevant `*API`), mirror it in the `*APIWrapper` (wrapping in the `Wrapper::Ref` type), register it in
  `RegisterModule` via `dukglue_register_method`, and update the `.js` script. `NewDocument` /
  `LoadDocument` are the canonical templates.
- **Keymap inheritance:** resolved inside `KeyMapping`, not the caller. `KeyMapping::Initialize` parses
  the keymap's own bindings first, then `ResolveInheritance` walks the `inherit` chain iteratively
  (cycle-safe via a visited set), appending each ancestor's bindings. First-match wins, so the child
  overrides its parents. `KeyMapping::LoadKeymapConfig(name)` is the single place keymap assets load.

## Debugging / verification recipes

- **GUI verification (SDL2 build, real `:0` display):** launch `goatedit` from `cmake-build-debug` with
  `DISPLAY=:0`, record the PID, find the window with `xdotool search --pid <PID>`. Drive with
  `xdotool windowsize <WID> W H` (resize) or `xdotool key --clearmodifiers <keys>`. Capture with
  `xwd -id <WID> -out f.xwd` then `ffmpeg -i f.xwd f.png`. **Kill ONLY your launched PID** — the user
  often has other goatedit/CLion windows open. Key actions: `Alt+s`/`Alt+w` height, `Alt+d`/`Alt+a`
  width, `Alt+e` focus editor (`Assets/Resources/Linux/default_keymap.yml`, `UINavigation == Alt`).
  NOTE: programmatic `windowsize` triggers the same SDL resize events as a mouse corner-drag, but to
  reproduce a *continuous* drag, send many small incremental `windowsize` steps (both axes for a
  corner). Some bugs only show when the pty/terminal is actively producing output during the resize, so
  launch with a folder (`./goatedit .`) so the terminal bootstrap/banner is live.
- **AddressSanitizer (the right tool for heap corruption — established 2026-06-06):** there is an
  untracked `cmake-build-asan/` build configured with `-fsanitize=address -fno-omit-frame-pointer`
  (SDL2, ccache). Rebuild target `goatedit`, run with
  `ASAN_OPTIONS="abort_on_error=0:halt_on_error=1:detect_leaks=0:log_path=/tmp/goat_asan"`, reproduce,
  then read `/tmp/goat_asan.<pid>`. ASan reports the exact free + write stacks; far better than chasing
  a malloc-time abort. It found the terminal-resize UAF in one run.
- **Instrument-to-confirm:** before fixing a non-obvious bug, add a temporary `fprintf(stderr, ...)`
  diagnostic, do an incremental build, reproduce, read the trajectory, then REMOVE it. Used this session
  to disprove a wrong hypothesis (SDL `RenderReadPixels` overflow) and to capture the splitter ratchet.

---

## Session 2026-06-06 — resume point (read this first)

Five commits, all pushed to `main`. Build clean; verified-green set passes. Theme: a refactor pass on
KeyMapping + TerminalController, then a hunt through **window-resize bugs** (three found & fixed, each
with a regression test and live GUI verification). User mentioned there may be "a few more resize bugs"
beyond these three.

**Commits this session (oldest→newest):**
- `ba4ab93` REFACTOR: Move keymap inheritance resolution into KeyMapping
- `9155ae5` REFACTOR: Clarify TerminalController keypress-handling state
- `f80844a` FIX: Heap use-after-free when terminal pane resized to zero
- `24fc535` FIX: Splitter collapse on window (corner) resize
- `a2c8c17` FIX: Terminal content drift when resizing back and forth

**Files modified this session:**
`src/Core/KeyMapping.{h,cpp}`, `src/Core/Editor.cpp`, `src/Core/Controllers/TerminalController.{h,cpp}`,
`src/Core/Views/TerminalView.cpp`, `src/Core/TerminalScreen.cpp`, `src/Core/Views/HSplitView.h`,
`src/Core/Views/VSplitView.h`, `utests/test_keymapping.cpp`, `utests/test_terminalscreen.cpp`,
`utests/test_layout.cpp`.

### 1. KeyMapping inheritance refactor (`ba4ab93`)
Inheritance was handled in `Editor::GetKeyMapping` and only resolved ONE level. Moved into `KeyMapping`:
- `KeyMapping::LoadKeymapConfig(name)` — static; the single place keymap assets are loaded from
  (OS-specific lookup via the `linux`/`macos` config section). Used by `Editor::GetKeyMapping` AND by
  parent resolution.
- `Initialize` parses own bindings first, then `ResolveInheritance` walks the `inherit` chain
  **iteratively** with a `visited` set (cycle-safe), appending each ancestor's bindings. Child parsed
  first + first-match lookup ⇒ child overrides parent, nearer ancestor over more distant.
- Removed the public `Inherit(Ref)`; `Editor::GetKeyMapping` slimmed to `LoadKeymapConfig → Create →
  cache`.
- `test_keymapping_inherit` uses real distribution assets (`terminal_keymap` inherits `default_keymap`;
  asserts both inheritance and child-override of `Tab`). **Deliberately NOT guarded by `GEDIT_LINUX`** —
  it should FAIL loudly when porting to a platform where `terminal_keymap` doesn't exist, rather than
  silently skip. (Only `Linux/terminal_keymap.yml` uses `inherit:` today.)

### 2. TerminalController readability refactor (`9155ae5`) — no behaviour change
- `ApplyCommand` → `HandleAnsiCmd` (it dispatches parsed ANSI/VT commands).
- `HandleKeyPressAltScreen` → `ForwardKeyPressToShell` — it encodes a key as raw pty bytes and is used
  by BOTH passthrough paths (alt-screen and shell-owned), so the alt-screen-specific name was
  misleading. Pairs with `ForwardActionToShell`.
- `HandleKeyPress` restructured with a numbered 3-path comment block (alt-screen / shell-owned /
  local-edit) making the precedence and the two orthogonal dimensions explicit.
- Binary `enum TermMode` → `bool doesShellOwnLineEditing` (accessor `DoesShellOwnLineEditing()`).
  Line-scoped name, no raw/bypass confusion; documented that it's independent of — not a sub-state of —
  `screen.IsAltScreen()`.

### 3. Terminal pane resize → heap use-after-free (`f80844a`) — was the crash AND garbage-render
Root-caused with **AddressSanitizer**. Shrinking the window so the terminal pane is squeezed to zero
(or negative) rows/cols hit `TerminalScreen::Resize`'s `grid.clear()` branch — but `cols`/`rows` were
assigned the raw (0/negative) values *before* the guard, so they outlived the freed grid. The
pty-reader thread's `EraseInLine` (guard `cols==0||rows==0`) slipped a *negative* value past and wrote
into the freed/empty grid ⇒ heap corruption, surfacing two ways (garbage glyphs, malloc abort).
**Fix:** in the degenerate branch force `cols = rows = 0`; only assign real dimensions on the valid
path, so every guard fires. `test_terminalscreen_resize_degenerate` resizes to 0 and -3, fires every
pty-reachable mutator (safe no-op), then regrows.

### 4. Splitter collapse on window (corner) resize (`24fc535`)
Grabbing the corner collapsed the HSplit divider to the bottom — editor filled the screen, terminal
went negative, status bar (drawn on the splitter row) vanished. `HSplitView::ReInitView` (and
identically `VSplitView`) **adopted a child view's live height/width as the new `splitterPos`**; during
the resize traversal the upper/left child gets relaid-out to fill, so the splitter ratcheted to the
edge and stayed there (each frame re-adopted the near-full extent). The assignment also bypassed
`ClampSplitterPos`. **Fix:** `splitterPos` is the authority — `ReInitView` keeps it, clamped, and skips
the transient size-0 pass; removed the `knownUpper/LowerHeight` + `knownLeft/RightWidth` tracking and
the adoption block. **Applied to both axes** (VSplitView had the same latent bug). Tests:
`test_layout_hsplit_resize_stability`, `test_layout_vsplit_resize_stability` (force a child to fill,
re-init ×5, assert the splitter never adopts it).

### 5. Terminal content drift on back-and-forth resize (`a2c8c17`)
Resizing up/down repeatedly injected blank lines / drifted the terminal content. `TerminalScreen::Resize`
kept the bottom `newRows` grid rows regardless of the cursor — but content lives in rows `[0..cursor.y]`
and rows below are blank padding, so a short prompt with padding below it would, on shrink, scroll the
REAL content into scrollback and keep the blanks. **Fix:** anchor preservation to the cursor —
`contentRows = isAltScreen ? gridRows : min(gridRows, cursor.y + 1)` — so content spills to scrollback
only when it actually overflows. Gated on `isAltScreen` so full-screen apps (which repaint on resize)
keep the previous behaviour exactly. `test_terminalscreen_resize_keeps_content` **failed on the old
code** (proving it), passes after.

---

## Earlier work (completed, in git — kept as a one-line index)

- README rewrite (contributor-focused pitch; NCurses removed). 2026-06-05 screenshots staged.
- TerminalScreen Steps 1–3 + shell rendering: cell-grid model replacing the old historyBuffer
  (`5d15c9d`); full CSI dispatch — cursor/erase/scroll-region/save-restore/alt-screen (`ab01f5d`);
  vi/full-screen support — key passthrough, cursor-key app mode, DSR/DA responses, `ESC M`,
  `CSI L/M/@/P`, `?25h/l` (`aca54fe`); bottom-anchor + WriteLine alignment + 256-colour SGR (`ffaa881`).
- Shell PTY simplification — child uses pty slave as stdio, parent reads `amaster` only; auto-colors
  work; `SetWindowSize` for vi/less (`7e96e93`).
- Split-view resize bounds — delegation gap (height action fell through `VSplitView` to unbounded
  `SetHeight`) + `ClampSplitterPos` at the `SetSplitterPos` chokepoint. Layout tests added.
- Vertical page-nav CLion symmetry fix (`52529d9`); ClipBoard paste splice rewrite (`00e9eee`) +
  cursor-advance via `GetPasteLineCount` end-`Point`; dc-overlay tab expansion + `IsInside` boundary;
  jsengine `LoadDocument` (`ff45ff7`); test-suite audit (`fa52c94`); `test_vnav_pageup` (`3763816`).

## Remaining / deferred
- **More resize bugs may remain** — user said "a few more" beyond the three fixed this session. The
  ASan build + GUI recipe are the tools; drive width-axis squeeze, workspace-panel drag, maximize/
  restore, and active-output-during-resize.
- **vi polish** — origin mode `ESC[?6h/l`; `test_terminalscreen_savestate` under-tests `SaveScreen`
  (should assert clean alt-screen: blank grid, cleared scrollback, cursor home, reset scroll region);
  bracketed-paste `?2004` / focus `?1004` consumed but not acted on (low priority).
- **Shell mode `promptLen = cursor.x` heuristic** — works when the cursor sits at the prompt end, breaks
  during active command output. A proper fix tracks prompt boundaries explicitly (non-trivial).
- **Standard-color SGR mapping** — `kSetForegroundColor` maps 30–37 via `(idx & 7) + 8` (bold=bright
  xterm convention); maybe a theme toggle later.
- **`.gitignore`** — add `claude.sessions.md` (user's private scratch file, tracked but never committed
  with content). Also consider ignoring `cmake-build-asan/`.
- **Remote/SSH backend** — the big bucket-list item (purpose-built modern-terminal backend, no NCurses).

## Untracked (intentionally never committed)
`.idea/`, `cmake-build-release/` (stale 2023 binary), `cmake-build-asan/` (kept for heap debugging),
`syntax_problem.cpp`, `terminal_rendering_bug.png`.
