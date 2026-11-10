//
// gvt — CharWidth over the generated range tables (WidthTables.inc, tools/gen_width.py).
//
#include <algorithm>
#include <iterator>

#include "gvt/CharWidth.h"

namespace {

    struct CodepointRange {
        char32_t first;
        char32_t last;
    };

#include "WidthTables.inc"

    template<size_t N>
    bool IsInRanges(const CodepointRange (&table)[N], char32_t codepoint) {
        auto it = std::upper_bound(std::begin(table), std::end(table), codepoint,
                                   [](char32_t value, const CodepointRange &range) {
                                       return value < range.first;
                                   });
        if (it == std::begin(table)) {
            return false;
        }
        --it;
        return codepoint <= it->last;
    }

}

int gnilk::vt::CharWidth(char32_t codepoint) {
    // Latin-1 and everything below the first combining mark: one cell
    if (codepoint < 0x0300) {
        return 1;
    }
    if (IsInRanges(kZeroWidthRanges, codepoint)) {
        return 0;
    }
    if (IsInRanges(kWideRanges, codepoint)) {
        return 2;
    }
    return 1;
}
