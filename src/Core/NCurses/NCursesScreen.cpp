//
// Created by gnilk on 14.01.23.
//
// - Cursor is global, put cursor functions here...
//
//

#include <ncurses.h>
#include <signal.h>
#include <string.h>

#include "logger.h"

#include "NCursesColorRepository.h"
#include "NCursesWindow.h"
#include "NCursesScreen.h"
#include "NCursesCursor.h"

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

    logger = gnilk::Logger::GetLogger("NCursesScreen");

    use_extended_names(TRUE);
    initscr();
    //install_sigwinch();
    if (has_colors()) {
        // Just test it a bit...
        start_color();
//        init_color(COLOR_GREEN, 200,1000,200);
//        init_pair(1, COLOR_GREEN, COLOR_BLACK);
//        init_pair(2, COLOR_BLACK, COLOR_GREEN);
//        attron(COLOR_PAIR(1));
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

    getmaxyx(stdscr,heightChars, widthChars);
    logger->Debug("Resolution: %d x %d chars", widthChars, heightChars);

    ESCDELAY = 1;

    return true;
}

void NCursesScreen::Close() {
    endwin();
}

void NCursesScreen::Clear() {
    // DO NOT CALL CLEAR - it does more than just clearing the screen
    // and I don't have a firm grasp of how the states in NCurses trickels down...
    //clear();
    // hmm... we can cache this one...

    auto bgColor = Config::Instance().GetGlobalColors().GetColor("background");
    auto fgColor = Config::Instance().GetGlobalColors().GetColor("foreground");

    auto colorPair = NCursesColorRepository::Instance().GetColorPairIndex(fgColor, bgColor);

    attrset(A_NORMAL);
    attron(COLOR_PAIR(colorPair));


    std::string clrstr(widthChars, ' ');

    for(int line = 0; line < heightChars; line++) {
        move(line,0);
        addnstr(clrstr.c_str(), widthChars-1);
        insch(' ');
    }


}

void NCursesScreen::Update() {
    // All drawing is complete, let's move the cursor to it's correct position...
    NCursesCursor::Instance().Draw();
    doupdate();
    refresh();
}

static int colorCounter = 0;
void NCursesScreen::RegisterColor(int appIndex, const ColorRGBA &foreground, const ColorRGBA &background) {

    return;
    int currentColor = colorCounter;

    init_color(colorCounter++, background.R() * 1000, background.G() * 1000, background.B() * 1000);
    init_color(colorCounter++, foreground.R() * 1000, foreground.G() * 1000, foreground.B() * 1000);

    init_pair(appIndex,  currentColor + 1, currentColor);
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

WindowBase *NCursesScreen::UpdateWindow(WindowBase *window, const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) {
    auto newWindow = new NCursesWindow(rect, static_cast<NCursesWindow *>(window));
    newWindow->Initialize(flags, decoFlags);

    delete window;

    return newWindow;

}
