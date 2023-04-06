//
// Created by gnilk on 22.02.23.
//

#include <ncurses.h>
#include "logger.h"
#include "NCursesDrawContext.h"
#include <string.h>
//
// Note: no editor output since I am missing: DrawStringWithAttributesAndColAt
//


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

//
// There is something wrong with the color_pair, all I get is 'black'
//
void NCursesDrawContext::DrawLineOverlays(int y) const {
    return;
    if (!overlay.isActive) return;
    if (!overlay.IsLinePartiallyCovered(y)) {
        return;
    }
    // Assume fully covered line...
    int start = 0;
    int end = GetRect().Width();
    // If only partially covered, take start/end column values depending on which line we are rendering
    if(overlay.IsLinePartiallyCovered(y)) {
        if (y == overlay.start.y) start = overlay.start.x;
        if (y == overlay.end.y) end = overlay.end.x;
    }

    // Color argument don't work..
    // It is supposed to be just the number we give when register
    // From docs 'the color argument is a color-pair index (as in the first argument of init_pair, see curs_color(3X)'
    int res = mvwchgat((WINDOW *)win, y, start, end, A_NORMAL, 2, nullptr);
    if (res == ERR) {
        exit(1);
    }
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

void NCursesDrawContext::DrawStringWithAttributesAndColAt(int x, int y, kTextAttributes attrib, int idxColor, const char *str) const {

    wmove((WINDOW *)win, y, x);

    auto ncAttr = attribToNCAttrib(attrib);
    auto ncColorAttrib = COLOR_PAIR(idxColor);
    wattrset((WINDOW *)win, A_NORMAL);  // Reset to normal
    wattron((WINDOW *)win, ncAttr);     // Enable whatever we have
    wattron((WINDOW *)win, ncColorAttrib);

    bool bWasActive = false;
    for(int i=0;i<strlen(str);i++) {
        // Note: This can be removed if I manage to get the drawoverlay function to work..
        // Switch on selection drawing if we are inside the overlay
        if (overlay.isActive && overlay.IsInside(x+i, y)) {
            wattrset((WINDOW *)win, A_NORMAL);  // Reset to normal
            wattron((WINDOW *)win, ncAttr);     // Enable whatever we have
            wattron((WINDOW *)win, A_REVERSE);
            bWasActive = true;
        } else if (bWasActive) {
            wattrset((WINDOW *)win, A_NORMAL);  // Reset to normal
            wattron((WINDOW *)win, ncAttr);     // Enable whatever we have
            wattron((WINDOW *)win, ncColorAttrib);
            bWasActive = false;
        }
        waddch((WINDOW *)win, str[i]);
    }
//    waddstr((WINDOW *)win, str);
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

