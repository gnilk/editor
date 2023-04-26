//
// Created by gnilk on 31.03.23.
//

#ifndef EDITOR_LINERENDER_H
#define EDITOR_LINERENDER_H

#include <vector>
#include "Core/Line.h"
#include "Core/DrawContext.h"
#include "Core/EditorModel.h"   // FIXME: Only because of selection

namespace gedit {
    class LineRender {
    public:
        LineRender(DrawContext &drawContext) : dc(drawContext) {

        }
        void DrawLine(Line::Ref line, int idxLine);
        void DrawLines(const std::vector<Line::Ref> &lines, int idxTopLine, int idxBottomLine, const Selection &selection);
        void DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l, const Selection &selection);

    private:
        const DrawContext &dc;
    };
}


#endif //EDITOR_LINERENDER_H
