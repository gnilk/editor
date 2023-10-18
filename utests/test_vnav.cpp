//
// Created by gnilk on 18.10.23.
//
//
// Created by gnilk on 15.10.23.
//
#include <testinterface.h>
#include "Core/Cursor.h"
#include "Core/VerticalNavigationViewModel.h"
#include "Core/RuntimeConfig.h"


using namespace gedit;
using namespace std::chrono_literals;

extern "C" {
DLL_EXPORT int test_vnav(ITesting *t);
DLL_EXPORT int test_vnav_exit(ITesting *t);
DLL_EXPORT int test_vnav_linedown(ITesting *t);
DLL_EXPORT int test_vnav_pagedown(ITesting *t);
}

static auto viewRect = gedit::Rect(80, 40);

static void DefaultLineCursor(LineCursor &lineCursor) {
    // Create default stuff
    lineCursor.idxActiveLine = 0;
    lineCursor.viewTopLine = 0;
    lineCursor.viewBottomLine = viewRect.Height();
    lineCursor.cursor.position.x = 0;
    lineCursor.cursor.position.y = 0;
    lineCursor.cursor.wantedColumn = 0;
}

DLL_EXPORT int test_vnav(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_vnav_exit(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_vnav_linedown(ITesting *t) {
    LineCursor lineCursor;
    DefaultLineCursor(lineCursor);

    VerticalNavigationViewModel vnav;
    vnav.lineCursor = &lineCursor;


    // Navigate from the top one line down
    vnav.OnNavigateDownCLion(1, viewRect, 10);
    TR_ASSERT(t, lineCursor.idxActiveLine == 1);
    TR_ASSERT(t, lineCursor.viewTopLine == 0);
    TR_ASSERT(t, lineCursor.viewBottomLine == viewRect.Height());

    // Try navigate from last line and one forward, this should stay
    lineCursor.idxActiveLine = 9;
    lineCursor.cursor.position.y = 9;
    vnav.OnNavigateDownCLion(1, viewRect, 10);
    TR_ASSERT(t, lineCursor.idxActiveLine == 9);
    TR_ASSERT(t, lineCursor.viewTopLine == 0);
    TR_ASSERT(t, lineCursor.viewBottomLine == viewRect.Height());


    // This should move the full view, we are on the last line and moving one line down
    lineCursor.idxActiveLine = 39;
    lineCursor.cursor.position.y = 39;
    vnav.OnNavigateDownCLion(1, viewRect, 100);

    TR_ASSERT(t, lineCursor.idxActiveLine == 40);
    TR_ASSERT(t, lineCursor.viewTopLine == 1);
    TR_ASSERT(t, lineCursor.viewBottomLine == 41);

    return kTR_Pass;
}

DLL_EXPORT int test_vnav_pagedown(ITesting *t) {
    LineCursor lineCursor;
    DefaultLineCursor(lineCursor);

    VerticalNavigationViewModel vnav;
    vnav.lineCursor = &lineCursor;

    // Consistent with movement in editor
    size_t szPageSize = viewRect.Height()-1;

    // Navigate from the top one page down - with only 10 items on the page
    vnav.OnNavigateDownCLion(szPageSize, viewRect, 10);
    TR_ASSERT(t, lineCursor.idxActiveLine == 9);
    TR_ASSERT(t, lineCursor.cursor.position.y == 0);
    TR_ASSERT(t, lineCursor.viewTopLine == 0);
    TR_ASSERT(t, lineCursor.viewBottomLine == viewRect.Height());

    // Navigate from the top one page down - with 100 items in the buffer
    // expect cursor to stay on top and everything to move szPageSize - 1
    // expect clipping
    DefaultLineCursor(lineCursor);
    vnav.OnNavigateDownCLion(szPageSize, viewRect, 100);
    TR_ASSERT(t, lineCursor.idxActiveLine == szPageSize-1);
    TR_ASSERT(t, lineCursor.cursor.position.y = 0);
    TR_ASSERT(t, lineCursor.viewTopLine == szPageSize-1);
    TR_ASSERT(t, lineCursor.viewBottomLine == lineCursor.viewTopLine + viewRect.Height());


    return kTR_Pass;
}

