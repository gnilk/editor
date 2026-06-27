//
// gansi core value types — truecolor cell model. Pure data, no platform deps.
//
#ifndef GANSI_TYPES_H
#define GANSI_TYPES_H

#include <cstdint>

namespace gnilk::ansi {

    // 24-bit truecolor. No palette indirection — the editor works in truecolor end-to-end.
    struct Color {
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;

        bool operator==(const Color &other) const {
            return r == other.r && g == other.g && b == other.b;
        }
        bool operator!=(const Color &other) const {
            return !(*this == other);
        }
    };

    // Cell attribute bits. Values are bit flags (combine with the operators below), NOT SGR codes
    // — AnsiEncoder maps each bit to its SGR parameter on the way out.
    enum class Attr : uint16_t {
        None      = 0,
        Bold      = 1u << 0,
        Dim       = 1u << 1,
        Italic    = 1u << 2,
        Underline = 1u << 3,
        Inverse   = 1u << 4,
    };

    inline Attr operator|(Attr a, Attr b) {
        return static_cast<Attr>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
    }
    inline Attr operator&(Attr a, Attr b) {
        return static_cast<Attr>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
    }
    inline Attr operator~(Attr a) {
        return static_cast<Attr>(~static_cast<uint16_t>(a));
    }
    inline Attr &operator|=(Attr &a, Attr b) {
        a = a | b;
        return a;
    }
    inline Attr &operator&=(Attr &a, Attr b) {
        a = a & b;
        return a;
    }
    // True if any bit in `flag` is set in `value`.
    inline bool HasAttr(Attr value, Attr flag) {
        return (static_cast<uint16_t>(value) & static_cast<uint16_t>(flag)) != 0;
    }

    // Default cell colors — used by CellGrid::Clear()/blank fill. A real backend overwrites these
    // with theme colors; they exist so a freshly-cleared grid is legible (light-grey on black).
    inline constexpr Color kDefaultFg{192, 192, 192};
    inline constexpr Color kDefaultBg{0, 0, 0};

    // One grid cell: a glyph + its pen (fg/bg/attr). `ch` is a full char32_t code point.
    struct Cell {
        char32_t ch = U' ';
        Color fg = kDefaultFg;
        Color bg = kDefaultBg;
        Attr attr = Attr::None;

        bool operator==(const Cell &other) const {
            return ch == other.ch && fg == other.fg && bg == other.bg && attr == other.attr;
        }
        bool operator!=(const Cell &other) const {
            return !(*this == other);
        }
    };

    // Hardware cursor shape (DECSCUSR codes). Default == terminal default.
    enum class CursorShape : int {
        Default         = 0,
        BlockBlink      = 1,
        BlockSteady     = 2,
        UnderlineBlink  = 3,
        UnderlineSteady = 4,
        BarBlink        = 5,
        BarSteady       = 6,
    };

}

#endif // GANSI_TYPES_H
