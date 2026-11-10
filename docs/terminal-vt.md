# Terminal emulation library — `gvt` (streaming VT parser + screen, outside the editor)

**Status:** VT-0..VT-2 done on `feat/gvt` (2026-11-09) — see §7 for where things stand and how to pick up. Plan written
2026-11-09 against `main` @ `7b95bc3`.
**Replaces:** the implementation plan (§5) of [`terminal-enhancements.md`](terminal-enhancements.md). Its gap
analysis (§2–§4) and capture recipe (§7) stay valid and are referenced from here.

## 1. Decision

- **Keep our own emulator; do not embed libghostty.** libghostty-vt (1.0, 2026-11-06) is good and JetBrains now
  ships it, but it needs a Zig toolchain on every build machine, and most of the remaining terminal work (input
  routing, key/mouse/paste policy, rendering wide cells) would be ours anyway.
- **Rewrite `VTermParser` as a real streaming parser** — the DEC/ANSI state machine by Paul Flo Williams, the same
  design libvterm, Ghostty and Alacritty's `vte` use.
- **Move parser + screen model into a standalone library**, `src/ext/gvt` (namespace `gnilk::vt`), built the way
  `gansi` is: depends on nothing in the editor, own trun test library, `add_subdirectory` from the editor.
  Everything it can't decide itself goes out through an interface the host implements.
- **References, not dependencies:**
  - **libvterm** — the *test oracle*. Plain C, packaged (`libvterm-dev` / `brew install libvterm`). Test-only,
    optional target; never linked into `goatedit`.
  - **libghostty / Ghostty source** (MIT) — the *reading reference* for behaviour questions (pending wrap,
    query replies, kitty keyboard, grapheme width). Could become a second oracle if Zig ever lands on both
    machines; nothing in this plan depends on that.

### Reading list

| What | Link |
|---|---|
| Williams' parser (state diagram + tables — the spec for §3.1) | <https://vt100.net/emu/dec_ansi_parser> |
| xterm control sequences (the de-facto dialect) | <https://invisible-island.net/xterm/ctlseqs/ctlseqs.html> |
| VT100/VT220/VT510 manuals (DEC originals) | <https://vt100.net/docs/> |
| libvterm (oracle) | <https://www.leonerd.org.uk/code/libvterm/> |
| Ghostty terminal core (reference) | <https://github.com/ghostty-org/ghostty/tree/main/src/terminal> |
| Alacritty `vte` (compact Rust Williams implementation) | <https://github.com/alacritty/vte> |
| Unicode East Asian Width / emoji data (wide cells) | <https://www.unicode.org/reports/tr11/>, <https://www.unicode.org/reports/tr29/> |

## 2. What changes, what stays

| Today | After |
|---|---|
| `VTermParser` — per-chunk, builds a `CMD` list, loses split sequences | `gvt::Parser` — persistent state machine, byte-at-a-time safe, emits to an interface |
| `TerminalController::HandleAnsiCmd` — decodes `CMD`s into grid calls | `gvt::Terminal` — decodes sequences and owns the grid; the controller only handles what the library hands up |
| `TerminalScreen` — grid + pen + alt screen **and** scrollback-as-`TextBuffer` + `CommandBlock`s + `Config` lookups | Grid/pen/modes move into `gvt::Terminal`. Scrollback `TextBuffer`, command blocks, persistence stay in the editor, fed by callbacks |
| Cells store resolved `ColorRGBA` | Cells store *colour references* (default / palette index / RGB); the view resolves through the theme at draw — a theme switch recolours existing output |

**Stays in the editor, unchanged in spirit:** `Shell` (pty), the input policy in `TerminalController` (local line
editing, history, dot-commands via `PluginExecutor`, shell-owns-line, bootstrap), scrollback + command blocks +
`.bin` persistence, the Terminal JS API, `TerminalView` rendering.

## 3. Library design

Three layers. Each one is usable without the one above it.

```
 pty bytes ──► Parser ──IParserSink──► Terminal ──ITerminalHost──► editor (TerminalController)
                (syntax)                (semantics + grid)            replies, scrolled-out rows, OSC 133, title…
                                          ▲
                editor ◄── KeyEncoder / MouseEncoder / Paste (use Terminal's modes) ── key/mouse/paste from UI
```

### 3.1 `Parser` — syntax only

Williams' state machine verbatim (ground, escape, escape-intermediate, csi-entry/param/intermediate/ignore,
dcs-*, osc-string, sos/pm/apc-string), plus UTF-8 decoding in ground.

```cpp
namespace gnilk::vt {
    struct CsiParams {                       // parameters as received, nothing interpreted
        static constexpr size_t kMaxParams = 32;
        int32_t values[kMaxParams];          // -1 = omitted ("CSI ;5H"); clamped to 65535
        uint32_t colonMask;                  // bit n: the separator AFTER values[n] is ':' (Ghostty's convention)
        size_t count;                        // "CSI H" = 0, "CSI ;H" = 2
    };

    class IParserSink {
    public:
        virtual ~IParserSink() = default;
        virtual void Print(char32_t codepoint) = 0;                    // decoded UTF-8, ground state
        virtual void Execute(uint8_t c0) = 0;                          // BEL BS HT LF VT FF CR SO SI …
        virtual void CsiDispatch(const CsiParams &params, std::string_view intermediates,
                                 uint8_t prefix /* 0 or < = > ? */, uint8_t final) = 0;
        virtual void EscDispatch(std::string_view intermediates, uint8_t final) = 0;
        virtual void OscDispatch(std::string_view payload) = 0;       // terminated by BEL or ST
        virtual void DcsHook(const CsiParams &params, std::string_view intermediates, uint8_t prefix,
                             uint8_t final) {}
        virtual void DcsPut(uint8_t byte) {}
        virtual void DcsUnhook() {}
        virtual void ApcDispatch(std::string_view payload) {}         // kitty graphics etc. — ignored by default
    };

    class Parser {
    public:
        explicit Parser(IParserSink &sink);
        void Feed(const uint8_t *data, size_t length);   // any split, any size; state persists between calls
        void Reset();
    };
}
```

Decisions baked in:
- **No C1 interpretation of 0x80–0x9F** — the stream is UTF-8, those are continuation bytes (old doc §4.B.2).
  7-bit `ESC [`, `ESC ]`, `ESC \` only.
- An incomplete UTF-8 sequence is carried to the next `Feed`; an invalid one prints U+FFFD.
- OSC payload is capped (e.g. 64 KiB, enough for OSC 52); past the cap it is discarded, not buffered forever.
- The parser allocates nothing per byte; the OSC/DCS buffer is reused.

### 3.2 `Terminal` — the screen model (implements `IParserSink`)

Owns: primary + alternate grid, cursor, pen, pending-wrap flag, scroll region, tab stops, saved cursor (per
screen), modes (DECAWM, DECCKM, DECTCEM, bracketed paste, mouse modes + encoding, focus events, synchronized
output, kitty keyboard flags stack), title, palette overrides (OSC 4/10/11).

```cpp
    struct Color { enum class Kind : uint8_t { kDefault, kIndexed, kRgb } kind; uint8_t index, r, g, b; };
    struct Cell {
        char32_t codepoint = U' ';     // first codepoint of the grapheme
        uint32_t graphemeId = 0;       // 0 = single codepoint; otherwise key into the extra-codepoint table
        Color fg, bg;
        uint16_t attrs;                // bold dim italic underline(+style) blink inverse hidden strike overline
        uint8_t width;                 // 1, 2 = wide lead, 0 = wide continuation
    };

    class ITerminalHost {             // everything the model can't decide; all default to no-op
    public:
        virtual ~ITerminalHost() = default;
        virtual void WriteToPty(std::string_view bytes) {}               // DA/DSR/XTVERSION/DECRQM/OSC 10/11 replies
        virtual void OnRowScrolledOut(std::span<const Cell> row, bool isWrapped) {}  // primary screen, top of region
        virtual void OnSemanticPrompt(char kind, std::string_view params) {}         // OSC 133 A/B/C/D
        virtual void OnWorkingDirectory(std::string_view uri) {}                      // OSC 7
        virtual void OnTitle(std::string_view title) {}                              // OSC 0/2
        virtual void OnBell() {}
        virtual void OnClipboardWrite(std::string_view selection, std::string_view base64) {}  // OSC 52
        virtual void OnHyperlink(std::string_view params, std::string_view uri) {}   // OSC 8
        virtual void OnUnhandled(std::string_view description) {}                  // diagnostics / logging
    };

    class Terminal : public IParserSink {
    public:
        Terminal(ITerminalHost &host, int cols, int rows);
        void Feed(const uint8_t *data, size_t length);      // owns a Parser
        void Resize(int cols, int rows);                    // no reflow in v1 (see §6)
        const Cell &At(int x, int y) const;
        std::u32string_view Grapheme(const Cell &cell) const;
        Cursor GetCursor() const;                           // pos, visible, shape
        bool IsAltScreen() const;
        const Modes &GetModes() const;
        uint64_t GetGeneration() const;                     // bumps on any visible change — redraw cue
        bool IsSynchronizedUpdate() const;                  // CSI ?2026 — view defers drawing
    };
```

- **Scrollback is not in the library (v1).** Rows leaving the top of the primary screen go to
  `OnRowScrolledOut`; the editor turns them into `Line`s in its scrollback `TextBuffer` exactly as
  `TerminalScreen` does now. Keeps the block/persistence/JS work untouched.
- **OSC 133 / OSC 7 are passed up, not modelled** — command blocks remain an editor concept.
- **Thread model:** not thread-safe, same as today. The pty thread calls `Feed` under `screenLock`; the UI thread
  reads under the lock. `ITerminalHost` callbacks run synchronously inside `Feed` (they must not call `Feed`).
- **Wide cells:** a generated `wcwidth`-style table (East Asian Width W/F + emoji presentation, script in
  `src/ext/gvt/tools/`), grapheme clustering for combining marks / ZWJ / variation selectors (UAX #29 subset).
  The same table is useful to the editor for CJK/emoji in documents.

### 3.3 Input encoders — the other direction

Pure functions over `Terminal::GetModes()`:
- `EncodeKey(Key, Modifiers, modes)` → bytes: legacy xterm (`CSI 1;<mods> X`, `CSI n;<mods> ~`, Alt-as-ESC,
  Ctrl+symbol), DECCKM, and later the kitty keyboard protocol.
- `EncodeMouse(event, modes)` → SGR (`CSI < b;x;y M/m`) / X10.
- `WrapPaste(text, modes)` → bracketed when enabled, with `ESC[201~` stripped from the payload.
- `EncodeFocus(bool)`.

The library defines its own small `Key` enum; `TerminalController` maps the editor's `KeyPress` onto it. Whether a
key goes to the shell at all stays the controller's decision — **that is where the "hijacking" lives**, and the
library never sees keys the editor keeps.

## 4. Testing

All in the library's own trun target (`gvttests`), like `gansitests`.

1. **Parser unit tests** — per state transition, from Williams' tables; recording sink compares event lists.
2. **Split invariance (property)** — every fixture fed whole vs. split at every byte offset → identical event list /
   identical grid. This is the TE-1 bug class, closed for good.
3. **Differential vs libvterm** — `gvt-oracle` test module, built only when `pkg-config vterm` is found (otherwise a
   CMake message, module skipped). Feed the same bytes to `gvt::Terminal` and `VTermScreen`, compare per cell:
   text, width, fg/bg (mapped to a common form), bold/italic/underline/inverse, cursor position, alt-screen flag.
   **Known divergences** go in an explicit allowlist with a reason and a reference (xterm ctlseqs / Ghostty
   source) — the oracle informs, xterm behaviour decides.
4. **Fixtures** — `src/ext/gvt/tests/fixtures/`: hand-built byte streams (intent-revealing, preferred) plus scrubbed
   captures of `claude`, `vim`, `htop`, `less`, `fzf`, `git log --graph --color` (recipe: old doc §7). Each fixture
   has an expected-text snapshot for the cases where we diverge from libvterm on purpose.
5. **Editor-side tests** stay in `utests` (`test_terminalcontroller`, scrollback, blocks), fed through the new path.

## 5. Work packages

Each one: failing test first, then code. The editor keeps working after every package.

| WP | What | Notes / old-doc items covered |
|---|---|---|
| VT-0 | `src/ext/gvt` skeleton: CMake (standalone + `add_subdirectory`), `gvttests`, version stamp | mirrors `gansi` |
| VT-1 | `Parser` + `IParserSink`, UTF-8 carry-over, no C1, OSC/DCS/APC with BEL/ST, caps | TE-1, TE-2 (syntax half), §4.B.1/2/5/8 |
| VT-2 | Oracle harness (`gvt-oracle`, libvterm via pkg-config) + first fixtures + split-invariance test | §7 replay becomes permanent |
| VT-3 | `Terminal` core: print, C0, cursor moves (incl. `G d E F` `` ` ``), erase (`J K X`), insert/delete (`@ P L M`), scroll region + `S T`, `ESC 7/8`, `ESC M`, tab stops, `REP`, **pending wrap**, DECAWM, alt screen 47/1047/1049, `OnRowScrolledOut` | TE-3, TE-5, §4.B.3/7 |
| VT-4 | SGR complete: 16/256/RGB (`;` and `:` forms), set/clear individual attributes, colour references | TE-4 |
| VT-5 | Modes + replies: DECSET/DECRST list, DA1/DA2, DSR/CPR, XTVERSION, DECRQM, OSC 10/11/4 queries, OSC 0/2/7/8/52/133 → host | TE-2 (semantic half), §4.H |
| VT-6 | Wide cells + graphemes: generated width table, continuation cells, combining marks; erase/insert/delete keep wide pairs whole | TE-13 |
| VT-7 | **Editor switch-over:** `TerminalController` owns a `gvt::Terminal` + implements `ITerminalHost`; scrollback/blocks fed by `OnRowScrolledOut`/`OnSemanticPrompt`; `TerminalView` draws from `Terminal` cells (resolving colour refs via the theme). Delete `VTermParser`, `HandleAnsiCmd`, the grid half of `TerminalScreen` | big-bang avoided: VT-3..6 land in the lib first, behind tests |
| VT-8 | Input encoders (§3.3) + controller passthrough policy (foreground-job detection, escape-hatch set, paste, mouse, focus) | TE-6, TE-7, TE-9, TE-11, TE-12, §4.A/E/F/G |
| VT-9 | Rendering fidelity: live-cell attributes, cursor visibility/shape, synchronized output, wide glyphs in SDL2/SDL3/gansi draw contexts | TE-10, §4.D |
| VT-10 | Kitty keyboard protocol (flags stack + encoder) | only when an app needs it |

**Exit criterion** (same as the old Phase 1): `claude` inside goatedit — trust dialog answerable, main screen
correct, typed non-ASCII prompts, Esc interrupts, Shift+Tab cycles, Ctrl-C×2 back to a working shell — and the
oracle suite green modulo the documented allowlist.

## 6. Open decisions

1. **Name / location.** `src/ext/gvt`, `gnilk::vt` proposed. Separate from `gansi` because the direction is
   opposite (gansi *drives* a terminal, gvt *is* one); they could later share the colour/attr types.
2. **Reflow on resize.** v1: no reflow (as today). Ghostty reflows; libvterm reflows optionally. Revisit after
   VT-7 — it touches the editor's scrollback ownership.
3. **Scrollback in the library?** v1 no (editor owns it). If the library ever needs to stand alone as a full
   emulator, add an optional ring buffer behind `OnRowScrolledOut`.
4. **Local line editor vs. readline.** Unchanged by this plan (old doc §6.2). With OSC 133 input marks a later
   option is letting readline own the line and reading the typed command back on Enter for dot-commands.
5. **Font ligatures** (`->` as one glyph) are a renderer feature (HarfBuzz shaping over stb_truetype; token hint
   decides where shaping is allowed) — separate effort, shared with the editor view, not part of `gvt`.

## 7. Status and pick-up notes

### Done (VT-0..VT-5, branch `feat/gvt`)

- `src/ext/gvt/`: CMake (standalone + `add_subdirectory` from the editor), `Parser`, `IParserSink`, `README.md`.
  Not linked into `goatedit` yet.
- VT-1/2 tests: `parser` (per-state cases, Ghostty `Parser.zig` vectors ported, split invariance), `oracle`
  (gvt vs libvterm parser on hand-built cases and on every fixture: **all six fixtures match libvterm event for
  event**).
- Fixtures: `vim`, `less`, `git log --graph`, `ls --color`, `dialog --menu`, `cat` UTF-8 — recorded with
  `src/ext/gvt/tools/capture.py`, see `src/ext/gvt/tests/fixtures/README.md`.
- **VT-3** `Terminal` (`include/gvt/Terminal.h`, `src/Terminal.cpp`): `Cell`/`Color` refs/`Pen`/`Modes`,
  `ITerminalHost` (`WriteToPty`, `OnRowScrolledOut`, `OnBell`, `OnUnhandled` so far), print with pending wrap and
  soft-wrap flag per row, DECAWM, C0, CUU/CUD/CUF/CUB/CNL/CPL/CHA/HPA/HPR/VPA/VPR/CUP/HVP, ED/EL/ECH,
  ICH/DCH/IL/DL, DECSTBM + IND/RI/NEL/SU/SD, DECOM, HTS/TBC/CHT/CBT, REP, DECSC/DECRC + SCOSC/SCORC + ?1048,
  alt screen 47/1047/1049, DEC Special Graphics G0/G1 + SO/SI, IRM, LNM, DECALN, DECSTR, RIS, `Resize`
  (no reflow; rows above the cursor pushed out to the host). SGR handles reset only (VT-4).
  `gvttests` 49 cases green: `terminal` module (20, xterm-expected results) and `oracle` `terminal` /
  `terminalfixtures` — **vim, less, git log, ls, cat match libvterm's screen** row for row, cursor, alt flag and
  scrollback. `dialog_menu` waits for wide characters (VT-6).
- **VT-4** SGR complete (`Terminal::SelectGraphicRendition`): bold/dim/italic/underline/blink/inverse/hidden/
  strike/overline set + clear (22 clears bold and dim, 21 = double underline), 16/bright/256/RGB fg+bg, underline
  colour 58/59 and style `4:0..5` (`Cell::underlineColor`, `Cell::underlineStyle`), `;` and `:` colour forms
  (`38:2::r:g:b` T.416 with colorspace slot, `38:2:r:g:b`, kakoune's mixed form). A cut-short `;` colour ends the
  sequence (xterm); a `:` group on any other code is skipped whole; `:` on any CSI other than SGR drops it.
  `CompareScreens` now also compares every cell's attributes and colours: **git log and ls fixtures match libvterm
  cell for cell**. `gvttests`: 53 green.
- **VT-5** replies through `ITerminalHost::WriteToPty`: DA1 (`?62;22c`), DA2 (`>1;10;0c`), DSR 5, CPR / DECXCPR
  (origin-relative under DECOM), XTVERSION (`DCS >|gvt x.y.z ST`), DECRQM (ANSI + private), XTWINOPS 18. OSC 10/11/12
  and OSC 4 queries answered from `ITerminalHost::QueryColor` (reply in xterm's `rgb:rrrr/gggg/bbbb`, ST). OSC to the
  host: 0/2 `OnTitle`, 7 `OnWorkingDirectory` (raw URI), 8 `OnHyperlink`, 52 `OnClipboardWrite` (queries not
  answered), 133 `OnSemanticPrompt`. `Modes` gained cursor blink (?12), focus events (?1004), synchronized output
  (?2026, `IsSynchronizedUpdate()`), mouse tracking 9/1000/1002/1003 + encoding 1005/1006/1015/1016, DECSCUSR shape.
  The kitty keyboard query (`CSI ? u`) is deliberately unanswered (no kitty support yet - apps fall back).
  `gvttests`: 58 green.

Decisions made while building it:
- **Malformed CSI** (`CSI 1?2h`, `CSI 1$2p`): ignored up to the final byte (Williams `csi_ignore`, as xterm,
  Ghostty, vte). libvterm prints the tail instead — recorded in the oracle's `kKnownDivergences`.
- **Colon outside SGR** (`CSI 38:2h`): Ghostty drops it in the parser; gvt reports it and leaves the decision to
  the screen model (VT-3 ignores colons outside `m`).
- **CAN/SUB** are executed (handed to the sink) after cancelling; an OSC/APC cut by them is not dispatched, a
  hooked DCS is always unhooked.
- **"ESC \" after a string** is swallowed as the second half of ST; on its own it is dispatched as `ESC \`.
- libvterm reports 7-bit C1 escapes (`ESC M`) as C1 controls (0x8D); the oracle maps them back.
- Screen level, gvt follows xterm where libvterm differs (`kScreenDivergences`, each pinned in `test_terminal.cpp`):
  CUU/CUD from outside the scroll region stop at the margin (VT510); REP wraps; `CSI s`/`CSI u` save/restore
  (libvterm lacks them); `?47` switches screens (libvterm ignores it); RIS leaves the alternate screen;
  `ESC 8` with nothing saved restores the default pen (libvterm: black on black); `38:2::r:g:b` skips the
  T.416 colorspace slot (libvterm reads the channels one off); erase takes only the pen's background (libvterm
  keeps its foreground too). libvterm has no dim, overline, dotted/dashed underline: not compared.
- **Scrollback feed:** a row goes to `OnRowScrolledOut` when it leaves row 0 of the primary screen by LF/IND/SU/
  wrap with the region's top at row 0 (xterm; libvterm agrees on the fixtures), or when `Resize` shrinks below
  the cursor. DL at row 0, regions starting lower and the alternate screen never feed it. ED 3 goes to
  `OnUnhandled` for now (the editor owns scrollback).

### Pick up on macOS

```sh
brew install libvterm pkg-config          # without them the oracle module is skipped (parser tests still run)
git fetch && git checkout feat/gvt
cmake --preset debug                       # or reconfigure an existing build dir
cmake --build cmake-build-debug --target gvttests
cd cmake-build-debug && trun --sequential ./libgvttests.dylib     # expect 58 executed, 0 failed
```

Not yet verified on macOS: the build, the oracle with Homebrew's libvterm, the fixture copy step.

### Next: VT-6 (wide characters), then VT-7 (editor switch-over)

VT-6: generated East-Asian-Width + emoji table (`tools/`), `Cell::width` 2/0, wrap before a wide char that does not
fit, erase/insert/delete/overwrite keep pairs whole, combining marks join the previous cell (grapheme table).
Take `dialog_menu.bin` off `kScreenDivergences`. VT-7 plan: `TerminalScreen` keeps scrollback, blocks, abs-row
spine and persistence; its grid half becomes a `gvt::Terminal` with a resolved-colour row cache for the view;
`TerminalController` implements `ITerminalHost` (pty replies, OSC 133/7); `VTermParser` + `HandleAnsiCmd` go.
