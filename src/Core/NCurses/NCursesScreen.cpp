//
// Created by gnilk on 14.01.23.
//
// - Cursor is global, put cursor functions here...
//
//

#include <ncurses.h>
#include "NCursesWindow.h"
#include "NCursesScreen.h"
#include <signal.h>
#include "logger.h"
using namespace gedit;

static void handle_winch(int sig)
{
    auto logger = gnilk::Logger::GetLogger("winch");
    logger->Debug("SIG WINCH!");
    endwin();
    // Needs to be called after an endwin() so ncurses will initialize
    // itself with the new terminal dimensions.
    refresh();
    //clear();
}
static void install_sigwinch() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = handle_winch;
    sigaction(SIGWINCH, &sa, NULL);
}

bool NCursesScreen::Open() {
    use_extended_names(TRUE);
    initscr();
    //install_sigwinch();
    if (has_colors()) {
        // Just test it a bit...
        start_color();
        init_color(COLOR_GREEN, 200,1000,200);
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
        init_pair(2, COLOR_BLACK, COLOR_GREEN);
        attron(COLOR_PAIR(1));
    } else {
        printf("No colors, going with defaults...\n");
    }
    timeout(1); // Make 'getch' non-blocking..
    clear();
    //raw();
    keypad(stdscr, TRUE);
    scrollok(stdscr, TRUE);
    noecho();
    cbreak();
    //nonl();
    // Make this configurable...

    int row, col;
    getmaxyx(stdscr,row,col);


    ESCDELAY = 1;

    return true;
}

void NCursesScreen::Close() {
    endwin();
}

void NCursesScreen::Clear() {
    clear();
}

void NCursesScreen::Update() {
    doupdate();
}
void NCursesScreen::BeginRefreshCycle() {
}
void NCursesScreen::EndRefreshCycle() {
}

Rect NCursesScreen::Dimensions() {
    int row, col;
    getmaxyx(stdscr,row,col);
    return Rect(col, row);
}

WindowBase *NCursesScreen::CreateWindow(const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) {
    auto window = new NCursesWindow(rect);
    window->Initialize(flags, decoFlags);
    return window;
}
