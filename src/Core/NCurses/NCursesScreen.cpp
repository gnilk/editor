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
    clipRect = gedit::Rect(col, row);


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

static int colorCounter = 0;
void NCursesScreen::RegisterColor(int appIndex, const ColorRGBA &foreground, const ColorRGBA &background) {

    int currentColor = colorCounter;

    init_color(colorCounter++, background.R() * 1000, background.G() * 1000, background.B() * 1000);
    init_color(colorCounter++, foreground.R() * 1000, foreground.G() * 1000, foreground.B() * 1000);

    init_pair(appIndex,  currentColor + 1, currentColor);
}

std::pair<int, int> NCursesScreen::ComputeView(int idxActiveLine) {
    //auto [rows, cols] = Dimensions();
    auto dimensions = Dimensions();

    int topLine = lastTopLine;
    int bottomLine = topLine + (dimensions.Height() - 3);  // -2, size of status bar => FIX THIS!

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
//
// There are several ways of doing, one is to prepare the gutter string and then let 'DrawLine' output it..
// Benefit is we can have all line-attribute logic in one place.
// Perhaps a marker if the line exceeds the width..
//
// Should probably have multiple draw-lines
// We need a 'base-core' version - which just outputs the dat
// We need one for the editor which has a 'gutter' and a few other things - as we need to offset the cursor properly
// Also need to indicate which line is the current line..
//
void NCursesScreen::DrawGutter(int idxStart) {
    //auto [rows, cols] = Dimensions();
    auto dimensions = Dimensions();

    auto [top, bottom] = ComputeView(idxStart);

    // FIXME: deduct gutter from idxStart...
    for(int i=1;i<dimensions.Height()-1;i++) {
        mvprintw(i, 0, "%4d|",i-1 + top);
    }
    szGutter = 5;
}

void NCursesScreen::DrawLineAt(int row, const std::string &prefix, const Line *line) {
    //auto [rows, cols] = Dimensions();
    auto dimensions = Dimensions();

    move(row, szGutter);
    clrtoeol();
    int nCharToPrint = (line->Length() + prefix.length())>(dimensions.Width()-szGutter)?(dimensions.Width()-szGutter):line->Length();
    move(row, szGutter);
    addstr(prefix.c_str());
    addnstr(line->Buffer().data(), nCharToPrint);
//    mvaddnstr(row, szGutter, line->Buffer().data(), nCharToPrint);
}

// TODO: Properly handle tab char (i.e. move cursor properly)
void NCursesScreen::DrawLineWithAttributes(Line &l, int nCharToPrint) {


    auto &attribs = l.Attributes();

    int idxColorPair = 0;
    // If no attribs - just dump it out...
    if (attribs.size() == 0) {
        for (int i = 0; i < l.Length(); i++) {
            addch(l.Buffer().at(i));
        }
        return;
    }

    int idxAttrib = 0;
    attr_t attrib;
    auto itAttrib = attribs.begin();
    //int cNext = attribs[0].cStart;
    for (int i = 0; i < nCharToPrint; i++) {
        if (i >= itAttrib->idxOrigString) {
            // FIXME: Convert - must be done in driver...
            attrib = COLOR_PAIR(itAttrib->idxColor);
            itAttrib++;
            if (itAttrib == attribs.end()) {
                --itAttrib;
            }
        }
        attrset(attrib);
        // Note: Improve selection visualization - allow a specific selection background
        //       set the selection background for all foregrounds add to a new pair...
        //       THIS should be done "automatically" by the driver...
        if (l.IsSelected()) {
            attron(A_REVERSE);
        }
        addch(l.Buffer().at(i));
        if (l.IsSelected()) {
            attroff(A_REVERSE);
        }
    }
    attrset(A_NORMAL);
}

void NCursesScreen::DrawLines(const std::vector<Line *> &lines, int idxActiveLine) {
    //auto [rows, cols] = Dimensions();
    auto dimensions = Dimensions();

    int idxRowActive = 0;

    auto [topLine, bottomLine] = ComputeView(idxActiveLine);

    for(int i=1;i<dimensions.Height()-1;i++) {
        int idxLine = topLine + i - 1;
        if (idxLine >= lines.size()) break;

        auto line = lines[idxLine];
        if (line->IsActive()) {
            idxRowActive = i-1;
        }
        if (invalidateAll || line->IsActive()) {
            // Clip
            int nCharToPrint = line->Length()>(dimensions.Width()-szGutter)?(dimensions.Width()-szGutter):line->Length();
            move(i, szGutter);
            clrtoeol();
            // FIXME: selection!
            DrawLineWithAttributes(*line, nCharToPrint);

//            // let's see...
//            attrset(A_NORMAL);
//
//            if (line->IsSelected()) {
//                attron(A_REVERSE);
//            }
//            mvaddnstr(i, szGutter, line->Buffer().data(), nCharToPrint);
//            if (line->IsSelected()) {
//                attroff(A_REVERSE);
//            }
        }
    }

    idxRowActive += 1;  // We have a top-bar

    move(dimensions.Height()-2,0);
    clrtoeol();
    mvprintw(dimensions.Height()-2, 0, "al: %d (%d) - tl: %d - bl: %d - dy: %d - iva: %s - r: %d",
             idxActiveLine,  idxRowActive, topLine, bottomLine, tmp_dyLast, invalidateAll?"y":"n", dimensions.Height());

    lastTopLine = topLine;

    // Always switch off...
    invalidateAll = false;
    move(idxRowActive, szGutter + cursorColumn);
}

void NCursesScreen::DrawTopBar(const char *str) {
    //auto [rows, cols] = Dimensions();
    int y,x;
    // Save the cursor position (as we move it when we print the status bar)
    getyx(stdscr, y, x);

    // test:
    move(0,0);
    attron(A_REVERSE);
    clrtoeol();
    hline(' ', Dimensions().Width());
    mvprintw(0,0,"%s",str);
    attroff(A_REVERSE);
    // end test


    // restore cursor to it's position before the status bar was drawn..
    move(y,x);
}


void NCursesScreen::DrawBottomBar(const char *str) {
    //auto [rows, cols] = Dimensions();
    int y,x;
    // Save the cursor position (as we move it when we print the status bar)
    getyx(stdscr, y, x);

    attron(A_REVERSE);
    move(Dimensions().Height()-1, 0);
    clrtoeol();
    hline(' ', Dimensions().Width());

    mvprintw(Dimensions().Height()-1,0, "%s [%d:%d]", str, y, x - szGutter);
    attroff(A_REVERSE);
    // restore cursor to it's position before the status bar was drawn..
    move(y,x);
}


//std::pair<int, int> NCursesScreen::Dimensions() {
//    int row, col;
//    getmaxyx(stdscr,row,col);
//    return std::make_pair(row, col);
//}
gedit::Rect NCursesScreen::Dimensions() {
    int row, col;
    getmaxyx(stdscr,row,col);
    return gedit::Rect(col, row);
}


void NCursesScreen::Scroll(int nLines) {
    for(int i=0;i<nLines;i++) {
        scroll(stdscr);
    }
}

// Raw drawing stuff
void NCursesScreen::DrawCharAt(int x, int y, const char ch) {
    move(y, x);
    addch(ch);
}
void NCursesScreen::DrawStringAt(int x, int y, const char *str) {
    move(y, x);
    addstr(str);
}

void NCursesScreen::DrawRect(const gedit::Rect &rect) {
    auto topLeft = rect.TopLeft();
    auto bottomRight = rect.BottomRight();
    for(int x = topLeft.x;x<bottomRight.x;x++) {
        if (clipRect.PointInRect(x, topLeft.y)) DrawCharAt(x, topLeft.y, '-');
        if (clipRect.PointInRect(x, bottomRight.y)) DrawCharAt(x, bottomRight.y, '-');
    }

    for(int y = topLeft.y;y<bottomRight.y;y++) {
        if (clipRect.PointInRect(topLeft.x,y)) DrawCharAt(topLeft.x, y, '|');
        if (clipRect.PointInRect(bottomRight.x,y)) DrawCharAt(bottomRight.x, y, '|');
    }
}
