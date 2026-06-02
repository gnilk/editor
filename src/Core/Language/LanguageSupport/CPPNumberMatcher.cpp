//
// Created by gnilk on 02.06.2026.
//

#include "CPPNumberMatcher.h"

using namespace gedit;

static inline bool isDec(char32_t c) { return (c >= U'0' && c <= U'9'); }
static inline bool isHex(char32_t c) {
    return (c >= U'0' && c <= U'9') || (c >= U'a' && c <= U'f') || (c >= U'A' && c <= U'F');
}
static inline bool isBin(char32_t c) { return (c == U'0' || c == U'1'); }
static inline bool isSep(char32_t c) { return (c == U'\''); }       // C++14 digit separator
static inline bool isSuffix(char32_t c) {
    return (c==U'u'||c==U'U'||c==U'l'||c==U'L'||c==U'f'||c==U'F'||c==U'z'||c==U'Z');
}

int CPPNumberMatcher::Match(const std::u32string_view &input) {
    size_t n = input.size();
    if (n == 0) {
        return 0;
    }

    char32_t c0 = input[0];

    // Policy: a number starts with a digit, or with '.' immediately followed by a digit (.5)
    bool startsWithDigit = isDec(c0);
    bool startsWithDotDigit = (c0 == U'.') && (n > 1) && isDec(input[1]);
    if (!startsWithDigit && !startsWithDotDigit) {
        return 0;
    }

    size_t i = 0;

    // Hex literal: 0x...
    if (c0 == U'0' && (n > 1) && (input[1] == U'x' || input[1] == U'X')) {
        i = 2;
        size_t startDigits = i;
        while ((i < n) && (isHex(input[i]) || isSep(input[i]))) { i++; }
        if (i == startDigits) { return 0; }     // "0x" with no digits is not a number
        while ((i < n) && isSuffix(input[i])) { i++; }
        return static_cast<int>(i);
    }

    // Binary literal: 0b...
    if (c0 == U'0' && (n > 1) && (input[1] == U'b' || input[1] == U'B')) {
        i = 2;
        size_t startDigits = i;
        while ((i < n) && (isBin(input[i]) || isSep(input[i]))) { i++; }
        if (i == startDigits) { return 0; }
        while ((i < n) && isSuffix(input[i])) { i++; }
        return static_cast<int>(i);
    }

    // Decimal (integer or floating-point)
    // integer part
    while ((i < n) && (isDec(input[i]) || isSep(input[i]))) { i++; }
    // fractional part
    if ((i < n) && (input[i] == U'.')) {
        i++;
        while ((i < n) && (isDec(input[i]) || isSep(input[i]))) { i++; }
    }
    // exponent
    if ((i < n) && (input[i] == U'e' || input[i] == U'E')) {
        size_t expStart = i;
        i++;
        if ((i < n) && (input[i] == U'+' || input[i] == U'-')) { i++; }
        size_t expDigits = i;
        while ((i < n) && isDec(input[i])) { i++; }
        // 'e' with no following digits is not part of the number - roll back
        if (i == expDigits) { i = expStart; }
    }
    // suffix
    while ((i < n) && isSuffix(input[i])) { i++; }

    return static_cast<int>(i);
}
