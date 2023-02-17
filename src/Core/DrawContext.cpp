//
// Created by gnilk on 14.02.23.
//
#include "DrawContext.h"
#include "Core/RuntimeConfig.h"
#include "logger.h"

using namespace gedit;
// Absolute coordinates
void DrawContext::Update(Rect &newClipRect) {
    clipRect = newClipRect;
    offset = clipRect.TopLeft();
}

void DrawContext::SetTextColor() {

}

void DrawContext::SetTextAttributes() {

}

Point DrawContext::ToScreen(Point pt) {
    Point ptRes;
    ptRes.x = pt.x + offset.x;
    ptRes.y = pt.y + offset.y;
    return ptRes;
}


//
// NOTE: 'Rect' is in Screen-Coords
// Currently you can pass the view's 'ContentRect' directly...
//
void DrawContext::Fill(const Rect &rect, char ch) {
    auto screen = RuntimeConfig::Instance().Screen();
    std::string clrStr(screen->Dimensions().Width(), ch);

    Point ptStart = rect.TopLeft(); //ToScreen(rect.TopLeft());

    for(int i=0;i<ContextRect().Height();i++) {
        int nCharToPrint = clrStr.length()>clipRect.Width()?(clipRect.Width()):clrStr.length();
        screen->DrawStringAt(ptStart.x, ptStart.y+i, nCharToPrint, clrStr.c_str());
    }

}


void DrawContext::Clear() {
    Fill(ContextRect(), ' ');
}



// Relative coordinates
void DrawContext::DrawStringAt(const Point &pt, const char *str) {
    DrawStringAt(pt.x, pt.y, str);
}

// Relative coords
void DrawContext::DrawStringAt(int x, int y, const char *str) {
    auto screen = RuntimeConfig::Instance().Screen();
    // Translate to absolute coordinates
    x += offset.x;
    y += offset.y;

    // FIXME: This needs proper clipping!!!
    if (clipRect.PointInRect(x,y)) {

        screen->DrawStringAt(x, y, str);
    }
}

static int old_active = -1;
void DrawContext::DrawLines(const std::vector<Line *> &lines, int idxTopLine, int idxBottomLine) {
    auto screen = RuntimeConfig::Instance().Screen();

    for(int i=idxTopLine;i<idxBottomLine;i++) {
        int idxLine = i;
        if (idxLine >= lines.size()) break;

        auto line = lines[idxLine];
        // Clip
        int nCharToPrint = line->Length()>clipRect.Width()?(clipRect.Width()):line->Length();
        // Translate to screen coords and draw...
        screen->DrawLineWithAttributesAt(offset.x, (i-idxTopLine)+offset.y, nCharToPrint, *line);
    }
}
void DrawContext::DrawLine(Line *line, int idxLine) {
    auto screen = RuntimeConfig::Instance().Screen();

    // Clip
    int nCharToPrint = line->Length()>clipRect.Width()?(clipRect.Width()):line->Length();
    // Translate to screen coords and draw...
    screen->DrawLineWithAttributesAt(offset.x, idxLine+offset.y, nCharToPrint, *line);
}
