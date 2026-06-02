//
// Created by gnilk on 02.06.2026.
//
// Unit tests for Line's character-index <-> visual-column mapping (tab expansion).
//
#include <testinterface.h>
#include "Core/Line.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_linelayout(ITesting *t);
DLL_EXPORT int test_linelayout_char2vis_notabs(ITesting *t);
DLL_EXPORT int test_linelayout_char2vis_tabs(ITesting *t);
DLL_EXPORT int test_linelayout_vis2char_tabs(ITesting *t);
DLL_EXPORT int test_linelayout_roundtrip(ITesting *t);
DLL_EXPORT int test_linelayout_edge(ITesting *t);
}

// Note: Line's ctor rtrims, so test strings must not end in whitespace.
static int C2V(const std::u32string &s, int idx, int ts) {
    Line line(s);
    return line.CharToVisualColumn(idx, ts);
}
static int V2C(const std::u32string &s, int col, int ts) {
    Line line(s);
    return line.VisualToCharIndex(col, ts);
}

DLL_EXPORT int test_linelayout(ITesting *t) {
    return kTR_Pass;
}

// Without tabs, char index and visual column are identical
DLL_EXPORT int test_linelayout_char2vis_notabs(ITesting *t) {
    for (int i = 0; i <= 5; i++) {
        TR_ASSERT(t, C2V(U"hello", i, 4) == i);
    }
    return kTR_Pass;
}

// Tabs advance to the next multiple of tabSize
DLL_EXPORT int test_linelayout_char2vis_tabs(ITesting *t) {
    // "a\tb" with tabSize 4: a@0, tab@1 spans 1->4, b@4, end@5
    TR_ASSERT(t, C2V(U"a\tb", 0, 4) == 0);
    TR_ASSERT(t, C2V(U"a\tb", 1, 4) == 1);
    TR_ASSERT(t, C2V(U"a\tb", 2, 4) == 4);
    TR_ASSERT(t, C2V(U"a\tb", 3, 4) == 5);

    // leading tab: tab@0 spans 0->4, b@4
    TR_ASSERT(t, C2V(U"\tb", 1, 4) == 4);

    // two leading tabs: 0->4, 4->8, x@8
    TR_ASSERT(t, C2V(U"\t\tx", 2, 4) == 8);

    // mid-line tab: "ab\tc": a@0,b@1,tab@2 spans 2->4, c@4
    TR_ASSERT(t, C2V(U"ab\tc", 3, 4) == 4);

    // tabSize 8: "a\tb": tab at col1 -> 8
    TR_ASSERT(t, C2V(U"a\tb", 2, 8) == 8);

    // already on a tab-stop boundary still advances a full tab: "abcd\te"
    // a..d@0..3, tab@4 spans 4->8, e@8
    TR_ASSERT(t, C2V(U"abcd\te", 5, 4) == 8);
    return kTR_Pass;
}

// Mapping a visual column back to the character whose cell contains it
DLL_EXPORT int test_linelayout_vis2char_tabs(ITesting *t) {
    // "a\tb" tabSize 4: tab cell spans columns 1..3
    TR_ASSERT(t, V2C(U"a\tb", 0, 4) == 0);   // 'a'
    TR_ASSERT(t, V2C(U"a\tb", 1, 4) == 1);   // tab
    TR_ASSERT(t, V2C(U"a\tb", 3, 4) == 1);   // still inside tab cell
    TR_ASSERT(t, V2C(U"a\tb", 4, 4) == 2);   // 'b'
    TR_ASSERT(t, V2C(U"a\tb", 5, 4) == 3);   // past end -> length
    TR_ASSERT(t, V2C(U"a\tb", 99, 4) == 3);  // far past end -> length
    return kTR_Pass;
}

// CharToVisual then VisualToChar returns the original index for real character starts
DLL_EXPORT int test_linelayout_roundtrip(ITesting *t) {
    const std::u32string samples[] = { U"hello", U"a\tb", U"\t\tx", U"ab\tcd\tef" };
    for (auto &s : samples) {
        Line line(s);
        for (int i = 0; i < (int)line.Length(); i++) {
            int vis = line.CharToVisualColumn(i, 4);
            TR_ASSERT(t, line.VisualToCharIndex(vis, 4) == i);
        }
    }
    return kTR_Pass;
}

DLL_EXPORT int test_linelayout_edge(ITesting *t) {
    // empty line
    TR_ASSERT(t, C2V(U"", 0, 4) == 0);
    TR_ASSERT(t, V2C(U"", 0, 4) == 0);
    TR_ASSERT(t, V2C(U"", 10, 4) == 0);

    // tabSize < 1 is clamped to 1 (tab behaves as a single column)
    TR_ASSERT(t, C2V(U"a\tb", 2, 0) == 2);

    // negative visual column clamps to first char
    TR_ASSERT(t, V2C(U"abc", -5, 4) == 0);
    return kTR_Pass;
}
