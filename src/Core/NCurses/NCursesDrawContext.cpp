//
// Created by gnilk on 22.02.23.
//

#include <ncurses.h>
#include "logger.h"
#include "NCursesDrawContext.h"
#include "NCursesTranslate.h"
#include <string.h>
#include "logger.h"
#include "Core/StrUtil.h"

using namespace gedit;

static int attribToNCAttrib(kTextAttributes attrib);


void NCursesDrawContext::Clear() const {

    SetRenderColors();
    FillRect(0,0,rect.Width(), rect.Height(), true);

//    for(int y=0;y<rect.Height()-1;y++) {
//        wmove((WINDOW *)win, y + rect.TopLeft().y, 0);
//        waddstr((WINDOW *)win, clrstr.c_str());
//    }
//    wmove((WINDOW *)win, rect.Height(), 0);
//    waddnstr((WINDOW *)win, clrstr.c_str(),rect.Width()-1);
//    winsch((WINDOW *) win, ' ');
}

std::pair<float, float> NCursesDrawContext::CoordsToScreen(float x, float y) const {
    auto pixWinOfs = NCursesTranslate::RowColToPixel(rect.TopLeft());

    float screenXPos = NCursesTranslate::ColToXPos(x) + pixWinOfs.x;
    float screenYPos = NCursesTranslate::RowToYPos(y) + pixWinOfs.y;

    return {screenXPos, screenYPos};
}


// Fill Rect use current color
void NCursesDrawContext::FillRect(float x, float y, float w, float h, bool isColorSet) const {
    auto [pixXStart, pixYStart] = CoordsToScreen(x, y);

    auto pixWidth = NCursesTranslate::ColToXPos(w);
    auto pixHeight = NCursesTranslate::RowToYPos(h);

    if (!isColorSet) {
        SetRenderColors();
    }
    // hmm... we can cache this one...
    std::string clrstr(pixWidth, ' ');

    for(int line = 0; line < pixHeight; line++) {
        move(line + pixYStart,pixXStart);
        addnstr(clrstr.c_str(), pixWidth-1);
        insch(' ');

//        mvaddnstr(line + pixYStart, pixXStart, clrstr.c_str(), pixWidth);
    }
}

void NCursesDrawContext::Scroll(int nRows) const {

    // not needed..

}

void NCursesDrawContext::ClearLine(int y) const {
    return;
//    auto [px, py] = CoordsToScreen(0, y);
//    auto pixWidth = NCursesTranslate::ColToXPos(rect.Width());
//
//    move(py,px);
//    std::string clrstr(pixWidth, ' ');
//    addnstr(clrstr.c_str(), pixWidth);

}

void NCursesDrawContext::FillLine(int y, kTextAttributes attrib, char c) const {
    auto [px, py] = CoordsToScreen(0, y);
    auto pixWidth = NCursesTranslate::ColToXPos(rect.Width());

    SetRenderColors();
    auto ncAttrib = attribToNCAttrib(attrib);
    attron(ncAttrib);

    move(py,px);
    std::string clrstr(pixWidth, c);
    addnstr(clrstr.c_str(), pixWidth-1);
    insch(c);
}

void NCursesDrawContext::DrawStringAt(int x, int y, const char *str) const {
    SetRenderColors();

    // Note: In NCurses we can't have newlines...
    std::string trimmedString(str);
    strutil::rtrim(trimmedString);

    auto [px, py] = CoordsToScreen(x, y);
    mvaddstr(py, px, trimmedString.c_str());
}

//
// There is something wrong with the color_pair, all I get is 'black'
//
void NCursesDrawContext::DrawLineOverlays(int y) const {
    for(auto &overlay : overlays) {
        DrawLineOverlay(y, overlay);
    }
}
void NCursesDrawContext::DrawLineOverlay(int y, const Overlay &overlay) const {
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

    SetRenderColors();
    auto ncAttr = attribToNCAttrib(attrib);
    wattrset((WINDOW *)win, A_NORMAL);  // Reset to normal
    wattron((WINDOW *)win, ncAttr);     // Enable whatever we have
    wattron((WINDOW *)win, COLOR_PAIR(activeColorPair));

    auto [px, py] = CoordsToScreen(x, y);
    mvaddstr(py, px, str);

//    int err = wmove((WINDOW *)win, y, x);
//    if (err < 0) {
//        return;
//    }
//    waddnstr((WINDOW *)win, str, rect.Width()-x-1);
//    // To occupy the last char we need to do this
//    if (strlen(str) >= rect.Width()) {
//        winsch((WINDOW *) win, str[rect.Width()-1]);
//    }
//
//    wattrset((WINDOW *)win, A_NORMAL);
}

void NCursesDrawContext::DrawStringWithAttributesAndColAt(int x, int y, kTextAttributes attrib, int idxColor, const char *str) const {

    exit(1);

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
        for(auto &overlay : overlays) {
            if (overlay.isActive && overlay.IsInside(x + i, y)) {
                wattrset((WINDOW *) win, A_NORMAL);  // Reset to normal
                wattron((WINDOW *) win, ncAttr);     // Enable whatever we have
                wattron((WINDOW *) win, A_REVERSE);
                bWasActive = true;
            } else if (bWasActive) {
                wattrset((WINDOW *) win, A_NORMAL);  // Reset to normal
                wattron((WINDOW *) win, ncAttr);     // Enable whatever we have
                wattron((WINDOW *) win, ncColorAttrib);
                bWasActive = false;
            }
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

void NCursesDrawContext::OnColorUpdate() const {
    SetRenderColors();
}
void NCursesDrawContext::SetRenderColors() const {
    // The colors can't be const as we perhaps need to modify them
    auto colorPair = NCursesColorRepository::Instance().GetColorPairIndex(fgColor, bgColor);
    //auto colorPair = const_cast<NCursesDrawContext *>(this)->.GetColorPairIndex(fgColor, bgColor);
    const_cast<NCursesDrawContext *>(this)->activeColorPair = colorPair;


    attrset(A_NORMAL);  // Reset to normal?? - should we?
    attron(COLOR_PAIR(activeColorPair)); // COLOR_PAIR(activeColorPair));
}

void NCursesDrawContext::DrawCursor(const gedit::Cursor &cursor) const {

    auto [cx, cy] = CoordsToScreen(cursor.position.x, cursor.position.y);
    auto logger = gnilk::Logger::GetLogger("NCursesDrawContext");
    logger->Debug("DrawCursor, abs pos: %d, %d", (int)cx,(int)cy);
    move((int)cy, (int)cx);

//    auto pixWinOfs = NCursesTranslate::RowColToPixel(windowRect.TopLeft());
//    float screenXPos = NCursesTranslate::ColToXPos(win_x) + pixWinOfs.x;
//    float screenYPos = NCursesTranslate::RowToYPos(win_y) + pixWinOfs.y;
//
//
//
//    auto logger = gnilk::Logger::GetLogger("NCursesWindow");
//    logger->Debug("SetCursor, pos=%d:%d, translation: %d:%d", cursor.position.x, cursor.position.y, win_x, win_y);
//    move(cursor.position.y + screenYPos, cursor.position.x + screenXPos);

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
