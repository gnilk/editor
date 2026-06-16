//
// Created by gnilk on 06.04.23.
//

#include <testinterface.h>
#include "Core/UI/Graphics/DrawContext.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_dcoverlay(ITesting *t);
DLL_EXPORT int test_dcoverlay_isinside(ITesting *t);
}

DLL_EXPORT int test_dcoverlay_isinside(ITesting *t) {
    DrawContext::Overlay overlay;
    // included: 0..3
    Point start(0,0);
    Point end(0,4);
    overlay.isActive = true;
    overlay.Set(start, end);

    TR_ASSERT(t, overlay.IsLineFullyCovered(0));
    TR_ASSERT(t, overlay.IsLineFullyCovered(4));    // Not sure...

    start = Point(4,0);
    end = Point(10,0);
    overlay.Set(start, end);

    TR_ASSERT(t, !overlay.IsLineFullyCovered(0));
    TR_ASSERT(t, overlay.IsLinePartiallyCovered(0));

    // The overlay range is [start.x, end.x) - the END COLUMN IS EXCLUSIVE. This matches how overlays
    // are built from a selection (end == cursor position) and from a search result (start = cursor_x,
    // end = cursor_x + length), where exactly 'length' cells must light up. So for the range 4..10:
    //   - the start column (4) is inside,
    //   - columns up to and including 9 are inside,
    //   - the end column (10) is OUTSIDE (it is the first cell past the highlight),
    //   - and anything left of the start column is outside.
    TR_ASSERT(t, !overlay.IsInside(3,0));    // left of start  -> outside
    TR_ASSERT(t, overlay.IsInside(4,0));     // start column   -> inside (inclusive)
    TR_ASSERT(t, overlay.IsInside(5,0));
    TR_ASSERT(t, overlay.IsInside(9,0));     // last cell      -> inside
    TR_ASSERT(t, !overlay.IsInside(10,0));   // end column     -> outside (exclusive)
    TR_ASSERT(t, !overlay.IsInside(11,0));   // past end       -> outside

    // The bottom edge (end.y) is INCLUSIVE - a multi-line range covers its last row (up to end.x).
    start = Point(2,1);
    end = Point(3,3);
    overlay.Set(start, end);
    TR_ASSERT(t, !overlay.IsInside(5,0));    // row above start.y -> outside
    TR_ASSERT(t, !overlay.IsInside(1,1));    // start row, left of start.x -> outside
    TR_ASSERT(t, overlay.IsInside(2,1));     // start row, start.x -> inside
    TR_ASSERT(t, overlay.IsInside(0,2));     // a fully-spanned middle row -> inside
    TR_ASSERT(t, overlay.IsInside(2,3));     // end row, before end.x -> inside
    TR_ASSERT(t, !overlay.IsInside(3,3));    // end row, end.x -> outside (exclusive)
    TR_ASSERT(t, !overlay.IsInside(0,4));    // row below end.y -> outside

    return kTR_Pass;
}

DLL_EXPORT int test_dcoverlay(ITesting *t) {

    return kTR_Pass;
}


