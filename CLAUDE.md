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

**Verified-green set** (run from `cmake-build-debug/`):
`trun -m clipboard,document,vnav,cpplang,jsonlang,cppnumbers,linelayout,dcoverlay,layout,jsengine,workspace,terminalscreen,vtermparser,keymapping --sequential ./libutests.so`

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
  `Linux/workspace_keymap.yml` is just `{ inherit: default_keymap }`). NOTE: `KeyMapping`'s `modifiers`
  member map is currently **write-only** (set at parse, never read — match-time masks are baked into
  `actionItems`); flagged for inspection, not yet removed.

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

## Session 2026-06-10 — resume point (read this first)

Work happened on the **Linux dev box (SDL2 backend)**, branch **`dev_workspace`** (NOT `main`).
Executed **Phase 2 steps P2.0 → P2.4** of the buffer/window split (`docs/workspace-refactor-plan.md`,
see its "Phase 2 — status" section). Each step is its own commit and the verified-green set passes
**138** (`-m document` now has 13 cases). **Not pushed yet — push on the next machine.**

**Commits this session (oldest→newest), all on `dev_workspace`:**
- `caa97ff` **P2.0** — pin the contract: `test_document` cases asserting cursor/selection/undo survive
  a document switch. (Finding recorded then: undo capture was coupled to the Editor *active* document.)
- `af98a17` **P2.1** — extract **`ViewState`** (`lineCursor` + `currentSelection`) → new
  `src/Core/ViewState.h` (the `Selection` struct moved there); `Document` holds a `ViewState::Ref`,
  referenced not fused.
- `2bdff75` **P2.2** — extract **`EditState`** (`UndoHistory historyBuffer`) → new
  `src/Core/EditState.h`; `Document` holds an `EditState::Ref`.
- `8b30a25` **P2.2-followup** — decouple `UndoHistory` from the Editor active-document. Capture API
  now takes `(const LineCursor&, [const Selection&,] TextBuffer::Ref)`; `UndoHistory.cpp` dropped
  `#include "Editor.h"`. Behavioral fix (undo on a non-active document) + regression test
  `test_document_undo_independent_of_active` (failed before, passes after).
- `d7c8143` **P2.3** — introduce **`EditorViewContainer`** (`src/Core/Views/EditorViewContainer.h`):
  the single editing slot in `main.cpp`, holding `EditorView` as its one child. Pure interposition;
  live-verified on SDL2 (`:0`) — render + window resize clean.
- `130e4e9` **P2.4** — `EditController` is now a value member of `EditorView`, borrowing a non-owning
  `Document*` (Attach/Detach on switch). Deleted the passthrough (`OnAction`/`OnViewInit`/`GetTextBuffer`/
  `Lines`/`LineAt` proxies + dead `onTextBufferChanged`); view calls `Document::OnAction`/`OnViewInit`/
  `Lines` directly. Removed `Node::controller`/`Set/GetController` and the controller creation in
  `EnsureDocumentForNode`; dropped the `EditController` include from `Workspace.h`.

**Continuation 2026-06-10 (macOS/SDL3) — Phase 2 COMPLETE.** Pulled yesterday's P2.0–P2.4 (already on
origin), ran the P2.4 live verification (see the ✅ block below — found + fixed the scroll-anchor bug),
then executed **P2.5**, the last step. Two new commits, **local only — push next**:
- `927900e` **FIX** — buffer-switch scroll-anchor reset (the P2.4-verification find). +regression test
  `test_document_switch_preserves_scroll`. (Details in the ✅ block below.)
- `88611d8` **P2.5** — mechanical rename `KeyPressAction → EditorAction` (58 sites) + moved the struct
  out of `KeyMapping.h` into a new dedicated `src/Core/EditorAction.h` (includes only `Action.h` +
  `KeyPress.h`, no new cycle; `KeyMapping.h` now includes it). `kAction` enum keeps its name.

**Verified-green set now 139** (`-m document` has 14 cases). The Phase-2 seam is in place: `ViewState`
+ `EditState` referenced by `Document`, `EditorViewContainer` wrapping the single `EditorView` item,
`EditController` owned by that item (raw KeyPress only; Actions → Document), and an `EditorAction` type
in its own header. **Next: Phase 3** (see `docs/workspace-refactor-plan.md` — N-item container / where
`ViewState` ultimately lives is in the "Deferred — to mull over" section, explicitly not yet decided).

> **✅ P2.4 live GUI verification DONE (2026-06-10, macOS/SDL3 continuation).** Confirmed in a running
> build: typing inserts, navigation moves the caret, undo reverts, and buffer-switch keeps per-document
> cursor. **It found a real bug:** on a document switch the logical caret survived but the view's
> vertical **scroll anchor** (`viewTopLine`) reset to line 0, drawing the caret off-screen for any
> document scrolled past the first page. Root cause: `Document::OnViewInit` unconditionally set
> `viewTopLine=0`, and the switch path runs `OnViewInit` on every re-point
> (`SetActiveDocument → RootView.Initialize() → EditorView::ReInitView → document->OnViewInit`), wiping
> the saved per-document `viewTopLine`. Fixed in `927900e` (keep `viewTopLine`, derive `viewBottomLine`
> from it + height, `RefocusViewArea()` only if the active line fell out of view) + regression test
> `test_document_switch_preserves_scroll` (replicates the real switch by re-calling `OnViewInit`;
> failed before, passes after). **Verified-green set now 139.**
>
> **macOS GUI-automation note:** on this box the process Claude runs commands under lacks both
> **Accessibility** and **Screen Recording** TCC permissions, so `osascript` keystroke injection and
> `screencapture` are blocked (System Events can *activate* a process by PID but can't read window
> state or send keys). The Linux `xdotool`/`xwd` recipe has no working macOS equivalent here — the
> live check was done as a **guided manual run** (Claude scripts the keystrokes from the real keymap,
> user performs them and reports). macOS `Alt` shortcuts = **left Option** (the SDL3 `only_left` hint).

> **If switching workstation:** `git push` first (this session's commits are local-only), then on the
> other box `git checkout dev_workspace && git pull`; reconfigure `cmake-build-debug/` for that box's
> backend (Linux = SDL2 ON/SDL3 OFF; macOS = the inverse — `CMakeLists.txt` default is committed as
> SDL2-ON for the Linux box, so the macOS box must pass `-DGEDIT_BUILD_SDL3=ON -DGEDIT_BUILD_SDL2=OFF`);
> build `utests` (`libutests.**so**` on Linux, `.dylib` on macOS) and run the verified-green set
> (`-m document,…`) for a clean 138 baseline. Phase-2 view work touches `EditorView` — keep SDL2/SDL3
> in sync.

---

## Session 2026-06-09 — resume point

Work happened on the **macOS dev box (SDL3 backend)**, branch **`dev_workspace`** (NOT `main`).
Everything is committed and pushed; working tree clean (only the usual intentional untracked files).
Two themes: (1) finished planning **Phase 2 — the buffer/window split** in
`docs/workspace-refactor-plan.md`, and (2) executed **Pre-step P2.pre** (the `model`-token clean-slate
sweep). Verified-green set passes **134** — note the module list changed: it's now **`-m document`**,
NOT `edtmodel` (renamed this session).

**Commits this session (oldest→newest), all on `dev_workspace`:**
- Plan docs: Phase 2 buffer/window split write-up + refinements (container/item split, EditController
  fit, ViewState/EditState, P2.5 `KeyPressAction→EditorAction` + own header, P2.pre scope).
- `1cfc1a4` P2.pre: sweep `model`→`document` variables in `src/Core` (15 files).
- `2a0cdcf` P2.pre: sweep `model` in `utests` to its **real type** — `node` where it was a
  `Workspace::Node` (lang tests' `NewDocument` returns a Node!), `document` where it was a `Document`.
- `322d841` P2.pre: `nodeModel`→`node`, drop dead `LoadEditorModelFromFile` decl, sweep `main.cpp` TODOs.
- `a4905d7` P2.pre: rename test module **`edtmodel`→`document`** (file + all case symbols + CMakeLists +
  this file's verified-green line, in lockstep).

**Next: Phase 2 proper, starting at Step P2.0** (pin the contract) in `docs/workspace-refactor-plan.md`.
Read that doc's Phase 2 section first — the target is `EditorViewContainer` (new, takes the single UI
slot) wrapping `EditorView`-as-item (holds `ViewState` + its `EditController`); `Document` keeps the
Action API + references an `EditState` (undo). The rule: **resolved Actions → Document, raw KeyPress →
controller.** Remaining lowercase `model` tokens are intentional (the `VerticalNavigationViewModel`
family + two TerminalScreen-model comments + one dead comment) — leave them.

> **If switching workstation:** `git checkout dev_workspace && git pull`; reconfigure `cmake-build-debug/`
> for that box's backend (Linux = SDL2 ON/SDL3 OFF — inverse of macOS); build `utests` (`libutests.**so**`
> on Linux) and run the verified-green set (`-m document,…`) for a clean baseline before P2.0. Phase-2
> view work touches `EditorView`, so keep SDL2/SDL3 in sync.

---

## Session 2026-06-08 — resume point

Six commits, all pushed to `main`. Build clean (utests + goatedit); verified-green set passes (132).
Worked on the **macOS dev box (SDL3 backend)**. Theme: a macOS keyboard-shortcut bug, then a keymap
loading/inheritance cleanup that fell out of it.

**Commits this session (oldest→newest):**
- `e40b204` FIX: macOS Option-key UI shortcuts swallowed by dead-key composition
- `98b9874` FEAT: Allow keymaps with no own 'actions' (pure inheritance)
- `6171c4a` TEST: Multi-level keymap inheritance (3-level chain)
- `a5e8a7d` REFACTOR: Collapse Linux workspace_keymap to pure inheritance
- `e1140d9` REFACTOR: Move keymap cache out of Editor into KeyMappingCache singleton
- `b06c0bb` TEST: Clear KeyMappingCache before each keymapping case
- (`6eed30b` FIX: log-output wording — user's own small commit, interleaved)

**Files modified:** `src/Core/Graphics/SDL3/SDLScreen.cpp`, `SDL3/SDLKeyboardDriver.cpp`,
`src/Core/Runloop.cpp`, `src/Core/KeyMapping.{h,cpp}`, `src/Core/KeyMappingCache.{h,cpp}` (NEW),
`src/Core/Editor.{h,cpp}`, `Assets/Resources/macOS/{default_keymap,workspace_keymap}.yml`,
`Assets/Resources/Linux/workspace_keymap.yml`, `utests/test_keymapping.cpp`, `CMakeLists.txt`.

### 1. macOS Option-key shortcuts dead — dead-key composition (`e40b204`)
`UISwitchToEditor` (Option+E) etc. silently did nothing on macOS while Option+T/Option+P worked.
Root cause (confirmed with the `[KBD]`/`[DISP]` stderr trace — now kept commented in the SDL3 driver +
Runloop): on macOS+SDL3 with text input active, *composing* `Option+<letter>` combos are eaten by Cocoa
dead-key composition and arrive only as `SDL_EVENT_TEXT_INPUT` — no `SDL_EVENT_KEY_DOWN` — so the action
never resolves. **Which combos compose is keyboard-layout dependent** (US: e/i/u/n/`` ` ``; the user's
layout also ate `g`), which is why it looked key-specific. **Fix:** `SDL_HINT_MAC_OPTION_AS_ALT=
"only_left"` before `SDL_Init` — left Option = clean Alt (shortcuts), right Option still composes (AltGr).
SDL2 has **no** such hint (SDL3-only) but delivers the KEY_DOWN anyway, so SDL2-on-mac works (minor stray
accent). Also added the missing `UISwitchTo{Terminal,Editor,Project}` bindings to macOS `default_keymap`.
See the macOS-keyboard recipe under Debugging.

### 2. Keymap inheritance cleanup (`98b9874`, `a5e8a7d`, `6171c4a`)
- **Empty-actions allowed:** `RebuildActionMapping` now requires `actions` **or** `inherit` (was: actions
  mandatory), so a pure-inheritance child is legal. `test_keymapping_inherit_empty_actions`.
- **Workspace keymaps de-duplicated:** both now `inherit: default_keymap` — macOS keeps only its genuine
  `CycleActiveViewNext` (+Tab) override; `Linux/workspace_keymap.yml` is just `{ inherit: default_keymap }`.
- **Multi-level proven:** `test_keymapping_inherit_multilevel` — in-memory child → `terminal_keymap` →
  `default_keymap` (3 levels; the resolver loop runs twice). The child loads from memory via
  `ConfigNode::FromString`; **only the child can be in-memory** — inherited parents resolve by name
  through the asset loader.

### 3. KeyMappingCache singleton — keymap-loading single source of truth (`e1140d9`, `b06c0bb`)
`Editor` owned the keymap cache yet inheritance bypassed it (re-parsing each parent per child). Moved the
cache into a new `KeyMappingCache` singleton; `Editor::GetKeyMapping/HasKeyMapping` are now thin
delegates and the `keymappings` member is gone. `ResolveInheritance` gained a `ParentResolver` (injected
by the cache) → appends the already-resolved parent's `actionItems`, so each keymap builds once; legacy
walk kept as the no-resolver fallback. `test_keymapping_cache` asserts same-name→same-instance and
shared-parent-instance; per-case `SetPreCaseCallback` clears the singleton between cases. (See the two
keymap patterns under "Established patterns".)

---

## Earlier work (completed, in git — kept as a one-line index)

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
- **`KeyMapping::modifiers` member is write-only** — set during parse, never read (match-time masks are
  baked into `actionItems`; `ModifierName` uses the static `strToModifierMap`). Candidate for removal;
  **user wants to inspect it first before dropping** (do not remove unprompted).
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
- **`.gitignore`** — add `claude.sessions.md` (user's private scratch file, tracked but never committed
  with content). Also consider ignoring `cmake-build-asan/`.
- **Remote/SSH backend** — the big bucket-list item (purpose-built modern-terminal backend, no NCurses).

## Untracked (intentionally never committed)
`.idea/`, `cmake-build-release/` (stale 2023 binary), `cmake-build-asan/` (kept for heap debugging),
`syntax_problem.cpp`, `terminal_rendering_bug.png`.
