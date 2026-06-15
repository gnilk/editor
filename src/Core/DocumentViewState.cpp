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
