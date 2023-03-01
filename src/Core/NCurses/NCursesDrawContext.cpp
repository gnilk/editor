//
// Created by gnilk on 22.02.23.
//

#include <ncurses.h>
#include "NCursesDrawContext.h"

using namespace gedit;

void NCursesDrawContext::Clear() {
    wclear((WINDOW *)win);
}

void NCursesDrawContext::Scroll(int nRows) {
    wscrl((WINDOW *)win, nRows);
}

void NCursesDrawContext::DrawStringAt(int x, int y, const char *str) {
    wmove((WINDOW *)win, y, x);
    waddstr((WINDOW *)win,str);
}
