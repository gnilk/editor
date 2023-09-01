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


void VerticalNavigationViewModel::HandleResize(Cursor &cursor, const Rect &viewRect) {
    auto logger = gnilk::Logger::GetLogger("VNavModel");

    // Clip source (lines) to view if view is smaller
    if ((viewBottomLine - viewTopLine) > viewRect.Height()) {
        logger->Debug("Clip visual view smaller than line-view");
        logger->Debug("Current visual height: %d", viewRect.Height());
        logger->Debug("Current Line-View, top=%zu bottom=%zu", viewTopLine, viewBottomLine);

        auto delta = (viewBottomLine - viewTopLine) - viewRect.Height();
        logger->Debug("Diff: %zu",delta);

        if (delta > viewTopLine) {
            logger->Debug("Already at top...");
            viewTopLine = 0;
            viewBottomLine = viewRect.Height();
        }  else {
            viewBottomLine -= delta;
        }
        // did the active line go outside the visible spectrum?
        if (idxActiveLine > viewBottomLine) {
            logger->Debug("Active Line outside visible scope, readjusting visible scope");

            int activeDelta = (viewBottomLine - viewTopLine) / 2;
            viewTopLine = idxActiveLine - activeDelta;
            viewBottomLine = viewTopLine + viewRect.Height();
            logger->Debug("New Line-View, top=%zu bottom=%zu", viewTopLine, viewBottomLine);
        }
    } else if ((viewBottomLine - viewTopLine) < viewRect.Height()) {
        auto delta = viewRect.Height() - (viewBottomLine - viewTopLine);
        viewBottomLine += delta;
    }

    // Now do cursor..
    if (idxActiveLine < viewTopLine) {
        logger->Error("idxActiveLine < viewTopLine!!!!!");
    }
    cursor.position.y = idxActiveLine - viewTopLine;
}


// This implements VSCode style of downwards navigation
// Cursor if moved first then content (i.e if standing on first-line, the cursor is moved to the bottom line on first press)
void VerticalNavigationViewModel::OnNavigateDownVSCode(Cursor &cursor, size_t rows, const Rect &rect, size_t nItems) {

    auto logger = gnilk::Logger::GetLogger("VNavModel");

    logger->Debug("OnNavDownVSCode,  before, active list = %d", idxActiveLine);

    idxActiveLine+=rows;
    if (idxActiveLine > nItems-1) {
        idxActiveLine = nItems-1;
    }

    if (idxActiveLine > static_cast<size_t>(rect.Height()-1)) {
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

void VerticalNavigationViewModel::OnNavigateUpVSCode(Cursor &cursor, size_t rows, const Rect &rect, size_t nItems) {
    if (idxActiveLine < rows) {
        idxActiveLine = 0;
    } else {
        idxActiveLine -= rows;
    }

    cursor.position.y -= rows;
    if (cursor.position.y < 0) {
        int delta = 0 - cursor.position.y;
        cursor.position.y = 0;
        if (delta <= viewTopLine) {
            viewTopLine -= delta;
            viewBottomLine -= delta;
        }
        if (viewTopLine < 0) {
            viewTopLine = 0;
            viewBottomLine = rect.Height();
        }
    }
}

// CLion/Sublime style of navigation on pageup/down - this first moves the content and then adjust cursor
// This moves content first and cursor rather stays
void VerticalNavigationViewModel::OnNavigateDownCLion(Cursor &cursor, size_t rows, const Rect &rect, size_t nItems) {

    if (rows == 1) {
        return OnNavigateDownVSCode(cursor, rows, rect, nItems);
    }

    bool forceCursorToLastLine = false;
    auto nRowsToMove = rows;
    auto maxRows = nItems - 1;

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

void VerticalNavigationViewModel::OnNavigateUpCLion(Cursor &cursor, size_t rows, const Rect &rect, size_t nItems) {

    auto logger = gnilk::Logger::GetLogger("VNavModel");

    if (rows == 1) {
        return OnNavigateUpVSCode(cursor, rows, rect, nItems);
    }


    int nRowsToMove = rows;
    bool forceCursorToFirstLine = false;

    if (nRowsToMove > viewTopLine) {
        forceCursorToFirstLine = true;
        nRowsToMove = 0;
    }


    logger->Debug("OnNavUpCLion");
    logger->Debug("  nRowsToMove=%d, forceCursor=%s, nLines=%zu, maxRows=%zu",
                  nRowsToMove, forceCursorToFirstLine?"Y":"N", nItems, rows);
    logger->Debug("  Before, topLine=%zu, bottomLine=%zu, activeLine=%zu, cursor.y=%d", viewTopLine, viewBottomLine, idxActiveLine, cursor.position.y);


    // Can't move beyond topline...  size_t would mean underflow -> very high numbers

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
    logger->Debug("  After, topLine=%zu, bottomLine=%zu, activeLine=%zu, cursor.y=%d", viewTopLine, viewBottomLine, idxActiveLine, cursor.position.y);
}


