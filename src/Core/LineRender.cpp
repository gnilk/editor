//
// Created by gnilk on 31.03.23.
//

#include "DrawContext.h"
#include "logger.h"
#include "LineRender.h"

using namespace gedit;

// This assumes X = 0
void LineRender::DrawLines(const std::vector<Line *> &lines, int idxTopLine, int idxBottomLine, const Selection &selection) {
    auto rect = dc.GetRect();

    for (int i = idxTopLine; i < idxBottomLine; i++) {
        if (i >= lines.size()) {
            break;
        }
        auto line = lines[i];
        auto nCharToPrint = line->Length() > rect.Width() ? rect.Width() : line->Length();
        dc.ClearLine(i - idxTopLine);
        DrawLineWithAttributesAt(0, i - idxTopLine, nCharToPrint, *line, selection);
        dc.DrawLineOverlays(i - idxTopLine);
    }
}

// This is the more advanced drawing routine...
void LineRender::DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l, const Selection &selection) {

    // No attributes?  Just dump the string...
    auto &attribs = l.Attributes();
    if (attribs.size() == 0) {
        dc.DrawStringAt(x, y, l.Buffer().data());
        return;
    }

    // We split the line in attribute chunks and draw partial lines...
    auto itAttrib = attribs.begin();
    int xp = x;

    while (itAttrib != attribs.end()) {
        auto next = itAttrib + 1;
        size_t len = std::string::npos;
        // Not at the end - replace with length of this attribute
        if (next != attribs.end()) {
            // Some kind of assert!
            if (itAttrib->idxOrigString > next->idxOrigString) {
                auto logger = gnilk::Logger::GetLogger("SDLDrawContext");
                logger->Error("DrawLineWithAttributesAt, attribute index is wrong for line: '%s'", l.Buffer().data());
                return;
            }
            len = next->idxOrigString - itAttrib->idxOrigString;
        }

        // Grab the substring for this attribute range
        std::string strOut = std::string(l.Buffer().data(), itAttrib->idxOrigString, len);

        // Draw string with the correct color...
        dc.DrawStringWithAttributesAndColAt(xp,y, itAttrib->textAttributes, itAttrib->idxColor, strOut.c_str());

        xp += len;
        itAttrib = next;
    }


    // Selection should be between cursor positions...
    // ...Just testing...
//    if (l.IsSelected()) {
//        SDL_SetRenderDrawColor(renderer,80,100,128,64);
//        FillRect(x,y,GetRect().Width(), 1);
//    }

}

