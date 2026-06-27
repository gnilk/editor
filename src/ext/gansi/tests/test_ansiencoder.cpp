//
// gansi AnsiEncoder tests — precise byte-stream assertions for UTF-8 encoding, SGR pen, cursor
// moves and the damage-tracked minimal diff (run coalescing, pen coalescing, multi-run, multi-row).
//
#include <testinterface.h>

#include <cstdio>
#include <string>

#include "gansi/AnsiEncoder.h"
#include "gansi/CellGrid.h"

using namespace gnilk::ansi;

extern "C" {
DLL_EXPORT int test_ansiencoder(ITesting *t);
DLL_EXPORT int test_ansiencoder_utf8(ITesting *t);
DLL_EXPORT int test_ansiencoder_movecursor(ITesting *t);
DLL_EXPORT int test_ansiencoder_sgr(ITesting *t);
DLL_EXPORT int test_ansiencoder_sequences(ITesting *t);
DLL_EXPORT int test_ansiencoder_nodamage(ITesting *t);
DLL_EXPORT int test_ansiencoder_singlecell(ITesting *t);
DLL_EXPORT int test_ansiencoder_coalesce_run(ITesting *t);
DLL_EXPORT int test_ansiencoder_penchange(ITesting *t);
DLL_EXPORT int test_ansiencoder_tworuns(ITesting *t);
DLL_EXPORT int test_ansiencoder_multirow(ITesting *t);
DLL_EXPORT int test_ansiencoder_sizemismatch(ITesting *t);
DLL_EXPORT int test_ansiencoder_rgb256(ITesting *t);
DLL_EXPORT int test_ansiencoder_sgr_256(ITesting *t);
DLL_EXPORT int test_ansiencoder_encodediff_256(ITesting *t);
}

static const Color kRed{255, 0, 0};
static const Color kGreen{0, 255, 0};
static const Color kBlack{0, 0, 0};
static const Color kWhite{255, 255, 255};

// Readable rendering of a byte string for mismatch diagnostics (ESC -> \e, other ctrl -> \xNN).
static std::string Escape(const std::string &s) {
    std::string out;
    for (unsigned char c : s) {
        if (c == 0x1b) {
            out += "\\e";
        } else if (c < 0x20 || c >= 0x7f) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\x%02x", c);
            out += buf;
        } else {
            out += static_cast<char>(c);
        }
    }
    return out;
}

static bool Expect(ITesting *t, const std::string &actual, const std::string &expected, const char *label) {
    if (actual != expected) {
        fprintf(stderr, "[%s] MISMATCH\n  expected: \"%s\"\n  actual:   \"%s\"\n",
                label, Escape(expected).c_str(), Escape(actual).c_str());
        return false;
    }
    return true;
}

static Cell Glyph(char32_t ch, Color fg, Color bg, Attr attr = Attr::None) {
    Cell c;
    c.ch = ch;
    c.fg = fg;
    c.bg = bg;
    c.attr = attr;
    return c;
}

DLL_EXPORT int test_ansiencoder(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_ansiencoder_utf8(ITesting *t) {
    TR_ASSERT(t, AnsiEncoder::EncodeUtf8(U'A') == "A");
    TR_ASSERT(t, AnsiEncoder::EncodeUtf8(U'é') == std::string("\xc3\xa9"));            // é
    TR_ASSERT(t, AnsiEncoder::EncodeUtf8(U'€') == std::string("\xe2\x82\xac"));        // €
    TR_ASSERT(t, AnsiEncoder::EncodeUtf8(U'\U0001f600') == std::string("\xf0\x9f\x98\x80")); // 😀
    return kTR_Pass;
}

DLL_EXPORT int test_ansiencoder_movecursor(ITesting *t) {
    // 0-based in, 1-based CUP out, row;col order.
    TR_ASSERT(t, Expect(t, AnsiEncoder::MoveCursor(0, 0), "\x1b[1;1H", "cup-origin"));
    TR_ASSERT(t, Expect(t, AnsiEncoder::MoveCursor(2, 4), "\x1b[5;3H", "cup-2-4"));
    return kTR_Pass;
}

DLL_EXPORT int test_ansiencoder_sgr(ITesting *t) {
    // Default blank pen.
    TR_ASSERT(t, Expect(t, AnsiEncoder::Sgr(Cell{}),
                        "\x1b[0;38;2;192;192;192;48;2;0;0;0m", "sgr-default"));
    // Red on black, no attrs.
    TR_ASSERT(t, Expect(t, AnsiEncoder::Sgr(Glyph(U' ', kRed, kBlack)),
                        "\x1b[0;38;2;255;0;0;48;2;0;0;0m", "sgr-red"));
    // Bold + underline, white on black — attrs in ascending SGR code order (1 then 4).
    TR_ASSERT(t, Expect(t, AnsiEncoder::Sgr(Glyph(U' ', kWhite, kBlack, Attr::Bold | Attr::Underline)),
                        "\x1b[0;1;4;38;2;255;255;255;48;2;0;0;0m", "sgr-bold-underline"));
    return kTR_Pass;
}

DLL_EXPORT int test_ansiencoder_sequences(ITesting *t) {
    TR_ASSERT(t, Expect(t, AnsiEncoder::EnterAltScreen(), "\x1b[?1049h", "alt-enter"));
    TR_ASSERT(t, Expect(t, AnsiEncoder::LeaveAltScreen(), "\x1b[?1049l", "alt-leave"));
    TR_ASSERT(t, Expect(t, AnsiEncoder::ShowCursor(), "\x1b[?25h", "cursor-show"));
    TR_ASSERT(t, Expect(t, AnsiEncoder::HideCursor(), "\x1b[?25l", "cursor-hide"));
    TR_ASSERT(t, Expect(t, AnsiEncoder::ClearScreen(), "\x1b[2J", "clear"));
    TR_ASSERT(t, Expect(t, AnsiEncoder::ResetSgr(), "\x1b[0m", "reset-sgr"));
    TR_ASSERT(t, Expect(t, AnsiEncoder::SetCursorShape(CursorShape::BarBlink), "\x1b[5 q", "shape-bar"));
    TR_ASSERT(t, Expect(t, AnsiEncoder::SetCursorShape(CursorShape::Default), "\x1b[0 q", "shape-default"));
    return kTR_Pass;
}

DLL_EXPORT int test_ansiencoder_nodamage(ITesting *t) {
    CellGrid front(4, 2);
    CellGrid back(4, 2);
    std::string out = "SENTINEL";
    AnsiEncoder::EncodeDiff(front, back, out);
    // Identical grids -> nothing appended.
    TR_ASSERT(t, out == "SENTINEL");
    return kTR_Pass;
}

DLL_EXPORT int test_ansiencoder_singlecell(ITesting *t) {
    CellGrid front(3, 1);
    CellGrid back(3, 1);
    back.At(1, 0) = Glyph(U'X', kRed, kBlack);
    std::string out;
    AnsiEncoder::EncodeDiff(front, back, out);
    std::string expected = std::string("\x1b[1;2H") + "\x1b[0;38;2;255;0;0;48;2;0;0;0m" + "X";
    TR_ASSERT(t, Expect(t, out, expected, "singlecell"));
    return kTR_Pass;
}

DLL_EXPORT int test_ansiencoder_coalesce_run(ITesting *t) {
    CellGrid front(3, 1);
    CellGrid back(3, 1);
    back.At(0, 0) = Glyph(U'A', kRed, kBlack);
    back.At(1, 0) = Glyph(U'B', kRed, kBlack);
    std::string out;
    AnsiEncoder::EncodeDiff(front, back, out);
    // One CUP, one SGR, then both glyphs.
    std::string expected = std::string("\x1b[1;1H") + "\x1b[0;38;2;255;0;0;48;2;0;0;0m" + "AB";
    TR_ASSERT(t, Expect(t, out, expected, "coalesce-run"));
    return kTR_Pass;
}

DLL_EXPORT int test_ansiencoder_penchange(ITesting *t) {
    CellGrid front(3, 1);
    CellGrid back(3, 1);
    back.At(0, 0) = Glyph(U'A', kRed, kBlack);
    back.At(1, 0) = Glyph(U'B', kGreen, kBlack);
    std::string out;
    AnsiEncoder::EncodeDiff(front, back, out);
    // One CUP (contiguous run), SGR changes mid-run when the pen changes.
    std::string expected = std::string("\x1b[1;1H")
                         + "\x1b[0;38;2;255;0;0;48;2;0;0;0m" + "A"
                         + "\x1b[0;38;2;0;255;0;48;2;0;0;0m" + "B";
    TR_ASSERT(t, Expect(t, out, expected, "penchange"));
    return kTR_Pass;
}

DLL_EXPORT int test_ansiencoder_tworuns(ITesting *t) {
    CellGrid front(3, 1);
    CellGrid back(3, 1);
    back.At(0, 0) = Glyph(U'A', kRed, kBlack);
    back.At(2, 0) = Glyph(U'C', kRed, kBlack);   // col 1 unchanged -> run breaks
    std::string out;
    AnsiEncoder::EncodeDiff(front, back, out);
    // Two CUPs (one per run); SGR coalesced across runs (pen unchanged).
    std::string expected = std::string("\x1b[1;1H") + "\x1b[0;38;2;255;0;0;48;2;0;0;0m" + "A"
                         + "\x1b[1;3H" + "C";
    TR_ASSERT(t, Expect(t, out, expected, "tworuns"));
    return kTR_Pass;
}

DLL_EXPORT int test_ansiencoder_multirow(ITesting *t) {
    CellGrid front(2, 2);
    CellGrid back(2, 2);
    back.At(0, 0) = Glyph(U'A', kRed, kBlack);
    back.At(0, 1) = Glyph(U'B', kRed, kBlack);
    std::string out;
    AnsiEncoder::EncodeDiff(front, back, out);
    // One CUP per row; SGR coalesced across rows.
    std::string expected = std::string("\x1b[1;1H") + "\x1b[0;38;2;255;0;0;48;2;0;0;0m" + "A"
                         + "\x1b[2;1H" + "B";
    TR_ASSERT(t, Expect(t, out, expected, "multirow"));
    return kTR_Pass;
}

DLL_EXPORT int test_ansiencoder_sizemismatch(ITesting *t) {
    CellGrid front(3, 1);
    CellGrid back(4, 2);
    back.At(0, 0) = Glyph(U'A', kRed, kBlack);
    std::string out;
    AnsiEncoder::EncodeDiff(front, back, out);
    // Mismatched sizes -> safe no-op (caller is expected to keep grids in lockstep).
    TR_ASSERT(t, out.empty());
    return kTR_Pass;
}

// Nearest xterm-256 index: pure colours map onto the 6x6x6 cube, neutral grays onto the ramp.
DLL_EXPORT int test_ansiencoder_rgb256(ITesting *t) {
    TR_ASSERT(t, AnsiEncoder::Rgb256(kBlack) == 16);             // cube (0,0,0)
    TR_ASSERT(t, AnsiEncoder::Rgb256(kWhite) == 231);            // cube (5,5,5)
    TR_ASSERT(t, AnsiEncoder::Rgb256(kRed) == 196);             // cube (5,0,0)
    TR_ASSERT(t, AnsiEncoder::Rgb256(Color{128, 128, 128}) == 244); // grayscale ramp (exact)
    return kTR_Pass;
}

// Indexed256 SGR uses 38;5;n / 48;5;n — never 24-bit 38;2.
DLL_EXPORT int test_ansiencoder_sgr_256(ITesting *t) {
    std::string s = AnsiEncoder::Sgr(Glyph(U' ', kRed, kBlack), ColorMode::Indexed256);
    TR_ASSERT(t, Expect(t, s, "\x1b[0;38;5;196;48;5;16m", "sgr-256-red"));
    // Truecolor mode (and the default) still emit 38;2.
    TR_ASSERT(t, AnsiEncoder::Sgr(Glyph(U' ', kRed, kBlack), ColorMode::Truecolor).find("38;2") != std::string::npos);
    TR_ASSERT(t, AnsiEncoder::Sgr(Glyph(U' ', kRed, kBlack)).find("38;2") != std::string::npos);
    return kTR_Pass;
}

// EncodeDiff threads the colour mode through to every pen it emits.
DLL_EXPORT int test_ansiencoder_encodediff_256(ITesting *t) {
    CellGrid front(3, 1);
    CellGrid back(3, 1);
    back.At(1, 0) = Glyph(U'X', kRed, kBlack);
    std::string out;
    AnsiEncoder::EncodeDiff(front, back, out, ColorMode::Indexed256);
    TR_ASSERT(t, out.find("38;5;196") != std::string::npos);
    TR_ASSERT(t, out.find("38;2") == std::string::npos);
    return kTR_Pass;
}
