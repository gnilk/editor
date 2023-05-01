//
// Created by gnilk on 15.04.23.
//
// This implements a common model for moving vertically in a list of items
//
//

#include <stdlib.h>

#include "logger.h"

#include "Rect.h"
#include "Cursor.h"
#include "VerticalNavigationViewModel.h"

using namespace gedit;

// This implements VSCode style of downwards navigation
// Cursor if moved first then content (i.e if standing on first-line, the cursor is moved to the bottom line on first press)
void VerticalNavigationViewModel::OnNavigateDownVSCode(Cursor &cursor, int rows, const Rect &rect, const size_t nItems) {

    auto logger = gnilk::Logger::GetLogger("VNavModel");

    logger->Debug("OnNavDownVSCode,  before, active list = %d", idxActiveLine);

    idxActiveLine+=rows;
    if (idxActiveLine > nItems-1) {
        idxActiveLine = nItems-1;
    }

    if (idxActiveLine > rect.Height()-1) {
        if (!(cursor.position.y < rect.Height()-1)) {
            if ((viewBottomLine + rows) < nItems) {
                logger->Debug("Clipping top/bottom lines");
                viewTopLine += rows;
                viewBottomLine += rows;
            } else {
                viewBottomLine = nItems;
                viewTopLine = viewBottomLine - rect.Height();
            }
        }
    }

    cursor.position.y = idxActiveLine - viewTopLine;
    if (cursor.position.y > rect.Height()-1) {
        cursor.position.y = rect.Height()-1;
    }
    logger->Debug("OnNavDownVSCode, rows=%d, new active line=%d", rows, idxActiveLine);
    logger->Debug("                 viewTopLine=%d, viewBottomLine=%d", viewTopLine, viewBottomLine);
    logger->Debug("                 cursor.pos.y=%d", cursor.position.y);

//    logger->Debug("OnNavDownVSCode, activeLine=%d, rows=%d, ypos=%d, height=%d", idxActiveLine, rows, cursor.position.y, rect.Height());
}

void VerticalNavigationViewModel::OnNavigateUpVSCode(Cursor &cursor, int rows, const Rect &rect, const size_t nItems) {
    idxActiveLine -= rows;
    if (idxActiveLine < 0) {
        idxActiveLine = 0;
    }

    cursor.position.y -= rows;
    if (cursor.position.y < 0) {
        int delta = 0 - cursor.position.y;
        cursor.position.y = 0;
        viewTopLine -= delta;
        viewBottomLine -= delta;
        if (viewTopLine < 0) {
            viewTopLine = 0;
            viewBottomLine = rect.Height();
        }
    }
}

// CLion/Sublime style of navigation on pageup/down - this first moves the content and then adjust cursor
// This moves content first and cursor rather stays
void VerticalNavigationViewModel::OnNavigateDownCLion(Cursor &cursor, int rows, const Rect &rect, const size_t nItems) {

    if (rows == 1) {
        return OnNavigateDownVSCode(cursor, rows, rect, nItems);
    }

    bool forceCursorToLastLine = false;
    int nRowsToMove = rows;
    int maxRows = nItems - 1;

    auto logger = gnilk::Logger::GetLogger("VNavModel");
    logger->Debug("OnNavDownCLion");

    // Note: We might want to revisit this clipping, if we want a margin to the bottom...
    // Maybe we should adjust for the visible margin at the end
    if ((viewTopLine+nRowsToMove) > maxRows) {
        //logger->Debug("  Move beyond last line!");
        nRowsToMove = 0;
        forceCursorToLastLine = true;
    }
    if ((viewBottomLine+nRowsToMove) > maxRows) {
        //logger->Debug("  Clip nRowsToMove");
        nRowsToMove = nItems - viewBottomLine;
        // If this results to zero, we are exactly at the bottom...
        if (!nRowsToMove) {
            forceCursorToLastLine = true;
        }
    }
    logger->Debug("  nRowsToMove=%d, forceCursor=%s, nLines=%d, maxRows=%d",
                  nRowsToMove, forceCursorToLastLine?"Y":"N", (int)nItems, maxRows);
    logger->Debug("  Before, topLine=%d, bottomLine=%d, activeLine=%d, cursor.y=%d", viewTopLine, viewBottomLine, idxActiveLine, cursor.position.y);


    // Reposition the view
    int activeLineDelta = idxActiveLine - viewTopLine;
    viewTopLine += nRowsToMove;
    viewBottomLine += nRowsToMove;

    // In case we would have moved beyond the visible part, let's enforce the cursor position..
    if (forceCursorToLastLine) {
        cursor.position.y = nItems - viewTopLine - 1;
        idxActiveLine = nItems-1;
        //logger->Debug("       force to last!");
    } else {
        idxActiveLine = viewTopLine + activeLineDelta;
        cursor.position.y = idxActiveLine - viewTopLine;
        if (cursor.position.y > rect.Height() - 1) {
            cursor.position.y = rect.Height() - 1;
        }
    }
    logger->Debug("  After, topLine=%d, bottomLine=%d, activeLine=%d, cursor.y=%d", viewTopLine, viewBottomLine, idxActiveLine, cursor.position.y);
}

void VerticalNavigationViewModel::OnNavigateUpCLion(Cursor &cursor, int rows, const Rect &rect, const size_t nItems) {

    if (rows == 1) {
        return OnNavigateUpVSCode(cursor, rows, rect, nItems);
    }


    int nRowsToMove = rows;
    bool forceCursorToFirstLine = false;

    if ((viewTopLine - nRowsToMove) < 0) {
        forceCursorToFirstLine = true;
        nRowsToMove = 0;
    }


//    logger->Debug("OnNavUpCLion");
//    logger->Debug("  nRowsToMove=%d, forceCursor=%s, nLines=%d, maxRows=%d",
//                  nRowsToMove, forceCursorToFirstLine?"Y":"N", (int)editorModel->Lines().size(), maxRows);
//    logger->Debug("  Before, topLine=%d, bottomLine=%d, activeLine=%d, cursor.y=%d", editorModel->viewTopLine, editorModel->viewBottomLine, editorModel->idxActiveLine, editorModel->cursor.position.y);


    // Reposition the view
    viewTopLine -= nRowsToMove;
    viewBottomLine -= nRowsToMove;

    // In case we would have moved beyond the visible part, let's enforce the cursor position..
    if (forceCursorToFirstLine) {
        cursor.position.y = 0;
        idxActiveLine = 0;
        viewTopLine = 0;
        viewBottomLine = rect.Height();
        //logger->Debug("       force to first!");
    } else {
        idxActiveLine -= nRowsToMove;
        cursor.position.y = idxActiveLine - viewTopLine;
        if (cursor.position.y < 0) {
            cursor.position.y = 0;
        }
    }
    //logger->Debug("  After, topLine=%d, bottomLine=%d, activeLine=%d, cursor.y=%d", editorModel->viewTopLine, editorModel->viewBottomLine, editorModel->idxActiveLine, editorModel->cursor.position.y);
}
