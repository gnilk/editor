//
// Created by gnilk on 22.02.23.
//

#include <ncurses.h>
#include "NCursesDrawContext.h"

using namespace gedit;

void NCursesDrawContext::Clear() const {
    wclear((WINDOW *)win);
}

void NCursesDrawContext::Scroll(int nRows) const {
    wscrl((WINDOW *)win, nRows);
}

void NCursesDrawContext::ClearLine(int y) const {
    wclrtoeol((WINDOW *)win);
}

void NCursesDrawContext::FillLine(int y, kTextAttributes attrib, char c) const {
    std::string fillStr(rect.Width(), c);
    DrawStringWithAttributesAt(0,y,attrib, fillStr.c_str());

}

void NCursesDrawContext::DrawStringAt(int x, int y, const char *str) const {
    int err = wmove((WINDOW *)win, y, x);
    if (err < 0) {
        return;
    }
    waddnstr((WINDOW *)win, str, rect.Width()-x-1);
}

static int attribToNCAttrib(kTextAttributes attrib) {
    int ncAttrib = A_NORMAL;
    if (attrib & kTextAttributes::kInverted) {
        ncAttrib |= A_REVERSE;
    }
    if (attrib & kTextAttributes::kBold) {
        ncAttrib |= A_BOLD;
    }
    if (attrib & kTextAttributes::kItalic) {
        ncAttrib |= A_ITALIC;
    }
    if (attrib & kTextAttributes::kUnderline) {
        ncAttrib |= A_UNDERLINE;
    }
    return ncAttrib;
}

void NCursesDrawContext::DrawStringWithAttributesAt(int x, int y, kTextAttributes attrib, const char *str) const {
    auto ncAttr = attribToNCAttrib(attrib);
    wattrset((WINDOW *)win, A_NORMAL);  // Reset to normal
    wattron((WINDOW *)win, ncAttr);     // Enable whatever we have
    int err = wmove((WINDOW *)win, y, x);
    if (err < 0) {
        return;
    }
    waddnstr((WINDOW *)win, str, rect.Width()-x-1);
    // To occupy the last char we need to do this
    if (strlen(str) >= rect.Width()) {
        winsch((WINDOW *) win, str[rect.Width()-1]);
    }

    wattrset((WINDOW *)win, A_NORMAL);
}


//void NCursesDrawContext::DrawLines(const std::vector<Line *> &lines, int idxTopLine, int idxBottomLine) {
//    for(int i=idxTopLine;i<idxBottomLine;i++) {
//        if (i >= lines.size()) {
//            break;
//        }
//        auto line = lines[i];
//        auto nCharToPrint = line->Length()>rect.Width()?rect.Width():line->Length();
//        DrawLineWithAttributesAt(0,i - idxTopLine, nCharToPrint, *line);
//    }
//}
//
//void NCursesDrawContext::DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l) {
//    wmove((WINDOW *)win, y, x);
//    // Reset attributes before drawing..
//    wattrset((WINDOW *)win, A_NORMAL);
//
//    auto &attribs = l.Attributes();
//
//    int idxColorPair = 0;
//    // If no attribs - just dump it out...
//    if (attribs.size() == 0) {
//        for (int i = 0; i < nCharToPrint; i++) {
//            waddch((WINDOW *)win, l.Buffer().at(i));
//        }
//        return;
//    }
//
//    int idxAttrib = 0;
//    attr_t attrib;
//    auto itAttrib = attribs.begin();
//    //int cNext = attribs[0].cStart;
//    for (int i = 0; i < nCharToPrint; i++) {
//        if (i >= itAttrib->idxOrigString) {
//            // FIXME: Convert - must be done in driver...
//            attrib = COLOR_PAIR(itAttrib->idxColor);
//            itAttrib++;
//            if (itAttrib == attribs.end()) {
//                --itAttrib;
//            }
//        }
//        wattrset((WINDOW *)win, attrib);
//        // Note: Improve selection visualization - allow a specific selection background
//        //       set the selection background for all foregrounds add to a new pair...
//        //       THIS should be done "automatically" by the driver...
//        if (l.IsSelected()) {
//            wattron((WINDOW *)win, A_REVERSE);
//        }
//        waddch((WINDOW *)win, l.Buffer().at(i));
//        if (l.IsSelected()) {
//            wattroff((WINDOW *)win, A_REVERSE);
//        }
//    }
//    wattrset((WINDOW *)win, A_NORMAL);
//}

