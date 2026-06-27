//
// gansi ANSI encoder — turns a cell-grid diff into the minimal ANSI/VT byte stream. Pure logic,
// fully testable without a real TTY: feed two grids, assert the emitted bytes.
//
// Minimality contract:
//   * only DAMAGED cells (back != front) are emitted;
//   * the cursor is positioned (CUP) once per contiguous damaged run, not per cell — the terminal
//     auto-advances the cursor as glyphs print;
//   * an SGR pen sequence is emitted only when the pen (fg/bg/attr) actually changes, coalesced
//     across runs and rows within a single EncodeDiff call.
//
#ifndef GANSI_ANSIENCODER_H
#define GANSI_ANSIENCODER_H

#include <string>
#include "gansi/Types.h"
#include "gansi/CellGrid.h"

namespace gnilk::ansi {

    // How a cell's colour is encoded into SGR. Truecolor emits 24-bit (38;2;r;g;b); Indexed256 degrades
    // each colour to the nearest xterm-256 palette index (38;5;n) for terminals without 24-bit support.
    enum class ColorMode {
        Truecolor,
        Indexed256,
    };

    class AnsiEncoder {
    public:
        // Diff `back` against `front`, appending the minimal ANSI byte stream to `out`. Grids must be
        // the same size; if they differ (or either is empty) nothing is written. Emits content only —
        // it does not move/show the hardware cursor (the caller does that after the flush).
        static void EncodeDiff(const CellGrid &front, const CellGrid &back, std::string &out,
                               ColorMode mode = ColorMode::Truecolor);

        // --- Pure sequence builders (also used directly by the Terminal glue) ---

        // CSI CUP — absolute cursor position. col/row are 0-based; the wire form is 1-based.
        static std::string MoveCursor(int col, int row);

        // SGR for a cell's pen: full reset (0) then attrs (ascending SGR code) then fg + bg in the
        // requested colour mode.
        static std::string Sgr(const Cell &pen, ColorMode mode = ColorMode::Truecolor);

        // Nearest xterm-256 palette index (16..255) for a 24-bit colour — best of the 6x6x6 colour cube
        // and the 24-step grayscale ramp. Exposed for testing.
        static int Rgb256(Color c);

        // UTF-8 encoding of a single code point (1..4 bytes).
        static std::string EncodeUtf8(char32_t ch);

        // Screen / mode control.
        static std::string EnterAltScreen();        // CSI ? 1049 h
        static std::string LeaveAltScreen();         // CSI ? 1049 l
        static std::string ShowCursor();             // CSI ? 25 h
        static std::string HideCursor();             // CSI ? 25 l
        static std::string ClearScreen();            // CSI 2 J
        static std::string SetCursorShape(CursorShape shape);  // DECSCUSR  (CSI <n> SP q)
        static std::string ResetSgr();               // CSI 0 m
    };

}

#endif // GANSI_ANSIENCODER_H
