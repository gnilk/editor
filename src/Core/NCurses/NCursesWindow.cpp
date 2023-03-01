//
// Created by gnilk on 22.02.23.
//

#include <ncurses.h>
#include "logger.h"
#include "NCursesWindow.h"
#include "NCursesDrawContext.h"

using namespace gedit;
void NCursesWindow::Initialize(WindowBase::kWinFlags flags, WindowBase::kWinDecoration newDecoFlags) {
    WindowBase::Initialize(flags, newDecoFlags);
    // Create underlying UI window unless we are invisible...
    if (flags & WindowBase::kWin_Visible) {
        CreateNCursesWindows();
    }
}

void NCursesWindow::CreateNCursesWindows() {
    if (winptr != nullptr) {
        delwin((WINDOW *)winptr);
    }

    auto nativeWin = newwin(windowRect.Height(), windowRect.Width(), windowRect.TopLeft().y, windowRect.TopLeft().x);
    werase(nativeWin);
    scrollok(nativeWin, TRUE);
    wtimeout(nativeWin, 1);
    winptr = nativeWin;
    drawContext = new NCursesDrawContext(winptr, windowRect);


    if (clientWindow != nullptr) {
        delwin(clientWindow);
    }

    auto clientRect = windowRect;
    if (decorationFlags & kWinDeco_Border) {
        clientRect.Deflate(1,1);
    }
    clientWindow = newwin(clientRect.Height(), clientRect.Width(), clientRect.TopLeft().y, clientRect.TopLeft().x);
    scrollok(clientWindow, TRUE);
    wtimeout(clientWindow, 1);
    clientContext = new NCursesDrawContext(clientWindow, clientRect);
}


void NCursesWindow::DrawWindowDecoration() {
    if (flags & WindowBase::kWin_Invisible) {
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

    wnoutrefresh((WINDOW *)winptr);

}

DrawContext &NCursesWindow::GetContentDC() {
    return *clientContext;
}
