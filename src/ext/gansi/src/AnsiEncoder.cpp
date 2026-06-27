//
// gansi ANSI encoder — cell-grid diff -> minimal ANSI/VT byte stream.
//
#include "gansi/AnsiEncoder.h"

#include <array>
#include <string>

using namespace gnilk::ansi;

// ESC + CSI introducer.
static constexpr const char *kCSI = "\x1b[";

// Attr bit -> SGR parameter, emitted in ascending code order so output is deterministic.
namespace {
    struct AttrCode {
        Attr bit;
        int sgr;
    };
    constexpr std::array<AttrCode, 5> kAttrCodes{{
        {Attr::Bold, 1},
        {Attr::Dim, 2},
        {Attr::Italic, 3},
        {Attr::Underline, 4},
        {Attr::Inverse, 7},
    }};

    // Pen equality — fg/bg/attr only (glyph is irrelevant to the SGR state).
    bool SamePen(const Cell &a, const Cell &b) {
        return a.fg == b.fg && a.bg == b.bg && a.attr == b.attr;
    }
}

void AnsiEncoder::EncodeDiff(const CellGrid &front, const CellGrid &back, std::string &out, ColorMode mode) {
    // Same-size, non-empty grids only — the caller keeps front/back in lockstep across a resize.
    if (front.IsEmpty() || back.IsEmpty() || !front.SameSizeAs(back)) {
        return;
    }

    const int cols = back.Cols();
    const int rows = back.Rows();

    bool penValid = false;
    Cell activePen;

    for (int y = 0; y < rows; ++y) {
        int x = 0;
        while (x < cols) {
            // Skip clean cells.
            if (back.At(x, y) == front.At(x, y)) {
                ++x;
                continue;
            }
            // Start of a contiguous damaged run — position the cursor once for the whole run.
            out += MoveCursor(x, y);
            while (x < cols && back.At(x, y) != front.At(x, y)) {
                const Cell &cell = back.At(x, y);
                if (!penValid || !SamePen(activePen, cell)) {
                    out += Sgr(cell, mode);
                    activePen = cell;
                    penValid = true;
                }
                out += EncodeUtf8(cell.ch);
                ++x;
            }
        }
    }
}

std::string AnsiEncoder::MoveCursor(int col, int row) {
    // CUP is 1-based and row-major (row ; col).
    return kCSI + std::to_string(row + 1) + ";" + std::to_string(col + 1) + "H";
}

std::string AnsiEncoder::Sgr(const Cell &pen, ColorMode mode) {
    std::string s = kCSI;
    s += "0";   // full reset first, then re-establish the whole pen
    for (const auto &ac : kAttrCodes) {
        if (HasAttr(pen.attr, ac.bit)) {
            s += ";" + std::to_string(ac.sgr);
        }
    }
    if (mode == ColorMode::Truecolor) {
        s += ";38;2;" + std::to_string(pen.fg.r) + ";" + std::to_string(pen.fg.g) + ";" + std::to_string(pen.fg.b);
        s += ";48;2;" + std::to_string(pen.bg.r) + ";" + std::to_string(pen.bg.g) + ";" + std::to_string(pen.bg.b);
    } else {
        s += ";38;5;" + std::to_string(Rgb256(pen.fg));
        s += ";48;5;" + std::to_string(Rgb256(pen.bg));
    }
    s += "m";
    return s;
}

int AnsiEncoder::Rgb256(Color c) {
    // The 6x6x6 colour cube uses these six levels per channel; the index is 16 + 36*r + 6*g + b.
    static constexpr int kLevels[6] = {0, 95, 135, 175, 215, 255};
    auto nearestLevel = [](int v) {
        int bestIdx = 0;
        int bestDist = 1 << 30;
        for (int i = 0; i < 6; ++i) {
            int d = v - kLevels[i];
            if (d < 0) {
                d = -d;
            }
            if (d < bestDist) {
                bestDist = d;
                bestIdx = i;
            }
        }
        return bestIdx;
    };
    auto dist2 = [](int r1, int g1, int b1, int r2, int g2, int b2) {
        int dr = r1 - r2, dg = g1 - g2, db = b1 - b2;
        return dr * dr + dg * dg + db * db;
    };

    const int ri = nearestLevel(c.r);
    const int gi = nearestLevel(c.g);
    const int bi = nearestLevel(c.b);
    const int cubeIdx = 16 + 36 * ri + 6 * gi + bi;
    const int cubeDist = dist2(c.r, c.g, c.b, kLevels[ri], kLevels[gi], kLevels[bi]);

    // Grayscale ramp candidate: indices 232..255 hold values 8 + 10*i (i in 0..23). Neutral grays land
    // far closer here than on the coarse cube.
    const int gray = (c.r + c.g + c.b) / 3;
    int grayStep = (gray - 8 + 5) / 10;   // round to nearest ramp step
    if (grayStep < 0) {
        grayStep = 0;
    }
    if (grayStep > 23) {
        grayStep = 23;
    }
    const int grayVal = 8 + 10 * grayStep;
    const int grayIdx = 232 + grayStep;
    const int grayDist = dist2(c.r, c.g, c.b, grayVal, grayVal, grayVal);

    return (grayDist < cubeDist) ? grayIdx : cubeIdx;
}

std::string AnsiEncoder::EncodeUtf8(char32_t ch) {
    std::string s;
    uint32_t c = static_cast<uint32_t>(ch);
    if (c < 0x80) {
        s += static_cast<char>(c);
    } else if (c < 0x800) {
        s += static_cast<char>(0xC0 | (c >> 6));
        s += static_cast<char>(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        s += static_cast<char>(0xE0 | (c >> 12));
        s += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (c & 0x3F));
    } else if (c <= 0x10FFFF) {
        s += static_cast<char>(0xF0 | (c >> 18));
        s += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
        s += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (c & 0x3F));
    } else {
        s += '?';   // out of Unicode range
    }
    return s;
}

std::string AnsiEncoder::EnterAltScreen() {
    return std::string(kCSI) + "?1049h";
}

std::string AnsiEncoder::LeaveAltScreen() {
    return std::string(kCSI) + "?1049l";
}

std::string AnsiEncoder::ShowCursor() {
    return std::string(kCSI) + "?25h";
}

std::string AnsiEncoder::HideCursor() {
    return std::string(kCSI) + "?25l";
}

std::string AnsiEncoder::ClearScreen() {
    return std::string(kCSI) + "2J";
}

std::string AnsiEncoder::SetCursorShape(CursorShape shape) {
    // DECSCUSR — CSI <n> SP q
    return std::string(kCSI) + std::to_string(static_cast<int>(shape)) + " q";
}

std::string AnsiEncoder::ResetSgr() {
    return std::string(kCSI) + "0m";
}
