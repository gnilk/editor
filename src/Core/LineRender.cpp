//
// Created by gnilk on 31.03.23.
//

#include "DrawContext.h"
#include "logger.h"
#include "LineRender.h"
#include "Editor.h"
using namespace gedit;

// This assumes X = 0
void LineRender::DrawLines(const std::vector<Line *> &lines, int idxTopLine, int idxBottomLine, const Selection &selection) {
    auto rect = dc.GetRect();

    for (int i = idxTopLine; i < idxBottomLine; i++) {
        if (i >= lines.size()) {
            break;
        }
        auto line = lines[i];
        line->Lock();
        auto nCharToPrint = line->Length() > rect.Width() ? rect.Width() : line->Length();
        dc.ClearLine(i - idxTopLine);
        DrawLineWithAttributesAt(0, i - idxTopLine, nCharToPrint, *line, selection);
        dc.DrawLineOverlays(i - idxTopLine);
        line->Release();
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
        auto [fgColor, bgColor] = Editor::Instance().ColorFromLanguageToken(itAttrib->tokenClass);
        dc.SetColor(fgColor, bgColor);
        dc.DrawStringWithAttributesAt(xp,y, itAttrib->textAttributes, strOut.c_str());

        xp += len;
        itAttrib = next;
    }

}

