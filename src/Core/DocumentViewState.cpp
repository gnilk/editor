//
// Created by gnilk on 15.06.26.
//
// Session (de)serialisation for the per-view editing state. Kept out of the header only to avoid
// recompiling the conversion bodies in every translation unit that includes DocumentViewState.h.
//

#include "Core/DocumentViewState.h"

using namespace gedit;

DocumentSession DocumentViewState::ToSession() const {
    DocumentSession session;
    // path is filled by Document (the owner of file identity), not here.
    session.cursorX = lineCursor.cursor.position.x;
    session.cursorY = lineCursor.cursor.position.y;
    session.wantedColumn = lineCursor.cursor.wantedColumn;
    session.idxActiveLine = lineCursor.idxActiveLine;
    session.viewTopLine = lineCursor.viewTopLine;
    session.viewBottomLine = lineCursor.viewBottomLine;

    // Raw ends so a backward selection (start after end) survives the round-trip un-normalised.
    const auto &selStart = currentSelection.GetRawStart();
    const auto &selEnd = currentSelection.GetRawEnd();
    session.selection.isActive = currentSelection.IsActive();
    session.selection.startX = selStart.x;
    session.selection.startY = selStart.y;
    session.selection.endX = selEnd.x;
    session.selection.endY = selEnd.y;

    session.viewMode = viewMode;
    return session;
}

void DocumentViewState::FromSession(const DocumentSession &session) {
    lineCursor.cursor.position = Point(session.cursorX, session.cursorY);
    lineCursor.cursor.wantedColumn = session.wantedColumn;
    lineCursor.idxActiveLine = session.idxActiveLine;
    lineCursor.viewTopLine = session.viewTopLine;
    lineCursor.viewBottomLine = session.viewBottomLine;

    currentSelection.SetActive(session.selection.isActive);
    currentSelection.SetStart(Point(session.selection.startX, session.selection.startY));
    currentSelection.SetEnd(Point(session.selection.endX, session.selection.endY));

    viewMode = session.viewMode;
}

std::vector<WrapRow> DocumentViewState::ProjectVisibleRows(const std::vector<Line::Ref> &lines, int tabSize) const {
    int nRows = lineCursor.viewBottomLine - lineCursor.viewTopLine;
    size_t idxTopLine = (lineCursor.viewTopLine < 0) ? 0 : static_cast<size_t>(lineCursor.viewTopLine);
    return WrapProjection::ProjectRows(lines, idxTopLine, nRows, wrapWidth, tabSize, wrapMode);
}

void DocumentViewState::ScrollActiveRowIntoView(const std::vector<Line::Ref> &lines, int tabSize) {
    int nRows = lineCursor.viewBottomLine - lineCursor.viewTopLine;
    size_t idxActiveLine = lineCursor.idxActiveLine;
    if ((wrapMode == WrapMode::kNone) || (nRows <= 0) || (idxActiveLine >= lines.size())) {
        return;
    }
    size_t idxTopLine = (lineCursor.viewTopLine < 0) ? 0 : static_cast<size_t>(lineCursor.viewTopLine);
    if (idxActiveLine < idxTopLine) {
        idxTopLine = idxActiveLine;
    }

    // Rows from the top of the view down to and including the caret's row. A caret on a segment boundary sits on
    // the NEXT row (same rule as WrapProjection::TextToScreen).
    std::vector<int> rowsPerLine;
    int rowsNeeded = 0;
    for (size_t idxLine = idxTopLine; idxLine < idxActiveLine; ++idxLine) {
        int nLineRows = static_cast<int>(WrapProjection::WrapLine(*lines[idxLine], wrapWidth, tabSize, wrapMode).size());
        rowsPerLine.push_back(nLineRows);
        rowsNeeded += nLineRows;
    }
    auto activeSegments = WrapProjection::WrapLine(*lines[idxActiveLine], wrapWidth, tabSize, wrapMode);
    size_t charIdx = (lineCursor.cursor.position.x < 0) ? 0 : static_cast<size_t>(lineCursor.cursor.position.x);
    size_t idxCaretSegment = 0;
    while (((idxCaretSegment + 1) < activeSegments.size()) && (charIdx >= activeSegments[idxCaretSegment].charEnd)) {
        ++idxCaretSegment;
    }
    rowsNeeded += static_cast<int>(idxCaretSegment) + 1;

    // Drop whole lines off the top until the caret row fits.
    size_t idxDropped = 0;
    while ((rowsNeeded > nRows) && (idxTopLine < idxActiveLine)) {
        rowsNeeded -= rowsPerLine[idxDropped];
        ++idxDropped;
        ++idxTopLine;
    }

    lineCursor.viewTopLine = static_cast<int32_t>(idxTopLine);
    lineCursor.viewBottomLine = lineCursor.viewTopLine + nRows;
    lineCursor.cursor.position.y = static_cast<int>(idxActiveLine - idxTopLine);
}
