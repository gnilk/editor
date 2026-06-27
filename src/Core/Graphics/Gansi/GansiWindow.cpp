//
// gedit::Gansi window implementation.
//
#include "Core/Graphics/Gansi/GansiWindow.h"
#include "Core/Graphics/Gansi/GansiScreen.h"

using namespace gedit;
using namespace gedit::Gansi;

GansiWindow::GansiWindow(const Rect &rect, gnilk::ansi::Terminal *term, GansiScreen *scr)
    : WindowBase(rect), screen(scr), clientContext(term, rect) {
    drawContext = &clientContext;
}

void GansiWindow::SetCursor(const Cursor &cursor) {
    if (screen == nullptr) {
        return;
    }
    // Window-local caret -> absolute grid cell.
    int absCol = windowRect.TopLeft().x + cursor.position.x;
    int absRow = windowRect.TopLeft().y + cursor.position.y;
    screen->SetActiveCursor(absCol, absRow, true);
}

void GansiWindow::UpdateRect(const Rect &newRect) {
    windowRect = newRect;
    clientContext.SetClientRect(newRect);
}
