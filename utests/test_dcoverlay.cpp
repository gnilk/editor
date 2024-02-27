//
// Created by gnilk on 06.04.23.
//

#include <testinterface.h>
#include "Core/DrawContext.h"

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
    TR_ASSERT(t, overlay.IsInside(5,0));
    TR_ASSERT(t, overlay.IsInside(4,0));
    TR_ASSERT(t, overlay.IsInside(9,0));
    TR_ASSERT(t, overlay.IsInside(10,0));

    return kTR_Pass;
}

DLL_EXPORT int test_dcoverlay(ITesting *t) {

    return kTR_Pass;
}


