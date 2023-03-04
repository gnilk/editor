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

void NCursesDrawContext::DrawLine(Line *line, int idxLine) {

}

void NCursesDrawContext::DrawLines(const std::vector<Line *> &lines, int idxTopLine, int idxBottomLine) {
    for(int i=idxTopLine;i<idxBottomLine;i++) {
        if (i >= lines.size()) {
            break;
        }
        auto line = lines[i];
        auto nCharToPrint = line->Length()>rect.Width()?rect.Width():line->Length();
        DrawLineWithAttributesAt(0,i - idxTopLine, nCharToPrint, *line);
    }
}

void NCursesDrawContext::DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l) {
    wmove((WINDOW *)win, y, x);

    auto &attribs = l.Attributes();

    int idxColorPair = 0;
    // If no attribs - just dump it out...
    if (attribs.size() == 0) {
        for (int i = 0; i < nCharToPrint; i++) {
            waddch((WINDOW *)win, l.Buffer().at(i));
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
        wattrset((WINDOW *)win, attrib);
        // Note: Improve selection visualization - allow a specific selection background
        //       set the selection background for all foregrounds add to a new pair...
        //       THIS should be done "automatically" by the driver...
        if (l.IsSelected()) {
            wattron((WINDOW *)win, A_REVERSE);
        }
        waddch((WINDOW *)win, l.Buffer().at(i));
        if (l.IsSelected()) {
            wattroff((WINDOW *)win, A_REVERSE);
        }
    }
    wattrset((WINDOW *)win, A_NORMAL);
}

void NCursesDrawContext::ClearLine(int y) {
    wclrtoeol((WINDOW *)win);
}
