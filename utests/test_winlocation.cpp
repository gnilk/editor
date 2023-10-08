//
// Created by gnilk on 08.10.23.
//
#include <testinterface.h>
#include "Core/WindowLocation.h"
#include "Core/RuntimeConfig.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_winloc(ITesting *t);
DLL_EXPORT int test_winloc_save(ITesting *t);
DLL_EXPORT int test_winloc_load(ITesting *t);
DLL_EXPORT int test_winloc_changed(ITesting *t);
}

DLL_EXPORT int test_winloc(ITesting *t) {
    t->CaseDepends("load", "save");
    t->CaseDepends("changed", "save");
    return kTR_Pass;
}
DLL_EXPORT int test_winloc_load(ITesting *t) {
    WindowLocation winLocation;

    TR_ASSERT(t, winLocation.Load());
    return kTR_Pass;

}
DLL_EXPORT int test_winloc_changed(ITesting *t) {
    WindowLocation winLocation;
    TR_ASSERT(t, winLocation.Load());
    TR_ASSERT(t, winLocation.Width() == WindowLocation::default_width);
    TR_ASSERT(t, winLocation.Height() == WindowLocation::default_height);
    TR_ASSERT(t, winLocation.XPos() == WindowLocation::default_xpos);
    TR_ASSERT(t, winLocation.YPos() == WindowLocation::default_ypos);
    // Change it
    winLocation.SetXPos(WindowLocation::default_xpos + 10);
    winLocation.SetYPos(WindowLocation::default_ypos + 11);
    winLocation.SetWidth(WindowLocation::default_width + 12);
    winLocation.SetHeight(WindowLocation::default_height + 13);
    winLocation.Save();

    WindowLocation winLoc2;
    TR_ASSERT(t, winLoc2.Load());
    TR_ASSERT(t, winLoc2.XPos() == WindowLocation::default_xpos+10);
    TR_ASSERT(t, winLoc2.YPos() == WindowLocation::default_ypos+11);
    TR_ASSERT(t, winLoc2.Width() == WindowLocation::default_width+12);
    TR_ASSERT(t, winLoc2.Height() == WindowLocation::default_height+13);
    // Don't save..
    return kTR_Pass;
}
DLL_EXPORT int test_winloc_save(ITesting *t) {
    WindowLocation winLocation;

    winLocation.Save();
    return kTR_Pass;
}
