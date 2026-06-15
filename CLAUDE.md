# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

**GoatEdit** — a personal text/code editor written in C++20. Runs on Linux and macOS. Renders through
an SDL backend (SDL2 and SDL3 sources both exist). **Backend is platform-dependent: the Linux dev box
builds/runs SDL2 (`gedit::SDL2::*`); the macOS dev box builds/runs SDL3 (`gedit::SDL3::*`).** Keep both
backends in sync when touching one (e.g. keyboard handling differs between them — see the macOS
Option-key note below). NCurses sources still exist in-tree but are **out of the build** (the stated
direction is a purpose-built modern-terminal backend, eventually able to run over SSH). Embeds a
JavaScript plugin engine (Duktape) for scripting. The primary executable target is `goatedit`.

## Build

CMake 3.22+ is required. Build is configured per-platform with optional SDL2/SDL3 flags.

```sh
# Install system deps (Linux)
sudo apt-get install -y libyaml-cpp-dev libncurses-dev libsdl2-dev
./setup_deps.sh       # clones ext/ source deps (json, gnklog, dukglue, fmt)

# Configure. NOTE: backend is per-machine (same 'cmake-build-debug' dir name, different config):
#   - Linux dev box: SDL2 ON / SDL3 OFF, running binary gedit::SDL2::* (flags below).
#   - macOS dev box:  SDL3 ON / SDL2 OFF, running binary gedit::SDL3::* (swap the flags).
# Match whatever the local build dir is already configured with.
cmake -B ./cmake-build-debug -DCMAKE_BUILD_TYPE=Debug -DGEDIT_BUILD_SDL3=OFF -DGEDIT_BUILD_SDL2=ON    # Linux
# cmake -B ./cmake-build-debug -DCMAKE_BUILD_TYPE=Debug -DGEDIT_BUILD_SDL3=ON -DGEDIT_BUILD_SDL2=OFF   # macOS

# Build the main binary
cmake --build ./cmake-build-debug --config Debug --target goatedit -j

# Build the unit test shared library
cmake --build ./cmake-build-debug --config Debug --target utests -j
```

CMake auto-clones missing `ext/` dependencies on first configure (except duktape, which is pre-included in `src/ext/duktape-2.7.0`).

## Running Tests

Tests use the **trun** (TestRunner v3) framework. The test target builds `utests` as a shared library;
`trun` loads and runs it. **On macOS the library is `libutests.dylib`; on Linux it's `libutests.so`** —
substitute accordingly in the commands below. The editor compiles its test cases against the **v1**
test interface (`TRUN_USE_V1` → `testinterface_v1.h`); that interface exposes per-module
`SetPreCaseCallback` / `SetPostCaseCallback` (signature `void(*)(ITesting *)`), registered from the
module entry fn (e.g. `test_keymapping`) — used to reset global singletons between cases (see
`test_keymapping` clearing `KeyMappingCache`).

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

**Verified-green set** (202 tests, run from `cmake-build-debug/`):
`trun -m clipboard,document,vnav,cpplang,jsonlang,cppnumbers,linelayout,dcoverlay,layout,jsengine,workspace,terminalscreen,vtermparser,keymapping,hexprojection,bytestream,hexview,indent --sequential ./libutests.so`

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
- **Renaming a type renames its references too.** When a type is renamed (e.g. `EditorModel` →
  `Document`), sweep the variables/members/params that hold it so the *name* tracks the type:
  `Document::Ref document; document->XYZ()` — never leave `Document::Ref model; model->XYZ()`. A
  stale variable name reading against its new type is a code smell and looks odd at every use site.
  (This is exactly the `model`→`document` cleanup left half-done by the Phase-1 sweep; finish such
  renames in the same pass, don't defer the variable names.)

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

- **A lower layer NEVER depends on a higher-level app service — invert via glue + plain data/callbacks.**
  (Mishap caught 2026-06-15, window-geometry session work — *don't repeat it*.) The Graphics backend
  (`ScreenBase` / `SDL2`+`SDL3` `SDLScreen`) sits **below** app-level singletons like `SessionManager`,
  `Editor`, `Config`. The first cut had `ScreenBase` call `SessionManager::Instance()` directly to read/write
  window geometry — backwards: it made the rendering layer depend on a high-level app construct. **The fix
  is the shape to reuse:** the low layer exposes *plain-data* hooks (`SetRequestedWindowGeometry(x,y,w,h)`, a
  `WindowGeometryChangedHandler` callback of plain ints, `GetPrimaryDisplayBounds()` — a genuine graphics
  concern), and a **glue function in the layer that owns initialisation** (`Editor::WireScreenGeometry`,
  called in `SetupSDL2`/`SetupSDL3` before `Open()`) bridges the two — it reads the session, feeds data in,
  and registers the write-back callback. Data flows OUT of the low layer through the callback; nothing
  app-level flows in. Logic that *legitimately* needs a low-layer resource (geometry default + clamp need
  the display bounds) stays in the low layer — that's not an app dependency. Same rule let the autosave
  debounce live in `SessionManager` with an **injected** `autoSaveHandler` (set by `Editor`) instead of a
  `SessionManager → Editor` dependency. **Smell test: if a `Core/Graphics/*` file `#include`s
  `Core/Session/*` (or `Editor.h`), you've inverted a layer.**
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
- **Keymap loading has ONE owner: `KeyMappingCache` (singleton).** Both `Editor::GetKeyMapping` (a thin
  delegate now — Editor no longer owns a keymap map) and inheritance resolution go through it, so each
  named keymap is built **exactly once** and shared by `Ref`. `GetOrLoad(name)` loads
  (`KeyMapping::LoadKeymapConfig`, the single asset-load point, OS-specific `linux`/`macos` lookup),
  builds, resolves the `inherit` chain *through itself*, caches, returns. Cycle-guarded by an
  `inProgress` set. `Clear()` exists for reload / test isolation. KeyMapping has **no** dependency on
  Editor (the cache depends only on KeyMapping + the asset loader).
- **Keymap inheritance: append the resolved parent, don't re-walk.** `KeyMapping::Initialize` parses the
  keymap's own bindings first (so first-match ⇒ child overrides parent), then `ResolveInheritance` takes
  a `ParentResolver` (injected by the cache). With it, the parent is already FULLY resolved (its own
  chain folded in, built once), so we just append the parent's `actionItems` — no chain walk, no
  re-parse. `ActionItem` is immutable post-build, so sharing the `Ref`s across keymaps is safe. The old
  iterative `LoadKeymapConfig`-per-ancestor walk is kept as the **no-resolver fallback** so memory-built
  keymaps (`Initialize(cfgNode)` with no cache — the `FromString` tests) stay hermetic. A keymap is
  valid with **no `actions` of its own** as long as it has `inherit:` (pure-inheritance child, e.g.
  `Linux/workspace_keymap.yml` is just `{ inherit: default_keymap }`). The members are now just
  `isInitialized` + `actionItems` — the old write-only `modifiers` map was dropped (`204c89b`); match-time
  modifier masks live baked into `actionItems`.

## Debugging / verification recipes

- **GUI verification (SDL2 build, real `:0` display):** launch `goatedit` from `cmake-build-debug` with
  `DISPLAY=:0`, record the PID, find the window with `xdotool search --pid <PID>`. **VERIFY THE WINDOW
  IS YOURS BEFORE SENDING ANY INPUT** — `xdotool search --pid` is unreliable and HAS returned another
  app's window (a 2026-06-10 session typed into the user's CLion this way). Confirm with
  `xprop -id <WID> _NET_WM_PID` (must equal your launched PID) and `xdotool getwindowname <WID>` (should
  be `gedit`) before any `key`/`type`. Drive with
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
- **macOS keyboard / dead-key shortcuts (SDL3):** on macOS+SDL3 with text input active, *composing*
  `Option+<letter>` combos (which combos compose is **keyboard-layout dependent**) are eaten by Cocoa
  dead-key composition and arrive **only** as `SDL_EVENT_TEXT_INPUT` — **no `SDL_EVENT_KEY_DOWN`** — so
  any UI shortcut bound to them (Alt == `UINavigationModifier`) silently never fires. Fix is
  `SDL_SetHint(SDL_HINT_MAC_OPTION_AS_ALT, "only_left")` before `SDL_Init` (`SDL3/SDLScreen.cpp`): left
  Option → clean Alt (shortcuts work, no composition), right Option untouched (AltGr `[ ] { }`/accents
  still type). The hint is **SDL3-only** — SDL2 has no equivalent, but SDL2 delivers the KEY_DOWN anyway
  so shortcuts work there (minor stray accent char). To diagnose: re-enable the commented `[KBD]` trace
  in `SDL3/SDLKeyboardDriver::ProcessEvent` (raw event → synthesized `KeyPress`) and the `[DISP]` trace
  in `Runloop::DispatchToHandler` (key/mods → action), run `./goatedit 2>trace.txt`, press the keys, and
  compare a working combo (KEY_DOWN present) vs a failing one (only TEXT_INPUT).

---

## Current state — resume point (read this first)

### Baseline (2026-06-15): active branch `feature/session-cache-phase1`

`main` is the consolidated baseline (2026-06-12); the **session-cache work lives on
`feature/session-cache-phase1`** (off `main`, not yet merged). Per-machine SDL backend is now
**auto-selected** (`if(APPLE)` in `CMakeLists.txt`, `28c413b`) — the old "never commit the CMakeLists SDL
toggle" caveat is **obsolete**; there is no toggle to leave dirty anymore. Working tree is clean except the
untracked-by-design files. **`docs/session-cache.md` is the authoritative tracker for this feature** (read its
§0 resume-point first).

**Session cache — Phase 1 COMPLETE on the branch (GUI-verified):** per-root `.goatedit/session.yml`
restores open files + cursors/scroll/selection/view-mode, layout (splitters + focused view + tree
expand/collapse), and **window geometry**; plus **debounced autosave**. Step 2 (live registry +
restore-on-reboot) is the next phase. See `docs/session-cache.md`. **Key layering rule that came out of
this — see the "Lower layer never depends on a higher-level app service" pattern below.**

Verified-green set **221** on the branch (run from `cmake-build-debug/`; macOS lib `libutests.dylib`, Linux
`libutests.so`):
`-m clipboard,document,vnav,cpplang,jsonlang,cppnumbers,linelayout,dcoverlay,layout,jsengine,workspace,terminalscreen,vtermparser,keymapping,hexprojection,bytestream,hexview,indent,session`
(was 202 on `main`; the `session` module adds the delta, and the obsolete `winlocation` module was removed.)
**Do NOT run the full debug suite** — the sqlite3-parse case is intentionally excluded (13–15s in debug).

**Recently shipped (on `main`):** autopair + a data-driven `IndentEngine` (electric indent/dedent, `indent.yml`)
+ the **reformat** feature — `ReformatLine` (`Cmd/Ctrl+L`), `ReformatBlock` (`Cmd/Ctrl+I`, selection→range or
no-selection→enclosing `{}`), block-surround (type `{` over a multi-line selection → braces on own lines, body
nested). Feel-check complete A–G. Design: `IndentEngine` (pure) + `Document` (wiring), token-aware reindent
(braces in comments/strings masked). Plus the E.18 whole-line cut/paste fix (see lesson below + open-bugs.md #4).

**E.18 KEY LESSON (keep — it bit us once):** GUI copy/paste does NOT use the internal clipboard item. It
round-trips through the **OS pasteboard as an external item** (`PasteFromClipboard → UpdateClipboardData →
CopyFromExternal`). So any clipboard fix must be unit-tested over THAT round-trip
(`test_clipboard_external_roundtrip_*`), not just `PasteToBuffer` — a fix that only touches the internal item
passes its tests while the GUI still misbehaves. Serialization has one home now:
`ClipBoard::ClipBoardItem::AsText()`, used by both SDL2/SDL3 `OnUpdate` hooks.

**Deferred bug sweep → `docs/open-bugs.md` (3 OPEN entries, do as one pass, NOT piecemeal):** (1) `Line::AttributeAt`
returns the first span for any pos in a line's last token span; (2) `Document::SetCursorPosition` writes an
absolute index into the screen-relative `position.y` (search-jump mis-draws on a scrolled view); (3) tokenizer
mis-tags an identifier abutting `{` (`{foo`) as an operator until a space is typed. Each entry carries a trace,
the chosen fix, and the test to add.

### Phase 2/3 baseline + the north-star (multi-view)

The Phase 2 (buffer/window split) + HexView-spike work below is the durable architecture under the shipped
features; **Phase 3 (multi-view) is the next north-star.** Untracked-by-design: `syntax_problem.cpp`,
`terminal_rendering_bug.png`, the `cmake-build-*` dirs.

**Done — Phase 2 (buffer/window split) + the pre-Phase-3 HexView spike** (shipped on `main`). The seam now
in place:
- `Document` references a **`DocumentViewState`** (cursor + selection + `DocumentViewMode {kText,kHex}`) and an
  **`EditState`** (undo) — both per-view-extractable. Rule: **resolved Actions → Document, raw KeyPress
  → controller.** `EditController` is a value member of `EditorView`; `EditorAction` is its own header.
- **`EditorViewContainer`** is the single editing slot **and the registered top view**: it holds the text
  `EditorView` (primary) + read-only `HexView` (alternate), forwards input/status/focus to the active
  item, owns the text↔hex swap (`OnAction` intercepts `kActionViewModeText/Hex` — Alt+R / Alt+H) **and**
  buffer cycling (`kActionCycleActiveBuffer{Next,Prev}` → `ActionHelper`). View-mode restores per-document
  across a switch: `ReInitView → SyncToActiveDocument → ApplyViewMode` (shared mechanics; `SwitchToViewMode`
  writes intent, the sync only reads it; focus transfers only if the outgoing item held it).
- **`HexView` is a bidirectional projection of the canonical caret:** `DocumentViewState.lineCursor` stays
  in text coords; `HexProjection` maps it ↔ byte offset over a `ByteStreamReader` (the one
  UTF-32→UTF-8 place). Horizontal nav steps whole chars (multibyte never traps); vertical/page = 16-byte
  rows then snap. Write-back via `Document::SetCursorPosition`. Read-only; caret drawn inverted in both the
  hex and ASCII columns; theme color via `contentColors["hexview"]`. The text-line **gutter stands down in
  hex mode** (`GutterView` early-returns when `GetViewMode()==kHex` — hex rows ≠ text lines).

**Next step — Phase 3 (the north-star feature: multi-view).** The narrowed unknown: **two *mutable*
views** of one buffer + where per-view `DocumentViewState` is keyed/owned (view-owns-`map<Document*,state>`
vs Document-hands-out vs a session object — left open by design; `EditState`/undo stays per-document, shared
by all views). The container-swap seam and the "view = projection of canonical state" model are proven by the
spike. **Pondered 2026-06-10 (the framing to start from):** the container's text↔hex swap is a
*representation* swap sharing **one** `DocumentViewState`; Phase 3 adds *views*, each needing **its own**.
The clean shape is **recursive** — today's `EditorViewContainer` becomes the per-view unit, and a new outer
split (reusing `VSplitView`) holds N of them; each unit owns its live `DocumentViewState`, all share the
`Document`'s `TextBuffer` + `EditState`, and `Document` keeps only the *saved snapshot* (for restore/reopen).
**First load-bearing move = lift the live `DocumentViewState` off `Document` onto the view-unit** (snapshot
stays on `Document`); preserve the spike's text↔hex caret-sharing — the share-point moves from "Document's
one ref" to "the unit's one ref", with the HexView round-trip test as the guard. Alternative lower-risk
direction if you want a visible win first: **restore-on-reopen** (serialize `DocumentViewState` + `EditState`
per document, restore on folder reopen — banks the Phase-2 extraction, no cursor-concurrency hazard).

**Deferred design (open, undecided):** typed buffers — `TextBuffer` vs a raw `BinBuffer` backend — so a
file can open as something other than editable text (a faithful hex view of *non-UTF-8* bytes, or e.g. a PNG
shown as an image). The HexView spike deliberately **re-encodes the text buffer** (lossy for non-UTF-8) as a
v1 limitation, NOT an oversight; whether the editor should hold multi-datatype views at all is philosophically
undecided (possibly out of this project's purpose).

**Operational notes:** per-machine backend (Linux SDL2 / macOS SDL3 — match the local `cmake-build-debug`;
the committed `CMakeLists.txt` default is SDL2-ON, so the macOS box configures with
`-DGEDIT_BUILD_SDL3=ON -DGEDIT_BUILD_SDL2=OFF`). On macOS, GUI verification is a **guided manual run**
(`osascript` keystrokes + `screencapture` are TCC-blocked for Claude's shell here); `Alt` shortcuts =
**left Option** (SDL3 `only_left` hint). Push before switching workstations, then reconfigure the backend
and rebuild `utests` (`libutests.so` Linux / `.dylib` macOS) on the other box.

---

## Earlier work (completed, in git — kept as a one-line index)

- **Autopair + IndentEngine + reformat + E.18** (`9b48245`, merged to `main` 2026-06-12): data-driven
  `IndentEngine` (electric indent/dedent, token-aware reindent) + `ReformatLine`/`ReformatBlock`/block-surround;
  E.18 whole-line cut/paste trailing-newline fix (selection-end normalization + OS-pasteboard round-trip via
  `ClipBoard::AsText()` / `CopyFromExternal`). Feel-check complete A–G. (See the E.18 KEY LESSON in the
  resume-point above.)
- **HexView spike — H.1–H.4 + polish** (merge commit `53f0193`): `HexProjection` coord translation +
  `ByteStreamReader`/`BinBuffer` (`8432aa4`); `DocumentViewMode {kText,kHex}` + view-mode switch action
  (`0edbf24`, enum renamed from `ViewMode` in `9cabfa2`); read-only `HexView` (`58e470f`);
  `EditorViewContainer` owns the text↔hex swap (`b9dbbb9`); per-document view-mode restore on switch
  (`a652e6e`); buffer-cycling→container (`01579e8`); ASCII-column caret (`2c876bb`); gutter stands down in
  hex mode (`bdc35d9`); theme hex color (`56b877c`,`e4c6178`); hygiene/.gitignore (`cbf919c`). Design notes
  see the Phase 2/3 section in the resume-point above.
- **Phase 2 — buffer/window split + P2.pre**: `model`→
  `document` token sweep + test module `edtmodel`→`document` (`1cfc1a4`,`2a0cdcf`,`322d841`,`a4905d7`);
  P2.0 document-switch contract tests; P2.1 extract `ViewState` (renamed `DocumentViewState` in the spike);
  P2.2 extract `EditState` + decouple `UndoHistory` from the Editor active-document (`af98a17`,`2bdff75`,
  `8b30a25`); P2.3 `EditorViewContainer` (`d7c8143`); P2.4 `EditController`→value member of `EditorView`
  (`130e4e9`) + buffer-switch scroll-anchor fix (`927900e`); P2.5 `KeyPressAction`→`EditorAction` in its
  own header (`88611d8`).
- **2026-06-08 keymap session**: macOS Option dead-key fix —
  `SDL_HINT_MAC_OPTION_AS_ALT=only_left` (`e40b204`); keymaps may pure-inherit + multi-level inheritance
  (`98b9874`,`a5e8a7d`,`6171c4a`); `KeyMappingCache` singleton as the single keymap-loading owner
  (`e1140d9`,`b06c0bb`). Mechanics live in "Established patterns" (keymap) + the macOS-keyboard Debugging
  recipe.

- **Session 2026-06-06** (5 commits): keymap-inheritance moved into `KeyMapping` (`ba4ab93`, since
  superseded by the `KeyMappingCache` refactor above); `TerminalController` keypress-state readability
  refactor (`9155ae5`); and three window-resize bug fixes each with a regression test + live GUI repro —
  terminal-pane-to-zero heap UAF found via ASan (`f80844a`), corner-resize splitter collapse (`24fc535`),
  back-and-forth resize content drift (`a2c8c17`). User noted "a few more resize bugs" may remain.

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
- **Open bugs tracker → `docs/open-bugs.md`.** Known-wrong code we chose NOT to fix in-place yet, with
  cold-start context. Currently: (1) `Line::AttributeAt` returns the FIRST span (kRegular) for any pos in
  a line's LAST token span (so a trailing comment reads as code) — left alone because
  `Document::OnActionWordRight` leans on that buggy fallback; reformat/indent use their own correct
  `TokenClassAtChar` scans instead. (2) `Document::SetCursorPosition` writes an ABSOLUTE line index into
  the screen-relative `cursor.position.y` (caret mis-draws on a scrolled view, e.g. search-jump). (3) the
  syntax highlighter mis-tags an identifier that directly abuts `{` (e.g. `{foo`) as an operator until a
  space is typed.
  Read that file before touching `AttributeAt`, word-nav, any caret/goto-line code, or the tokenizer.
- ~~HexView: per-document view-mode restore across a document switch.~~ **DONE 2026-06-10** (see
  resume-point Next-steps #1): `EditorViewContainer::ReInitView` → `SyncToActiveDocument` → shared
  `ApplyViewMode` mechanics (focus transferred only if the outgoing item held it). Compile + green; GUI
  confirm still pending.
- ~~`KeyMapping::modifiers` member is write-only.~~ **DONE** — dropped in `204c89b`; `KeyMapping`'s only
  members are now `isInitialized` + `actionItems` (match-time modifier masks are baked into `actionItems`).
- **More resize bugs may remain** — user said "a few more" beyond the three fixed 2026-06-06. The
  ASan build + GUI recipe are the tools; drive width-axis squeeze, workspace-panel drag, maximize/
  restore, and active-output-during-resize. (These were a Linux/SDL2 hunt — re-check on macOS/SDL3 too.)
- **vi polish** — origin mode `ESC[?6h/l`; `test_terminalscreen_savestate` under-tests `SaveScreen`
  (should assert clean alt-screen: blank grid, cleared scrollback, cursor home, reset scroll region);
  bracketed-paste `?2004` / focus `?1004` consumed but not acted on (low priority).
- **Shell mode `promptLen = cursor.x` heuristic** — works when the cursor sits at the prompt end, breaks
  during active command output. A proper fix tracks prompt boundaries explicitly (non-trivial).
- **Standard-color SGR mapping** — `kSetForegroundColor` maps 30–37 via `(idx & 7) + 8` (bold=bright
  xterm convention); maybe a theme toggle later.
- **`.gitignore`** — now ignores `cmake-build-asan/`, `cmake-build-release/`, `.idea/` (the build/IDE
  clutter). NOTE: `claude.sessions.md` is **deliberately committed** as a session backup
  (`6ce81e6`/`faf5e39`), so it is intentionally NOT ignored — the old note ("never committed with content")
  was stale.
- **Remote/SSH backend** — the big bucket-list item (purpose-built modern-terminal backend, no NCurses).

## Untracked (intentionally never committed)
`.idea/`, `cmake-build-release/` (stale 2023 binary), `cmake-build-asan/` (kept for heap debugging),
`syntax_problem.cpp`, `terminal_rendering_bug.png`.
