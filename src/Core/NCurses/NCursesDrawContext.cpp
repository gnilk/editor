//
// Created by gnilk on 20.02.23.
//

#include "NCursesDrawContext.h"

using namespace gedit;

void NCursesDrawContext::Clear() {
    wclear((WINDOW *)win);
}

void NCursesDrawContext::Fill(const Rect &rect, char ch) {
    std::string clrStr(clipRect.Width(), ch);

    Point ptStart = rect.TopLeft(); //ToScreen(rect.TopLeft());

    for(int i=0;i<ContextRect().Height();i++) {
        int nCharToPrint = clrStr.length()>clipRect.Width()?(clipRect.Width()):clrStr.length();
        DrawStringAt(ptStart.x, ptStart.y+i, nCharToPrint, clrStr.c_str());
    }
}

void NCursesDrawContext::DrawRect(const gedit::Rect &rect) {
    auto topLeft = rect.TopLeft();
    auto bottomRight = rect.BottomRight();

    for(int x = topLeft.x;x<bottomRight.x;x++) {
        DrawCharAt(x, topLeft.y, '-');
        DrawCharAt(x, bottomRight.y, '-');
    }

    for(int y = topLeft.y;y<bottomRight.y;y++) {
        DrawCharAt(topLeft.x, y, '|');
        DrawCharAt(bottomRight.x, y, '|');
    }
}
void NCursesDrawContext::DrawVLine(const gedit::Point &ptStart, const gedit::Point &ptEnd) {
    for(int y = ptStart.y; y<ptEnd.y; y++) {
        DrawCharAt(ptStart.x, y, '|');
    }
}
void NCursesDrawContext::DrawHLine(const gedit::Point &ptStart, const gedit::Point &ptEnd) {
    for(int x = ptStart.x; x<ptEnd.x; x++) {
        DrawCharAt(x, ptStart.y, '-');
    }
}


void NCursesDrawContext::DrawCharAt(int x, int y, const char ch) {
    wmove((WINDOW *)win, y, x);
    waddch((WINDOW *)win, ch);
}

void NCursesDrawContext::DrawStringAt(const Point &pt, const char *str) {
    wmove((WINDOW *)win, pt.y, pt.x);

    waddstr((WINDOW *)win, str);

}

void NCursesDrawContext::DrawStringAt(int x, int y, const char *str) {
    wmove((WINDOW *)win, y, x);
    waddstr((WINDOW *)win, str);
}

void NCursesDrawContext::DrawStringAt(int x, int y, int nCharToPrint, const char *str) {
    move(y, x);
    waddnstr((WINDOW *)win,str,nCharToPrint);
}

void NCursesDrawContext::DrawLines(const std::vector<Line *> &lines, int idxTopLine, int idxBottomLine) {
    for(int i=idxTopLine;i<idxBottomLine;i++) {
        int idxLine = i;
        if (idxLine >= lines.size()) break;

        auto line = lines[idxLine];
        // Clip
        int nCharToPrint = line->Length()>clipRect.Width()?(clipRect.Width()):line->Length();
        // Translate to screen coords and draw...
        DrawLineWithAttributesAt(0, (i-idxTopLine), nCharToPrint, *line);
    }
}

void NCursesDrawContext::DrawLine(Line *line, int idxLine) {
    int nCharToPrint = line->Length()>clipRect.Width()?(clipRect.Width()):line->Length();
    DrawLineWithAttributesAt(0, idxLine, nCharToPrint, *line);
}

void NCursesDrawContext::DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l) {
    wmove((WINDOW *)win, y, x);
    wclrtoeol((WINDOW *)win);
    DrawLineWithAttributes(l, nCharToPrint);
}

void NCursesDrawContext::DrawLineWithAttributes(Line &l, int nCharToPrint) {
    auto &attribs = l.Attributes();

    int idxColorPair = 0;
    // If no attribs - just dump it out...
    if (attribs.size() == 0) {
        for (int i = 0; i < l.Length(); i++) {
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
            wattron((WINDOW *)win,A_REVERSE);
        }
        waddch((WINDOW *)win,l.Buffer().at(i));
        if (l.IsSelected()) {
            wattroff((WINDOW *)win,A_REVERSE);
        }
    }
    wattrset((WINDOW *)win,A_NORMAL);
}


void NCursesDrawContext::Scroll(int nLines) {
    wscrl((WINDOW *)win, nLines);
}
