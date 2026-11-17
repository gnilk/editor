//
// The generic number matcher - see the header.
//

#include "GenericNumberMatcher.h"

using namespace gedit;

namespace {
    bool IsDigit(char32_t ch) {
        return (ch >= U'0') && (ch <= U'9');
    }

    bool IsHexDigit(char32_t ch) {
        return IsDigit(ch) || ((ch >= U'a') && (ch <= U'f')) || ((ch >= U'A') && (ch <= U'F'));
    }

    bool IsWordChar(char32_t ch) {
        return IsDigit(ch) || ((ch >= U'a') && (ch <= U'z')) || ((ch >= U'A') && (ch <= U'Z')) || (ch == U'_');
    }

    // Digits (as 'isDigit' says) and '_' separators from 'pos'; how many
    size_t SkipDigits(const std::u32string_view &input, size_t pos, bool (*isDigit)(char32_t)) {
        auto start = pos;
        while ((pos < input.size()) && (isDigit(input[pos]) || (input[pos] == U'_'))) {
            pos++;
        }
        return pos - start;
    }
}

int GenericNumberMatcher::Match(const std::u32string_view &input) {
    if (input.empty()) {
        return 0;
    }
    size_t pos = 0;
    auto at = [&input](size_t idx) -> char32_t {
        return (idx < input.size()) ? input[idx] : U'\0';
    };

    if ((at(0) == U'0') && ((at(1) == U'x') || (at(1) == U'X') || (at(1) == U'o') || (at(1) == U'O') ||
                            (at(1) == U'b') || (at(1) == U'B'))) {
        auto prefix = at(1);
        auto isDigit = IsHexDigit;
        if ((prefix == U'o') || (prefix == U'O')) {
            isDigit = [](char32_t ch) { return (ch >= U'0') && (ch <= U'7'); };
        } else if ((prefix == U'b') || (prefix == U'B')) {
            isDigit = [](char32_t ch) { return (ch == U'0') || (ch == U'1'); };
        }
        auto numDigits = SkipDigits(input, 2, isDigit);
        if (numDigits == 0) {
            return 0;
        }
        pos = 2 + numDigits;
    } else {
        // Integer part - or none, for '.5'
        pos = SkipDigits(input, 0, IsDigit);
        if ((pos == 0) && !((at(0) == U'.') && IsDigit(at(1)))) {
            return 0;
        }
        // Fraction: only with a digit after the dot ('1.' then a word is not a number's)
        if ((at(pos) == U'.') && IsDigit(at(pos + 1))) {
            pos += 1 + SkipDigits(input, pos + 1, IsDigit);
        }
        // Exponent: 'e' only when digits follow
        if ((at(pos) == U'e') || (at(pos) == U'E')) {
            auto expPos = pos + 1;
            if ((at(expPos) == U'+') || (at(expPos) == U'-')) {
                expPos++;
            }
            if (IsDigit(at(expPos))) {
                pos = expPos + SkipDigits(input, expPos, IsDigit);
            }
        }
    }
    if ((pos < input.size()) && (suffixes.find(input[pos]) != std::u32string::npos)) {
        pos++;
    }
    // Glued to a word: not a number at all
    if ((pos < input.size()) && IsWordChar(input[pos])) {
        return 0;
    }
    return static_cast<int>(pos);
}
