//
// Created by gnilk on 14.01.23.
//

#include <ncurses.h>
#include "NCursesScreen.h"

bool NCursesScreen::Open() {
    use_extended_names(TRUE);
    initscr();
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
    clear();
    //raw();
    keypad(stdscr, TRUE);
    noecho();
    cbreak();
    //nonl();
    // Make this configurable...


    ESCDELAY = 1;
    // TODO: Add 'atexit' call which enforces call of 'endwin'
    return true;
}

void NCursesScreen::Close() {
    endwin();
}

void NCursesScreen::Clear() {
    InvalidateAll();
    clear();
    szGutter = 0;
}

void NCursesScreen::Update() {
    refresh();
}


std::pair<int, int> NCursesScreen::ComputeView(int idxActiveLine) {
    auto [rows, cols] = Dimensions();

    int topLine = lastTopLine;
    int bottomLine = topLine + (rows - 2);  // -2, size of status bar => FIX THIS!

    bool bInside = true;
    int dy = 0;

    if ((idxActiveLine >= topLine) && (idxActiveLine < bottomLine)) {
        // We are inside the active area...
        bInside = true;
    } else {
        if (idxActiveLine >= bottomLine) {
            // Scrolling down
            dy = idxActiveLine - bottomLine;
            topLine += dy;
            bottomLine += dy;
        } else if (idxActiveLine < topLine) {
            // Scrolling up..
            dy = idxActiveLine - topLine;
            topLine += dy;
            bottomLine += dy;
        }

        bInside = false;
        invalidateAll = true;
    }

    tmp_dyLast = dy;

    view.first = topLine;
    view.second = bottomLine;

    return view;
}

//
// Should probably have multiple draw-lines
// We need a 'base-core' version - which just outputs the dat
// We need one for the editor which has a 'gutter' and a few other things - as we need to offset the cursor properly
// Also need to indicate which line is the current line..
//
void NCursesScreen::DrawGutter(int idxStart) {
    auto [rows, cols] = Dimensions();

    auto [top, bottom] = ComputeView(idxStart);

    for(int i=0;i<rows-1;i++) {
        mvprintw(i, 0, "%3d|",i + top);
    }
    szGutter = 4;
}

void NCursesScreen::DrawLines(const std::vector<Line *> &lines, int idxActiveLine) {
    auto [rows, cols] = Dimensions();
    int idxRowActive = 0;

    auto [topLine, bottomLine] = ComputeView(idxActiveLine);

    for(int i=0;i<rows-1;i++) {
        int idxLine = topLine + i;
        if (idxLine >= lines.size()) break;

        auto line = lines[idxLine];
        if (line->IsActive()) {
            idxRowActive = i;
        }
        if (invalidateAll || line->IsActive()) {
            // Clip
            int nCharToPrint = line->Length()>(cols-szGutter)?(cols-szGutter):line->Length();
            move(i, szGutter);
            clrtoeol();
            if (line->IsSelected()) {
                attron(A_REVERSE);
            }
            mvaddnstr(i, szGutter, line->Buffer().data(), nCharToPrint);
            if (line->IsSelected()) {
                attroff(A_REVERSE);
            }
        }
    }

    move(rows-1,0);
    clrtoeol();
    mvprintw(rows-1, 0, "al: %d (%d) - tl: %d - bl: %d - dy: %d - iva: %s - r: %d", idxActiveLine,  idxRowActive, topLine, bottomLine, tmp_dyLast, invalidateAll?"y":"n", rows);

    lastTopLine = topLine;

    // Always switch off...
    invalidateAll = false;
    move(idxRowActive, szGutter + cursorColumn);
}
void NCursesScreen::DrawStatusBar(const char *str) {
    auto [rows, cols] = Dimensions();
    int y,x;
    // Save the cursor position (as we move it when we print the status bar)
    getyx(stdscr, y, x);

    attron(A_REVERSE);
    mvprintw(rows-1,0, "%s [%d:%d]", str, y, x - szGutter);
    attroff(A_REVERSE);
    // restore cursor to it's position before the status bar was drawn..
    move(y,x);
}


std::pair<int, int> NCursesScreen::Dimensions() {
    int row, col;
    getmaxyx(stdscr,row,col);
    return std::make_pair(row, col);
}

