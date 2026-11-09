# gvt — terminal emulation library

A streaming DEC/ANSI (VT/xterm) parser, and later the screen model behind GoatEdit's embedded terminal.
Standalone: nothing here depends on the editor, only on the C++20 standard library. Namespace `gnilk::vt`.
The plan, design and work packages live in the editor repo: `docs/terminal-vt.md`.

## What's here (VT-0..VT-2)

- `Parser` (`include/gvt/Parser.h`) — Paul Flo Williams' state machine (<https://vt100.net/emu/dec_ansi_parser>)
  with the usual UTF-8-era changes (no C1 controls in 0x80-0x9F, `:` sub-parameters, OSC ends on BEL or ST,
  over-long sequences dropped). State persists between `Feed` calls: split reads parse like whole ones.
- `IParserSink` (`include/gvt/IParserSink.h`) — what the parser emits: `Print`, `Execute`, `CsiDispatch`,
  `EscDispatch`, `OscDispatch`, `DcsHook/Put/Unhook`, `ApcDispatch`. Implement it to do anything with a VT stream.

```cpp
class MySink : public gnilk::vt::IParserSink { /* ... */ };
MySink sink;
gnilk::vt::Parser parser(sink);
parser.Feed(bytes, length);     // any chunking
```

## Build and test

```sh
cmake -S src/ext/gvt -B build-gvt && cmake --build build-gvt --target gvttests
cd build-gvt && trun --sequential ./libgvttests.so       # .dylib on macOS
```

In the editor build the target is `gvttests` too (`libgvttests.*` next to `libutests.*`).

- `parser` module — per-state cases, test vectors ported from Ghostty's `Parser.zig`, split invariance (every
  cut of a corpus, every chunk size over the fixtures).
- `oracle` module — built only when libvterm is installed (`libvterm-dev`; macOS `brew install libvterm pkg-config`).
  Compares gvt's events with libvterm's parser on hand-built cases and on every fixture; deliberate differences
  are listed in `kKnownDivergences`. Also holds `OracleScreen` (libvterm's screen snapshot) for the screen work.
- Fixtures: `tests/fixtures/*.bin`, recorded with `tools/capture.py` — see `tests/fixtures/README.md`.
