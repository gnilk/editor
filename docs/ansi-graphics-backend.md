# New Graphics backend for modern terminal

Create a new graphics backend that enables rendering to a modern TTY console terminal.

> Status: **proposal / design** (2026-06-27). No code written yet — this document is the plan.
> The phased work-item list is at the end.

---

## 1. Brief / Requirements (source)

* Allow for rendering over SSH
* Allow rendering directly in a local terminal
* Needs only to support modern (not older than 10 years) terminal protocols
* Should be able to offer an alternative backend implementation in the same binary as SDL2/3 and Headless
* Should always be a selectable 'backend' (either via the config.yml or through the '--backend' cmd switch)

This can be done in several ways. The suggested shape:

1. Create a library (treated as its own entity — it must **not** depend on anything in the editor).
   Think of it as the 'SDL' (or a modern NCurses) API surface.
   * Lives in its own folder (`src/ext/<name>` or `src/Core/<name>`).
   * Has its own `CMakeLists.txt`; the editor pulls it in via `add_subdirectory` — never by listing the
     library's source files explicitly.
   * Unit tests live in a subfolder of the library itself.
   * macOS and Linux supported. Windows would be nice — not yet considered.
   * If ~80% can be made platform-independent, use platform interfaces + concrete impls
     (`ISomething` → `class LinuxSomething : public ISomething` for C++; for C, compile the platform
     impl in the library's `CMakeLists.txt`).
2. Create a graphics backend that implements everything necessary using the library.

* The 'library' can be C or C++ (doesn't matter).
* The backend should consume the library the same way the `SDL3` backend consumes SDL3.
* Base classes for the graphics backend: `src/Core/UI/Graphics`.
* Existing implementations: `src/Core/Graphics/` — examples `SDL3`, `SDL2`, `Headless`.
* NCurses is already out of compilation and should be **removed completely** (do not study it).
* Special care in: **coordinate translation**, **keyboard & mouse input**, **primitive drawing**,
  **overlay handling**.
* Follow the coding patterns established in the existing backends.

---

## 2. How a backend plugs in today (the contract to satisfy)

A backend implements four interfaces under `src/Core/UI/Graphics/` and is selected at runtime:

| Interface (`src/Core/UI/Graphics/`) | Responsibility | SDL3 reference |
|---|---|---|
| `ScreenBase` | Open/Close, `Clear`/`Update`, `Dimensions()` (in **cols×rows**), `CreateWindow`/`UpdateWindow`, `PollEvents()` (pump → post to Runloop), `CopyToTexture`/`ClearWithTexture` (modal snapshot), geometry hooks | `SDL3::SDLScreen` |
| `WindowBase` | A logical rect with a `DrawContext`, caption, decorations, cursor | `SDL3::SDLWindow` |
| `DrawContext` | All drawing: `DrawStringAt` / `DrawStringWithAttributesAt`, `FillLine`, `DrawHRule`, `Scroll`, overlays, fg/bg color (works in `ColorRGBA`) | `SDL3::SDLDrawContext` |
| `KeyboardDriverBase` | Produce `KeyPress` from native events | `SDL3::SDLKeyboardDriver` |

Key facts that shape the TTY design:

* **The editor already renders into a character grid.** `ScreenBase::Dimensions()` returns cols×rows;
  views lay out in cells. SDL's whole job is to translate those cells *down* to pixels. **A TTY backend
  removes that translation — cells are the native unit.** This makes most of the "graphics" work
  disappear and turns into a damage-tracked cell-grid → ANSI problem.
* **The event pump is already main-thread + queue-based.** `Runloop::ProcessMessageQueue` calls
  `screen->PollEvents()`, which blocks up to ~250 ms, then posts `KeyPress`/`MouseEvent` via
  `Runloop::PostMessage(...)`. Window resize is dispatched inside `PollEvents` (`OnSizeChanged()` →
  `RootView().Resize()`). The TTY backend slots into exactly this shape: `poll()` stdin with a timeout,
  parse, post; `SIGWINCH` → `OnSizeChanged()`.
* **Input is `KeyPress` { `char32_t key`, `bool isSpecialKey`, `int specialKey` (a `Keyboard::kKeyCode`),
  `uint8_t modifiers` (`Keyboard::kMod_*`) }** and `MouseEvent` { kind, col `x`, row `y`, button,
  wheelDelta, clicks, modifiers }. Mouse coordinates are **absolute screen cells** already — the
  terminal reports cells natively (SGR mouse), so there is *no* pixel→cell scaling on input either.
* **The editor works in truecolor `ColorRGBA`** end-to-end (the indexed `RegisterColor` /
  `DrawStringWithAttributesAndColAt` path is unused — SDL3 `exit(1)`s it). → emit 24-bit SGR.
* **The macOS/SDL3 modifier pain** (Option dead-keys eating `KEY_DOWN`, Shift-Arrow selection) is the
  exact problem the **Kitty keyboard protocol** solves on the terminal side — see §5.2.

---

## 3. Library decision

### Build our own, don't vendor a toolkit

I evaluated vendoring an existing library:

* **notcurses** — powerful but large, GPL-adjacent licensing friction, owns its own rendering/IO model
  that fights our `DrawContext`/`Window` contract; overkill.
* **termbox2** — single-header C, modern, truecolor + SGR mouse, no terminfo. Closest fit, but it owns
  input parsing and the cell model in *its* shape, and gives us no platform-interface seam to grow into
  Windows. Good **reference**, not a clean drop-in under our `ScreenBase`/`DrawContext` split.
* **FTXUI / libtickit / ncurses** — wrong altitude (FTXUI is a whole UI framework), or exactly the
  terminfo-era machinery the brief says to skip.

The brief itself describes building a first-party library (own folder, own CMake, own tests, platform
interfaces). For "modern terminals only, no terminfo" the scope is genuinely small, and a hand-rolled
library gives us: the precise API surface our backend wants, the `ISomething` platform seam for a later
Windows port, full control over the Kitty-protocol input path (the load-bearing feature), and pure-logic
units that are trivially testable headless. **Recommendation: build it.** Borrow the proven escape
sequences from termbox2/notcurses as a reference, not as a dependency.

### Name, location, language, namespace

* **Name / target / folder:** `gansi` — short, unique, greppable; the `g` reads as *gnilk* or *goat*.
* **Location:** `src/ext/gansi/`. `src/ext` is where self-contained, independently-built units already
  live (`duktape-2.7.0`, `stb`); putting it there reinforces "no editor dependency." (`src/Core/gansi`
  is equally acceptable per the brief — `src/ext` recommended.)
* **Language:** **C++20**, header + small `.cpp`s. Integrates cleanly, lets us use `std::u32string`,
  RAII for termios restore, and the interface/concrete pattern the brief asks for. Zero third-party deps
  (no `fmt`, no editor types) so it stays standalone.
* **Namespace:** **`gnilk::ansi`** — consistent with the other `gnilk` libraries' nested
  `gnilk::XYZ` convention. *Not* `gansi` (everything gnilk-owned stays under `gnilk`) and *not*
  `gnilk::gansi` (the `g` already means gnilk — that stutters). Being a `gnilk::` (not `gedit::`)
  symbol is itself the signal that this is a standalone library, not editor code. The **backend** that
  consumes it lives in `gedit::Gansi` (mirroring `gedit::SDL3`).

---

## 4. Library design — `gansi` (`gnilk::ansi`)

### Responsibilities

* Raw-mode terminal setup/teardown (termios), with RAII restore even on crash paths.
* A **double-buffered cell grid** (front = last flushed, back = current) with per-cell glyph
  (`char32_t`) + fg/bg truecolor + attribute bits.
* **Damage-tracked flush**: diff back vs front, emit the *minimal* byte stream — move the cursor only
  when the run breaks, change SGR only when the pen changes. UTF-8 encode glyphs on the way out.
* **Capability sequence** on open/close: alternate screen (`?1049h/l`), cursor hide/show + shape
  (DECSCUSR), SGR mouse (`?1006h` + button/motion), bracketed paste (`?2004h`), focus events
  (`?1004h`), and **Kitty progressive keyboard enhancement** (`CSI > 1 u` / pop on exit) with an
  `xterm modifyOtherKeys` fallback.
* **Input parsing**: a byte-stream state machine → neutral events: UTF-8 chars, CSI/SS3 special keys
  (arrows, F-keys, Home/End/PageUp/Down, Insert/Delete), SGR mouse (1006), bracketed-paste payloads,
  focus in/out. Tolerant of sequences split across reads.
* **Size**: `TIOCGWINSZ` + a `SIGWINCH` flag (signal handler only sets an atomic).
* **Clipboard out** via **OSC 52** (works over SSH where the terminal supports it). Inbound paste is
  delivered as a bracketed-paste *event* (the reliable cross-terminal path).

### Non-responsibilities

No layout, no views, no theme, no knowledge of the editor. No terminfo / no termcap DB — we assume a
modern, fixed feature set and *send* sequences; we don't query a capability database.

### Public API surface (sketch)

```cpp
namespace gnilk::ansi {
    struct Color  { uint8_t r, g, b; };                       // truecolor only, no palette indirection
    enum class Attr : uint16_t { None=0, Bold=1, Dim=2, Italic=4, Underline=8, Inverse=16, /*…*/ };
    struct Cell   { char32_t ch = U' '; Color fg{}; Color bg{}; Attr attr = Attr::None; };

    enum class Key { None, Up, Down, Left, Right, Home, End, PageUp, PageDown,
                     Insert, Delete, Enter, Tab, Backspace, Escape, F1, /* … */ F12 };
    enum class Mod : uint8_t { None=0, Shift=1, Alt=2, Ctrl=4, Super=8 };

    struct KeyEvent   { bool isChar; char32_t ch; Key key; Mod mods; };
    struct MouseEvent { enum class Kind { Press, Release, Move, Wheel } kind;
                        int col, row, button, wheel; Mod mods; };
    struct PasteEvent { std::u32string text; };
    struct FocusEvent { bool focused; };
    struct ResizeEvent{ int cols, rows; };
    using  Event = std::variant<KeyEvent, MouseEvent, PasteEvent, FocusEvent, ResizeEvent>;

    // Platform seam — Posix now, Windows later (the only file that #includes <termios.h>).
    class ITerminalIO {
    public:
        virtual ~ITerminalIO() = default;
        virtual bool   EnterRawMode()                                = 0;
        virtual void   RestoreMode()                                 = 0;
        virtual bool   GetSize(int &cols, int &rows)                 = 0;
        virtual long   Read(uint8_t *buf, size_t n, int timeoutMs)   = 0;   // poll()-backed
        virtual void   Write(const uint8_t *buf, size_t n)           = 0;
        virtual bool   TakePendingResize()                           = 0;   // consumes SIGWINCH flag
    };

    // The "screen": owns both grids + encoder + input parser, drives ITerminalIO.
    class Terminal {
    public:
        explicit Terminal(std::unique_ptr<ITerminalIO> io);
        bool  Open();                       // raw mode + alt-screen + enable caps
        void  Close();                      // disable caps + restore (RAII-safe)
        void  Resize(int cols, int rows);
        int   Cols() const;  int Rows() const;
        Cell &At(int col, int row);         // back-buffer write; bounds-clamped
        void  Clear(Color bg);
        void  Flush();                      // diff back↔front → minimal ANSI → Write
        void  SetCursor(int col, int row, bool visible /*, CursorStyle*/);
        std::optional<Event> PollEvent(int timeoutMs);   // Read + parse one event
        void  SetClipboard(const std::string &utf8);     // OSC 52
    };
}
```

### Platform abstraction

`ITerminalIO` is the single seam. `PosixTerminalIO` (macOS + Linux share ~all of it — termios, `poll`,
`ioctl(TIOCGWINSZ)`, `sigaction(SIGWINCH)`) is the only impl now; a future `WindowsTerminalIO` (Console
API + `ENABLE_VIRTUAL_TERMINAL_PROCESSING`) drops in without touching `Terminal`. Everything above
`ITerminalIO` is platform-independent (well past the 80% bar).

### File layout

```
src/ext/gansi/
├── CMakeLists.txt                 # produces static lib target `gansi`; add_subdirectory'd by the editor
├── include/gansi/                 # public headers (Terminal.h, Types.h, Event.h, ITerminalIO.h)
├── src/
│   ├── Terminal.cpp               # grids, flush/diff orchestration
│   ├── AnsiEncoder.{h,cpp}        # cell-diff → ANSI bytes (pure, testable)
│   ├── InputParser.{h,cpp}        # bytes → Event (pure, testable)
│   ├── Caps.{h,cpp}               # enable/disable sequence builders
│   └── platform/PosixTerminalIO.{h,cpp}   # the only termios/poll/signal file
└── tests/                         # trun module(s): encoder, parser, resize invariants, mock IO
```

### Testing approach

The encoder, the input parser, and the grid/diff are **pure logic** — no real TTY needed. Tests feed
byte streams → assert decoded events (incl. modifier combos and sequences split across reads), and
render into the grid → assert the emitted ANSI byte stream and that the diff is minimal. The platform
seam is exercised with a `MockTerminalIO` (in-memory read/write buffers + scripted resize). This matches
the project's reproduce-before-fix discipline and headless-test philosophy. Tests use **trun** (it's a
test framework, not an editor dependency) but build/run from the library's own CMake target so the
library stays independently buildable.

---

## 5. Backend design — `gedit::Gansi` (`src/Core/Graphics/Gansi/`)

Mirrors the SDL3 layout. Consumes `gansi` exactly as `gedit::SDL3` consumes SDL3.

| Backend class | Built on | Notes |
|---|---|---|
| `Gansi::GansiScreen : ScreenBase` | `gnilk::ansi::Terminal` | Open/Close, `Dimensions()`=cols×rows, `Clear`/`Update`(=Flush), `PollEvents`, modal snapshot |
| `Gansi::GansiWindow : WindowBase` | shared back-buffer ref | logical rect + clip + cursor; **no per-window texture** |
| `Gansi::GansiDrawContext : DrawContext` | `Terminal::At` | all draw virtuals → cell writes |
| `Gansi::GansiKeyboardDriver : KeyboardDriverBase` | `gnilk::ansi::KeyEvent` | → `KeyPress`; shared modifier translation for mouse |

**One shared back-buffer, windows are clip rects.** Like the SDL backends already do (main.cpp TODO:
"not using the underlying windowing mechanism… reposition everything myself"), the TTY backend
composites every window into a single `gnilk::ansi::Terminal` grid. `CreateWindow` returns a logical
rect; `GansiDrawContext` writes absolute cells = `(window.TopLeft() + local)`, clipped to the window
rect. `UpdateWindow` just updates the rect. `GansiScreen::Update()` = `Terminal::Flush()`.

### 5.1 Coordinate translation (special-care #1)

Near-identity vs SDL. The DrawContext offsets local (x,y) by the window origin and clips to the window
rect — no `fac_x_to_rc` scaling, no font metrics. Model stays logical (char indices); the *only* visual
conversion the editor already does (`Line::CharToVisualColumn` for tabs) is unchanged. Input mouse
coordinates arrive as cells from the terminal — no `PixelToRowCol`. The gotchas are the *sub-cell*
primitives SDL draws in pixels (see §5.3).

### 5.2 Keyboard & mouse input (special-care #2)

* **Keyboard:** enable the **Kitty keyboard protocol** (progressive enhancement, `CSI u`) so
  `Shift`/`Ctrl`/`Alt` combos and key-release arrive *unambiguously* — this is the whole reason
  "modern terminals only" buys us correctness. It directly fixes the two pains the SDL notes call out:
  **Shift-Arrow selection** and **Alt as the UI-navigation modifier** (`UINavigationModifier == Alt`).
  Fallback to `xterm modifyOtherKeys` where Kitty isn't supported; legacy bare-escape sequences as the
  floor. Map parsed events → `KeyPress`: printable → `key` (char32, UTF-8 decoded); named keys →
  `isSpecialKey` + `specialKey` (`Keyboard::kKeyCode_*`); `mods` → `Keyboard::kMod_*`. (Note: terminals
  can't tell left/right modifier; map to the `Left*` variants — the keymaps already accept either.)
* **Mouse:** enable **SGR mouse (1006)** + button + motion. Parse `CSI < b ; col ; row M/m` →
  `MouseEvent` (cells, 1-based→0-based). Wheel = buttons 64/65 → `wheelDelta`. `clicks` (double-click)
  is stamped centrally by `Runloop::ProcessMouseEvent` (`MouseClickTracker`) — the backend leaves it 1,
  same as SDL.
* **Pump:** `GansiScreen::PollEvents()` → `Terminal::PollEvent(250ms)`; for each event post to the
  Runloop (`Runloop::PostMessage(0, …ProcessKeyPress/ProcessMouseEvent)`), exactly like SDL3. A
  `ResizeEvent` (from the SIGWINCH flag) → `OnSizeChanged()` → recompute `Dimensions` →
  `RootView().Resize()` + `InvalidateAll()`. Threading model is unchanged (all on the main thread).

### 5.3 Primitive drawing (special-care #3)

| `DrawContext` op | TTY realisation |
|---|---|
| `DrawStringAt` / `…WithAttributesAt` | write glyphs into cells with current pen; `kInverted` swaps fg/bg; `kUnderline` → SGR underline attribute |
| `FillLine` / background fills | write spaces with the bg color across the cell span |
| `DrawHRule` (sub-cell line in SDL) | box-drawing row (`U+2500 ─`) on the row boundary — the op is intention-named for exactly this fallback (see `DrawContext.h`) |
| `Scroll` | grid row shift (or emit scroll region); cheap |
| `Clear` | fill grid with bg |

Underline/inverse/bold map to real SGR attributes (the cell carries `Attr`), so they're free.

### 5.4 Overlay handling (special-care #4)

Overlays (selection/search highlight) are already in **screen cell coords** (`DrawContext::Overlay`
with `IsInside`/`IsLinePartiallyCovered`). `DrawLineOverlays(y)` walks the covered cell span on row `y`
and sets each cell's background (or inverse) in the shared grid — composited naturally because the grid
*is* the back-buffer. No texture blending, no pixel rects.

### 5.5 Cursor, clipboard, modal snapshot, resize

* **Cursor:** the real terminal hardware cursor. `GansiScreen::Update()` positions it at the active
  window's cursor (`CUP`), sets shape via DECSCUSR, shows/hides around redraw. Replaces SDL's drawn
  `SDLCursor`.
* **Clipboard:** outbound via **OSC 52** in the `ClipBoard::SetClipboardChangeDelegate` hook (works over
  SSH). Inbound via **bracketed paste** → `ClipBoard::CopyFromExternal`. ⚠️ Honour the **E.18 lesson**:
  GUI copy/paste round-trips through the *external* item — unit-test the round-trip, reuse
  `ClipBoard::ClipBoardItem::AsText()`, don't shortcut through the internal item.
* **Modal snapshot** (`CopyToTexture`/`ClearWithTexture`): snapshot the back-buffer grid to a saved grid;
  restore before drawing the modal. Cheap memcpy of cells.
* **Resize:** `SIGWINCH` (handler sets atomic) → `TakePendingResize()` surfaces a `ResizeEvent` from
  `PollEvent` → `Terminal::Resize` (grid realloc, dimension fields tracked — same invariant the
  `TerminalScreen` notes stress: dims must equal the grid at all times) → `OnSizeChanged()`. No font
  metrics, so far simpler than SDL.

---

## 6. Build & runtime integration

* **CMake:** `add_subdirectory(src/ext/gansi)` → target `gansi`. Add the backend sources
  (`src/Core/Graphics/Gansi/*`) to the graphics source list and link `gansi`, guarded by a new
  `GEDIT_BUILD_ANSI` option (**default ON; coexists with SDL** — it is *not* the SDL2-XOR-SDL3 split).
  Define `GEDIT_USE_ANSI`. Backend sources compile into both `goatedit` and `utests` (like SDL).
* **Selection:** add `"ansi"` (alias `tty`/`terminal`?) to `glbSupportedBackends`; add
  `Editor::SetupAnsi()` next to `SetupSDL3`/`SetupHeadless` (create `GansiScreen` + `GansiKeyboardDriver`,
  register with `RuntimeConfig`/`UIHost`). **No** `WireScreenGeometry` (window geometry is N/A for a
  TTY — the terminal owns size). Selectable via `--backend ansi` or `config.yml main.backend: ansi`.
* **Terminal-session guard:** `ConfigureSubSystems()` currently *exits* on
  `isTerminal && !enforceSDL` ("NCurses discontinued"). Amend: a terminal session is now valid when
  `backend == ansi` (and arguably should *default* to `ansi` when `IsTerminalSession()` and no SDL UI
  launch). Decide default policy (§7).
* **NCurses removal:** delete `src/Core/Graphics/NCurses/` and the dead `Editor::SetupNCurses()` +
  any references. Out of scope to study — just remove.

---

## 7. Risks, terminal compatibility, open decisions

* **Terminal-compat matrix** (targets, all "modern"): kitty, WezTerm, foot, iTerm2, Windows Terminal,
  Ghostty, Alacritty, `tmux`/`screen` (passthrough + DCS wrapping caveats). Kitty-protocol and OSC-52
  support vary — degrade gracefully (modifyOtherKeys fallback; if OSC 52 is refused, internal clipboard
  still works locally). Document what each tier gives.
* **Nesting:** the embedded `TerminalView` (forkpty shell) now runs *inside* an editor that itself runs
  in a terminal. That's fine (a pty within a pty) but worth a smoke test.
* **`tmux`:** mouse, focus, and OSC 52 need `tmux` passthrough config; note it, don't chase it.
* **Settled:** library at **`src/ext/gansi`** (target/folder `gansi`, namespace `gnilk::ansi`);
  backend `gedit::Gansi`.
* **Open decisions (recommend, don't block):**
  1. Default backend in a terminal session — auto-`ansi`, or require explicit `--backend ansi`?
     *(Recommend: auto-`ansi` when `IsTerminalSession()` and not launched as a desktop app.)*
  2. Does the library's test target reuse `trun`, or ship a tiny standalone harness to keep it
     buildable with zero project tooling? *(Recommend: trun, but isolated target.)*

---

## 8. Phased work-item list

Phases are dependency-ordered. **Phases 1–2 are pure logic and fully unit-testable before any backend
or real TTY wiring** — write the discriminating tests first.

### Phase 0 — Groundwork & cleanup
- [ ] Remove `src/Core/Graphics/NCurses/` entirely; delete `Editor::SetupNCurses()` and any references.
- [ ] Scaffold library skeleton at `src/ext/gansi`: `CMakeLists.txt`
      (static target `gansi`, namespace `gnilk::ansi`), `include/gansi/`, `src/`, empty `tests/`.
- [ ] Wire `add_subdirectory(src/ext/gansi)` + `GEDIT_BUILD_ANSI` option (default ON, coexists with SDL);
      define `GEDIT_USE_ANSI`.
- [ ] Add `"ansi"` to `glbSupportedBackends`; add a stub `Editor::SetupAnsi()`; amend the
      terminal-session guard in `ConfigureSubSystems()` so `backend == ansi` is allowed.

### Phase 1 — Library: cell model + ANSI output (no real TTY)
- [ ] Core types: `Color`, `Attr`, `Cell`; double-buffered grid; `Resize` with bounds/invariants.
- [ ] `AnsiEncoder`: cell-diff → minimal ANSI (truecolor SGR, cursor move, pen coalescing, UTF-8 encode,
      clear, alt-screen enter/leave, cursor show/hide/shape).
- [ ] `ITerminalIO` interface + `MockTerminalIO` (in-memory) for tests.
- [ ] Unit tests: render→assert bytes; diff is minimal; pen-change coalescing; resize property tests.

### Phase 2 — Library: input parsing + platform I/O
- [ ] `InputParser`: UTF-8 decode; CSI/SS3 special keys; SGR mouse (1006); bracketed paste; focus
      in/out; **Kitty keyboard (CSI u)** with `modifyOtherKeys` fallback → neutral `Event`s. Tolerate
      split sequences.
- [ ] `PosixTerminalIO`: termios raw mode (RAII restore), `TIOCGWINSZ`, `SIGWINCH` atomic flag,
      `poll()` read-with-timeout, write flush.
- [ ] `Caps`: enable/disable sequence builders (alt-screen, mouse, bracketed paste, focus, kitty flags).
- [ ] `Terminal::Open/Close/PollEvent/Flush/SetCursor/SetClipboard` glue.
- [ ] Unit tests: byte-stream→event tables incl. modifier combos and partial reads (discriminating-first).

### Phase 3 — Backend: wire library to the `ScreenBase` contract
- [ ] `GansiScreen : ScreenBase` — Open (raw+alt+caps), Close (restore), `Dimensions()`=cols×rows,
      `Clear`/`Update`(=Flush), `PollEvents` (parse→post; ResizeEvent→`OnSizeChanged`→`RootView().Resize()`),
      `CopyToTexture`/`ClearWithTexture` (grid snapshot/restore).
- [ ] `GansiWindow : WindowBase` — logical rect + shared back-buffer ref + cursor.
- [ ] `GansiDrawContext : DrawContext` — all draw virtuals → cell writes; offset+clip; overlays→cell bg;
      `DrawHRule`→box-drawing; underline/inverse→SGR.
- [ ] `GansiKeyboardDriver : KeyboardDriverBase` — `gnilk::ansi::KeyEvent`→`KeyPress`; shared modifier
      translation for mouse parity.
- [ ] Cursor: position/shape the real terminal cursor on `Update`.

### Phase 4 — Integration, clipboard, polish, verification
- [ ] Real `Editor::SetupAnsi()`; decide & implement terminal-session default policy; config defaults.
- [ ] Clipboard: OSC 52 out (`SetClipboardChangeDelegate`); bracketed-paste in via `CopyFromExternal`
      — honour the **E.18** external round-trip (test it).
- [ ] Manual verification: local terminal + over SSH; nesting smoke test (embedded `TerminalView`).
- [ ] Terminal-compat matrix doc (kitty/iTerm2/WezTerm/foot/Windows Terminal/Ghostty/tmux tiers).
- [ ] Performance pass (diff coalescing, avoid full repaints); theme/color sanity.
- [ ] `docs/work-log.md` entry + link this doc; note `gansi`/`gnilk::ansi` is a standalone library
      (its own CMake + tests), not editor code.
```
