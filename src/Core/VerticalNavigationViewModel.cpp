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
#include <assert.h>
using namespace gedit;


void VerticalNavigationViewModel::HandleResize(const Rect &viewRect) {
    assert(lineCursor != nullptr);

    auto logger = gnilk::Logger::GetLogger("VNavModel");

    // Clip source (lines) to view if view is smaller
    if (lineCursor->Height() > viewRect.Height()) {
        logger->Debug("Clip visual view smaller than line-view");
        logger->Debug("Current visual height: %d", viewRect.Height());
        logger->Debug("Current Line-View, top=%zu bottom=%zu", lineCursor->viewTopLine, lineCursor->viewBottomLine);

        auto delta = lineCursor->Height() - viewRect.Height();
        logger->Debug("Diff: %zu",delta);

        if (delta > lineCursor->viewTopLine) {
            logger->Debug("Already at top...");
            lineCursor->viewTopLine = 0;
            lineCursor->viewBottomLine = viewRect.Height();
        }  else {
            lineCursor->viewBottomLine -= delta;
        }
        // did the active line go outside the visible spectrum?
        if (lineCursor->idxActiveLine > lineCursor->viewBottomLine) {
            logger->Debug("Active Line outside visible scope, readjusting visible scope");

            int activeDelta = lineCursor->Height() / 2;
            lineCursor->viewTopLine = lineCursor->idxActiveLine - activeDelta;
            lineCursor->viewBottomLine = lineCursor->viewTopLine + viewRect.Height();
            logger->Debug("New Line-View, top=%zu bottom=%zu", lineCursor->viewTopLine, lineCursor->viewBottomLine);
        }
    } else if (lineCursor->Height() < viewRect.Height()) {
        auto delta = viewRect.Height() - lineCursor->Height();
        lineCursor->viewBottomLine += delta;
    }

    // Now do cursor..
    if (lineCursor->idxActiveLine < lineCursor->viewTopLine) {
        logger->Error("idxActiveLine < viewTopLine!!!!!");
    }
    lineCursor->cursor.position.y = lineCursor->idxActiveLine - lineCursor->viewTopLine;
}


// This implements VSCode style of downwards navigation
// Cursor if moved first then content (i.e if standing on first-line, the cursor is moved to the bottom line on first press)
void VerticalNavigationViewModel::OnNavigateDownVSCode(size_t rows, const Rect &rect, size_t nItems) {
    assert(lineCursor != nullptr);

    auto logger = gnilk::Logger::GetLogger("VNavModel");

    logger->Debug("OnNavDownVSCode,  before, active list = %d", lineCursor->idxActiveLine);

    lineCursor->idxActiveLine+=rows;
    if (lineCursor->idxActiveLine > nItems-1) {
        lineCursor->idxActiveLine = nItems-1;
    }

    if (lineCursor->idxActiveLine > static_cast<size_t>(rect.Height()-1)) {
        if (!(lineCursor->cursor.position.y < rect.Height()-1)) {
            if ((lineCursor->viewBottomLine + rows) < nItems) {
                logger->Debug("Clipping top/bottom lines");
                lineCursor->viewTopLine += rows;
                lineCursor->viewBottomLine += rows;
            } else {
                lineCursor->viewBottomLine = nItems;
                lineCursor->viewTopLine = lineCursor->viewBottomLine - rect.Height();
            }
        }
    }

    lineCursor->cursor.position.y = lineCursor->idxActiveLine - lineCursor->viewTopLine;
    if (lineCursor->cursor.position.y > rect.Height()-1) {
        lineCursor->cursor.position.y = rect.Height()-1;
    }
    logger->Debug("OnNavDownVSCode, rows=%d, new active line=%d", rows, lineCursor->idxActiveLine);
    logger->Debug("                 viewTopLine=%d, viewBottomLine=%d", lineCursor->viewTopLine, lineCursor->viewBottomLine);
    logger->Debug("                 cursor.pos.y=%d", lineCursor->cursor.position.y);

//    logger->Debug("OnNavDownVSCode, activeLine=%d, rows=%d, ypos=%d, height=%d", idxActiveLine, rows, cursor.position.y, rect.Height());
}

void VerticalNavigationViewModel::OnNavigateUpVSCode(size_t rows, const Rect &rect, size_t nItems) {
    assert(lineCursor != nullptr);

    if (lineCursor->idxActiveLine < rows) {
        lineCursor->idxActiveLine = 0;
    } else {
        lineCursor->idxActiveLine -= rows;
    }

    lineCursor->cursor.position.y -= rows;
    if (lineCursor->cursor.position.y < 0) {
        int delta = 0 - lineCursor->cursor.position.y;
        lineCursor->cursor.position.y = 0;
        if (delta <= lineCursor->viewTopLine) {
            lineCursor->viewTopLine -= delta;
            lineCursor->viewBottomLine -= delta;
        }
        if (lineCursor->viewTopLine < 0) {
            lineCursor->viewTopLine = 0;
            lineCursor->viewBottomLine = rect.Height();
        }
    }
}

// CLion/Sublime style of navigation on pageup/down - this first moves the content and then adjust cursor
// This moves content first and cursor rather stays
void VerticalNavigationViewModel::OnNavigateDownCLion(size_t rows, const Rect &rect, size_t nItems) {
    assert(lineCursor != nullptr);

    if (rows == 1) {
        return OnNavigateDownVSCode(rows, rect, nItems);
    }

    bool forceCursorToLastLine = false;
    auto nRowsToMove = rows;
    auto maxRows = nItems - 1;

    auto logger = gnilk::Logger::GetLogger("VNavModel");
    logger->Debug("OnNavDownCLion");

    // Note: We might want to revisit this clipping, if we want a margin to the bottom...
    // Maybe we should adjust for the visible margin at the end
    if ((lineCursor->viewTopLine+nRowsToMove) > maxRows) {
        //logger->Debug("  Move beyond last line!");
        nRowsToMove = 0;
        forceCursorToLastLine = true;
    }
    if (!forceCursorToLastLine && ((lineCursor->viewBottomLine+nRowsToMove) > maxRows)) {

        //
        // FIXME: This clipping doesn't make sense!!!
        //


        int nTooMuch = (lineCursor->viewBottomLine+nRowsToMove) - maxRows;
        nRowsToMove = (lineCursor->viewBottomLine+nRowsToMove) - maxRows;
//        //logger->Debug("  Clip nRowsToMove");
//        if (nItems > lineCursor->viewBottomLine) {
//            nRowsToMove = nItems - lineCursor->viewBottomLine;
//        } else {
//            nRowsToMove = nItems;
//        }
        // If this results to zero, we are exactly at the bottom...
        if (!nRowsToMove) {
            forceCursorToLastLine = true;
        }
    }
    logger->Debug("  nRowsToMove=%d, forceCursor=%s, nLines=%d, maxRows=%d",
                  nRowsToMove, forceCursorToLastLine?"Y":"N", (int)nItems, maxRows);
    logger->Debug("  Before, topLine=%d, bottomLine=%d, activeLine=%d, cursor.y=%d",
                  lineCursor->viewTopLine, lineCursor->viewBottomLine, lineCursor->idxActiveLine,
                  lineCursor->cursor.position.y);


    // Reposition the view
    int activeLineDelta = lineCursor->idxActiveLine - lineCursor->viewTopLine;
    lineCursor->viewTopLine += nRowsToMove;
    lineCursor->viewBottomLine += nRowsToMove;

    // In case we would have moved beyond the visible part, let's enforce the cursor position..
    if (forceCursorToLastLine) {
        lineCursor->cursor.position.y = nItems - lineCursor->viewTopLine - 1;
        if (lineCursor->cursor.position.y < 0) {
            lineCursor->cursor.position.y = 0;
        }
        lineCursor->idxActiveLine = nItems-1;
        //logger->Debug("       force to last!");
    } else {
        lineCursor->idxActiveLine = lineCursor->viewTopLine + activeLineDelta;
        lineCursor->cursor.position.y = lineCursor->idxActiveLine - lineCursor->viewTopLine;
        if (lineCursor->cursor.position.y > rect.Height() - 1) {
            lineCursor->cursor.position.y = rect.Height() - 1;
        }
    }
    logger->Debug("  After, topLine=%d, bottomLine=%d, activeLine=%d, cursor.y=%d",
                  lineCursor->viewTopLine, lineCursor->viewBottomLine, lineCursor->idxActiveLine,
                  lineCursor->cursor.position.y);
}

void VerticalNavigationViewModel::OnNavigateUpCLion(size_t rows, const Rect &rect, size_t nItems) {
    assert(lineCursor != nullptr);
    auto logger = gnilk::Logger::GetLogger("VNavModel");

    if (rows == 1) {
        return OnNavigateUpVSCode(rows, rect, nItems);
    }


    int nRowsToMove = rows;
    bool forceCursorToFirstLine = false;

    if (nRowsToMove > lineCursor->viewTopLine) {
        forceCursorToFirstLine = true;
        nRowsToMove = 0;
    }


    logger->Debug("OnNavUpCLion");
    logger->Debug("  nRowsToMove=%d, forceCursor=%s, nLines=%zu, maxRows=%zu",
                  nRowsToMove, forceCursorToFirstLine?"Y":"N", nItems, rows);
    logger->Debug("  Before, topLine=%zu, bottomLine=%zu, activeLine=%zu, cursor.y=%d",
                  lineCursor->viewTopLine, lineCursor->viewBottomLine, lineCursor->idxActiveLine,
                  lineCursor->cursor.position.y);


    // Can't move beyond topline...  size_t would mean underflow -> very high numbers

    // Reposition the view
    lineCursor->viewTopLine -= nRowsToMove;
    lineCursor->viewBottomLine -= nRowsToMove;

    // In case we would have moved beyond the visible part, let's enforce the cursor position..
    if (forceCursorToFirstLine) {
        lineCursor->cursor.position.y = 0;
        lineCursor->idxActiveLine = 0;
        lineCursor->viewTopLine = 0;
        lineCursor->viewBottomLine = rect.Height();
        //logger->Debug("       force to first!");
    } else {
        lineCursor->idxActiveLine -= nRowsToMove;
        lineCursor->cursor.position.y = lineCursor->idxActiveLine - lineCursor->viewTopLine;
        if (lineCursor->cursor.position.y < 0) {
            lineCursor->cursor.position.y = 0;
        }
    }
    logger->Debug("  After, topLine=%zu, bottomLine=%zu, activeLine=%zu, cursor.y=%d",
                  lineCursor->viewTopLine, lineCursor->viewBottomLine, lineCursor->idxActiveLine,
                  lineCursor->cursor.position.y);
}


