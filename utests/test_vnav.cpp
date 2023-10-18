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

// tests the clion vertical navigation model
DLL_EXPORT int test_vnav_linedown(ITesting *t);
DLL_EXPORT int test_vnav_pagedown(ITesting *t);
DLL_EXPORT int test_vnav_lineup(ITesting *t);
DLL_EXPORT int test_vnav_pageup(ITesting *t);
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

static void PositionAbsolute(LineCursor &lineCursor, size_t idxActiveLine, int cursorYPos) {

    int dy = idxActiveLine - cursorYPos;
    if (dy < 0) dy = 0;

    lineCursor.idxActiveLine = idxActiveLine;
    lineCursor.viewTopLine = dy;
    lineCursor.viewBottomLine = viewRect.Height() + dy;
    lineCursor.cursor.position.x = 0;
    lineCursor.cursor.position.y = cursorYPos;
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

// Test various page-down movments...
DLL_EXPORT int test_vnav_pagedown(ITesting *t) {
    LineCursor lineCursor;
    DefaultLineCursor(lineCursor);

    VerticalNavigationViewModel vnav;
    vnav.lineCursor = &lineCursor;

    // Consistent with movement in editor
    size_t szPageSize = viewRect.Height()-1;

    // case 1) Move one page where there are just a few lines
    // Navigate from the top one page down - with only 10 items on the page
    vnav.OnNavigateDownCLion(szPageSize, viewRect, 10);
    TR_ASSERT(t, lineCursor.idxActiveLine == 9);
    TR_ASSERT(t, lineCursor.cursor.position.y == 9);
    TR_ASSERT(t, lineCursor.viewTopLine == 0);
    TR_ASSERT(t, lineCursor.viewBottomLine == 10);

    // case 2) Move beyond the last line of the view...
    // Navigate from the top one page down - with 100 items in the buffer
    // expect cursor to stay on top and everything to move szPageSize - 1
    // expect clipping
    DefaultLineCursor(lineCursor);
    PositionAbsolute(lineCursor, 80, 20);
    vnav.OnNavigateDownCLion(szPageSize, viewRect, 100);
    TR_ASSERT(t, lineCursor.idxActiveLine == 99);
    TR_ASSERT(t, lineCursor.cursor.position.y == viewRect.Height()-1);
    TR_ASSERT(t, lineCursor.viewTopLine == 100 - viewRect.Height());
    TR_ASSERT(t, lineCursor.viewBottomLine == 100);   // or 100??


    // case 3) Move from the top one page down in a buffer with many items
    // Navigate from the top one page down - with 100 items in the buffer
    // expect cursor to stay top move one line down and position
    DefaultLineCursor(lineCursor);
    PositionAbsolute(lineCursor, 0,0);
    vnav.OnNavigateDownCLion(szPageSize, viewRect, 100);
    TR_ASSERT(t, lineCursor.idxActiveLine == szPageSize);
    TR_ASSERT(t, lineCursor.cursor.position.y == 1);
    TR_ASSERT(t, lineCursor.viewTopLine == szPageSize);
    TR_ASSERT(t, lineCursor.viewBottomLine == lineCursor.viewTopLine + viewRect.Height());

    // case 4) Move one page from within the buffer and stay within the buffer
    DefaultLineCursor(lineCursor);
    PositionAbsolute(lineCursor, 40,20);
    auto topLineBefore = lineCursor.viewTopLine;
    auto bottomLineBefore = lineCursor.viewBottomLine;
    vnav.OnNavigateDownCLion(szPageSize, viewRect, 100);
    TR_ASSERT(t, lineCursor.idxActiveLine == 40 + szPageSize);
    TR_ASSERT(t, lineCursor.cursor.position.y == 21);
    TR_ASSERT(t, lineCursor.viewTopLine == topLineBefore + szPageSize);
    TR_ASSERT(t, lineCursor.viewBottomLine == bottomLineBefore + szPageSize);

    return kTR_Pass;
}

DLL_EXPORT int test_vnav_lineup(ITesting *t) {
    LineCursor lineCursor;
    DefaultLineCursor(lineCursor);

    VerticalNavigationViewModel vnav;
    vnav.lineCursor = &lineCursor;

    // case 1) move up from start - shouldn't move at all
    PositionAbsolute(lineCursor, 0, 0);
    vnav.OnNavigateUpCLion(1, viewRect, 10);
    TR_ASSERT(t, lineCursor.idxActiveLine == 0);
    TR_ASSERT(t, lineCursor.cursor.position.y == 0);
    TR_ASSERT(t, lineCursor.viewTopLine == 0);
    TR_ASSERT(t, lineCursor.viewBottomLine == viewRect.Height());

    // case 2) move up from within - should keep view and move one up
    PositionAbsolute(lineCursor, 5, 5);
    vnav.OnNavigateUpCLion(1, viewRect, 10);
    TR_ASSERT(t, lineCursor.idxActiveLine == 4);
    TR_ASSERT(t, lineCursor.cursor.position.y == 4);
    TR_ASSERT(t, lineCursor.viewTopLine == 0);
    TR_ASSERT(t, lineCursor.viewBottomLine == viewRect.Height());

    // case 4) move up from top line and there are more lines above
    PositionAbsolute(lineCursor, 75, 0);
    vnav.OnNavigateUpCLion(1, viewRect, 100);
    TR_ASSERT(t, lineCursor.idxActiveLine == 74);
    TR_ASSERT(t, lineCursor.cursor.position.y == 0);
    TR_ASSERT(t, lineCursor.viewTopLine == 74);
    TR_ASSERT(t, lineCursor.viewBottomLine == 74+viewRect.Height());

    // case 4) move up from top line and there are more lines above
    PositionAbsolute(lineCursor, 75, 10);
    vnav.OnNavigateUpCLion(1, viewRect, 100);
    TR_ASSERT(t, lineCursor.idxActiveLine == 74);
    TR_ASSERT(t, lineCursor.cursor.position.y == 9);
    TR_ASSERT(t, lineCursor.viewTopLine == 65);
    TR_ASSERT(t, lineCursor.viewBottomLine == 65+viewRect.Height());

    return kTR_Pass;
}
DLL_EXPORT int test_vnav_pageup(ITesting *t) {
    return kTR_Pass;
}
