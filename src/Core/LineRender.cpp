//
// Created by gnilk on 31.03.23.
//

#include "DrawContext.h"
#include "logger.h"
#include "LineRender.h"
#include "Editor.h"
#include "Core/Editor.h"
using namespace gedit;

// This assumes X = 0
void LineRender::DrawLines(const std::vector<Line::Ref> &lines, int idxTopLine, int idxBottomLine, const Selection &selection) {
    auto &rect = dc.GetRect();
    auto &theme = Editor::Instance().GetTheme();
    auto contentColors = theme->GetContentColors();


    for (int i = idxTopLine; i < idxBottomLine; i++) {
        if (i >= (int)lines.size()) {
            break;
        }
        auto line = lines[i];

        // I've seen a crash ONCE where there was interference between this and the language parser
        // But I couldn't determine the crash, std::mutex (down the drain) would throw an exception..
//        if (line->IsLocked()) {
//            continue; // This is fishy - we should probably log this
//        }

        line->Lock();
        auto nCharToPrint = line->Length() > static_cast<size_t>(rect.Width()) ? static_cast<size_t>(rect.Width()) : line->Length();
        dc.ClearLine(i - idxTopLine);
        DrawLineWithAttributesAt(0, i - idxTopLine, nCharToPrint, *line, selection);


        dc.SetFGColor(contentColors["selection"]);
        dc.DrawLineOverlays(i - idxTopLine);
        line->Release();
    }
}
void LineRender::DrawLine(int x, int y, const Line::Ref line) {
    line->Lock();
    DrawLineWithAttributesAt(x,y, line);
    line->Release();
}


// This is the more advanced drawing routine...
void LineRender::DrawLineWithAttributesAt(int x, int y, const Line::Ref line) {

    // No attributes?  Just dump the string...
    auto &attribs = line->Attributes();
    if (attribs.size() == 0) {
        dc.DrawStringAt(x, y, line->Buffer().data());
        return;
    }
    // We split the line in attribute chunks and draw partial lines...
    int xp = x;

    auto callback = [&xp,y, this](const Line::LineAttribIterator &itAttrib, std::u32string &strOut) {
        dc.SetColor(itAttrib->foregroundColor, itAttrib->backgroundColor);
        dc.DrawStringWithAttributesAt(xp,y, itAttrib->textAttributes, strOut);
        xp += strOut.length();
    };

    line->IterateWithAttributes(callback);
}


// This is the more advanced drawing routine...
// FIXME: Remove this...
void LineRender::DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l, const Selection &selection) {

    // No attributes?  Just dump the string...
    auto &attribs = l.Attributes();
    if (attribs.size() == 0) {
        dc.DrawStringAt(x, y, l.Buffer().data());
        return;
    }

    int xp = x;
    auto callback = [&xp,y, this](const Line::LineAttribIterator &itAttrib, std::u32string &strOut) {
        dc.SetColor(itAttrib->foregroundColor, itAttrib->backgroundColor);
        dc.DrawStringWithAttributesAt(xp,y, itAttrib->textAttributes, strOut);
        xp += strOut.length();
    };

    l.IterateWithAttributes(callback);
}

