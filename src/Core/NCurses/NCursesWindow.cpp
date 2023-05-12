//
// Created by gnilk on 22.02.23.
//

#include <ncurses.h>
#include "logger.h"
#include "NCursesWindow.h"
#include "NCursesDrawContext.h"
#include "NCursesTranslate.h"
#include "NCursesCursor.h"
using namespace gedit;

char glbFillchar = 'a';
bool glbDebugNCWindow = false;

NCursesWindow::NCursesWindow(const Rect &rect, NCursesWindow *other) : WindowBase(rect) {
    caption = other->caption;
    flags = other->flags;
    decorationFlags = other->decorationFlags;
}

NCursesWindow::~NCursesWindow() noexcept {
    if (winptr != nullptr) {
        delwin((WINDOW *)winptr);
    }
    if (clientWindow != nullptr) {
        delwin(clientWindow);
    }
    if (drawContext != nullptr) {
        delete drawContext;
    }
}
void NCursesWindow::Initialize(WindowBase::kWinFlags flags, WindowBase::kWinDecoration newDecoFlags) {
    WindowBase::Initialize(flags, newDecoFlags);
    // Create underlying UI window unless we are invisible...
    if (flags & WindowBase::kWin_Visible) {
        CreateNCursesWindows();
        tmp_fillChar = glbFillchar;
        glbFillchar += 1;
        if (glbFillchar > 'z') {
            glbFillchar = 'a';
        }
    }
}

void NCursesWindow::CreateNCursesWindows() {
    if (winptr != nullptr) {
        delwin((WINDOW *)winptr);
    }

    winptr = nullptr;
    if (drawContext != nullptr) {
        delete drawContext;
    }
    drawContext = new NCursesDrawContext(winptr, windowRect);

    if (clientWindow != nullptr) {
        delwin(clientWindow);
    }

    auto clientRect = windowRect;
    if (decorationFlags & kWinDeco_Border) {
        clientRect.Deflate(1,1);
    }

    if (clientContext != nullptr) {
        delete clientContext;
    }
    clientContext = new NCursesDrawContext(stdscr, clientRect);

    return;

    // Old code below here...
/*
    // Can't do it like this - will cause overlapping windows and screw up redrawing...
    auto nativeWin = newwin(windowRect.Height(), windowRect.Width(), windowRect.TopLeft().y, windowRect.TopLeft().x);
    werase(nativeWin);
    scrollok(nativeWin, TRUE);
    wtimeout(nativeWin, 1);
    leaveok(nativeWin, TRUE);
    winptr = nativeWin;
    if (drawContext != nullptr) {
        delete drawContext;
    }
    drawContext = new NCursesDrawContext(winptr, windowRect);

    if (clientWindow != nullptr) {
        delwin(clientWindow);
    }

    auto clientRect = windowRect;
    if (decorationFlags & kWinDeco_Border) {
        clientRect.Deflate(1,1);
    }
//    clientWindow = subwin(nativeWin, clientRect.Height(), clientRect.Width(), clientRect.TopLeft().y, clientRect.TopLeft().x);
    clientWindow = newwin(clientRect.Height(), clientRect.Width(), clientRect.TopLeft().y, clientRect.TopLeft().x);
    scrollok(clientWindow, TRUE);
    leaveok(clientWindow, TRUE);
    wtimeout(clientWindow, 1);
    if (clientContext != nullptr) {
        delete clientContext;
    }
    clientContext = new NCursesDrawContext(clientWindow, clientRect);
*/
}

DrawContext &NCursesWindow::GetContentDC() {
    // Sometimes we need this for just sub-component calculation - so let's create a dummy context
    if (clientContext == nullptr) {
        clientContext = new NCursesDrawContext(nullptr, windowRect);
    }
    return *clientContext;
}


void NCursesWindow::Clear() {
    exit(1);
    std::string marker;

    if (caption.empty()) {
        marker += tmp_fillChar;
    } else {
        marker = caption;
    }

    auto logger = gnilk::Logger::GetLogger("NCursesWindow");
    logger->Debug("Clear - %s, p:(%d:%d) d:(%d:%d)", marker.c_str(),
                  windowRect.TopLeft().x, windowRect.TopLeft().y,
                  windowRect.Width(), windowRect.Height());


    if (caption == "SingleLineView") {
        int breakme = 1;
    }

    int ec=0;
//    for(int y=windowRect.TopLeft().y;y<windowRect.BottomRight().y;y++) {
    for(int y=0;y<windowRect.Height();y++) {
        int xp = 0;
        while(xp < windowRect.Width()) {
            if ((xp + marker.size()) > windowRect.Width()) {
                break;
            }
            auto res = mvwprintw((WINDOW *)winptr, y,xp,"%s",marker.c_str());
            if (res < 0) {
                ec++;
            }
            xp += marker.size();
        }
    }
    if (ec) {
        logger->Error("  mvprintfw, errors=%d", ec);
    }
    touchwin((WINDOW *)winptr);
}


void NCursesWindow::DrawWindowDecoration() {
    if ((flags & WindowBase::kWin_Invisible) && (!glbDebugNCWindow)) {
        return;
    }

    // Erase the whole window
    werase((WINDOW *)winptr);

    if (decorationFlags & kWinDeco_TopBorder) {
        wmove((WINDOW *) winptr, 0, 0);
        wvline((WINDOW *) winptr, ACS_VLINE, windowRect.Height());
    }

    if (decorationFlags & kWinDeco_BottomBorder) {
        wmove((WINDOW *) winptr, 0, windowRect.Width() - 1);
        wvline((WINDOW *) winptr, ACS_VLINE, windowRect.Height());
    }

    if (decorationFlags & kWinDeco_LeftBorder) {
        wmove((WINDOW *) winptr, 0, 0);
        //whline((WINDOW *) winptr, ACS_HLINE, windowRect.Width());
        whline((WINDOW *) winptr, '-', windowRect.Width());
    }

    if (decorationFlags & kWinDeco_RightBorder) {
        wmove((WINDOW *) winptr, windowRect.Height() - 1, 0);
        //whline((WINDOW *) winptr, ACS_HLINE, windowRect.Width());
        whline((WINDOW *) winptr, '-', windowRect.Width());
    }

    if (decorationFlags & kWinDeco_DrawCaption) {
        wmove((WINDOW *)winptr, 0, 2);
        waddstr((WINDOW *)winptr, caption.c_str());
    }

//    wnoutrefresh((WINDOW *)winptr);

}

void NCursesWindow::SetCursor(const Cursor &cursor) {

    NCursesCursor::Instance().SetCursor(cursor, [this](const Cursor &c) -> void {
       OnDrawCursor(c);
    });

}
void NCursesWindow::OnDrawCursor(const Cursor &cursor) {
    if (clientContext == nullptr) {
        return;
    }

    auto dc = static_cast<NCursesDrawContext *>(clientContext);
    dc->DrawCursor(cursor);
}
