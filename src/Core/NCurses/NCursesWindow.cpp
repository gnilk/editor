//
// Created by gnilk on 20.02.23.
//

#include <ncurses.h>
#include "logger.h"
#include "NCursesWindow.h"

using namespace gedit;

void NCursesWindow::Scroll(int rows) {
    auto res = wscrl(window, 1);
    if (res == ERR) {
        auto logger = gnilk::Logger::GetLogger("NCursesWindow");
        logger->Error("wscrl");
    }
}

void NCursesWindow::Refresh() {
    wrefresh(window);
}
