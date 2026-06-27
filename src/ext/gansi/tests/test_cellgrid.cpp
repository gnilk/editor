//
// gansi CellGrid tests — dimensions/blank fill, bounds-clamped access (incl. empty grid),
// resize content-preservation + the dims-track-buffer invariant.
//
#include <testinterface.h>

#include "gansi/CellGrid.h"

using namespace gnilk::ansi;

extern "C" {
DLL_EXPORT int test_cellgrid(ITesting *t);
DLL_EXPORT int test_cellgrid_dims(ITesting *t);
DLL_EXPORT int test_cellgrid_clamp(ITesting *t);
DLL_EXPORT int test_cellgrid_empty(ITesting *t);
DLL_EXPORT int test_cellgrid_clear(ITesting *t);
DLL_EXPORT int test_cellgrid_resize_grow(ITesting *t);
DLL_EXPORT int test_cellgrid_resize_shrink(ITesting *t);
DLL_EXPORT int test_cellgrid_resize_invariant(ITesting *t);
DLL_EXPORT int test_cellgrid_copyfrom(ITesting *t);
}

DLL_EXPORT int test_cellgrid(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_cellgrid_dims(ITesting *t) {
    CellGrid g(80, 24);
    TR_ASSERT(t, g.Cols() == 80);
    TR_ASSERT(t, g.Rows() == 24);
    TR_ASSERT(t, !g.IsEmpty());
    // Fresh cells are the blank default.
    Cell blank;
    TR_ASSERT(t, g.At(0, 0) == blank);
    TR_ASSERT(t, g.At(79, 23) == blank);
    return kTR_Pass;
}

DLL_EXPORT int test_cellgrid_clamp(ITesting *t) {
    CellGrid g(4, 3);
    g.At(2, 1).ch = U'Z';
    // Over-range indices clamp to the nearest valid cell — writing (99,99) hits (3,2).
    g.At(99, 99).ch = U'C';
    TR_ASSERT(t, g.At(3, 2).ch == U'C');
    // Negative clamps to 0.
    g.At(-5, -5).ch = U'N';
    TR_ASSERT(t, g.At(0, 0).ch == U'N');
    // The earlier in-range write is untouched.
    TR_ASSERT(t, g.At(2, 1).ch == U'Z');
    return kTR_Pass;
}

DLL_EXPORT int test_cellgrid_empty(ITesting *t) {
    CellGrid g;
    TR_ASSERT(t, g.Cols() == 0);
    TR_ASSERT(t, g.Rows() == 0);
    TR_ASSERT(t, g.IsEmpty());
    // Access on an empty grid must not crash (lands on the scratch cell).
    g.At(0, 0).ch = U'X';
    g.At(10, 10).ch = U'Y';
    TR_ASSERT(t, true);
    return kTR_Pass;
}

DLL_EXPORT int test_cellgrid_clear(ITesting *t) {
    CellGrid g(3, 2);
    g.At(1, 1).ch = U'Q';
    Color bg{10, 20, 30};
    g.Clear(bg);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            TR_ASSERT(t, g.At(x, y).ch == U' ');
            TR_ASSERT(t, g.At(x, y).bg == bg);
            TR_ASSERT(t, g.At(x, y).fg == kDefaultFg);
            TR_ASSERT(t, g.At(x, y).attr == Attr::None);
        }
    }
    return kTR_Pass;
}

DLL_EXPORT int test_cellgrid_resize_grow(ITesting *t) {
    CellGrid g(2, 2);
    g.At(0, 0).ch = U'A';
    g.At(1, 1).ch = U'D';
    g.Resize(4, 4);
    TR_ASSERT(t, g.Cols() == 4);
    TR_ASSERT(t, g.Rows() == 4);
    // Existing content preserved in place.
    TR_ASSERT(t, g.At(0, 0).ch == U'A');
    TR_ASSERT(t, g.At(1, 1).ch == U'D');
    // Newly exposed cells are blank.
    Cell blank;
    TR_ASSERT(t, g.At(3, 3) == blank);
    TR_ASSERT(t, g.At(2, 0) == blank);
    return kTR_Pass;
}

DLL_EXPORT int test_cellgrid_resize_shrink(ITesting *t) {
    CellGrid g(4, 4);
    g.At(0, 0).ch = U'A';
    g.At(3, 3).ch = U'Z';   // will be dropped
    g.Resize(2, 2);
    TR_ASSERT(t, g.Cols() == 2);
    TR_ASSERT(t, g.Rows() == 2);
    TR_ASSERT(t, g.At(0, 0).ch == U'A');
    // Growing back: dropped content does not reappear.
    g.Resize(4, 4);
    Cell blank;
    TR_ASSERT(t, g.At(3, 3) == blank);
    return kTR_Pass;
}

// Property: across an arbitrary resize sequence, Cols()/Rows() always equal the real buffer extent
// (every in-range cell is addressable) — the dims-track-buffer invariant.
DLL_EXPORT int test_cellgrid_resize_invariant(ITesting *t) {
    CellGrid g;
    const int sizes[][2] = {{10, 5}, {0, 0}, {1, 1}, {200, 50}, {3, 80}, {0, 9}, {7, 0}, {64, 64}};
    for (auto &s : sizes) {
        g.Resize(s[0], s[1]);
        int expCols = s[0] < 0 ? 0 : s[0];
        int expRows = s[1] < 0 ? 0 : s[1];
        TR_ASSERT(t, g.Cols() == expCols);
        TR_ASSERT(t, g.Rows() == expRows);
        TR_ASSERT(t, g.IsEmpty() == (expCols == 0 || expRows == 0));
        // Touch the extreme corners — must never go out of bounds.
        if (!g.IsEmpty()) {
            g.At(expCols - 1, expRows - 1).ch = U'*';
            TR_ASSERT(t, g.At(expCols - 1, expRows - 1).ch == U'*');
        }
    }
    return kTR_Pass;
}

DLL_EXPORT int test_cellgrid_copyfrom(ITesting *t) {
    CellGrid a(3, 2);
    a.At(1, 1).ch = U'M';
    CellGrid b;
    b.CopyFrom(a);
    TR_ASSERT(t, b.SameSizeAs(a));
    TR_ASSERT(t, b.At(1, 1).ch == U'M');
    // Deep copy: mutating the source must not change the copy.
    a.At(1, 1).ch = U'N';
    TR_ASSERT(t, b.At(1, 1).ch == U'M');
    return kTR_Pass;
}
