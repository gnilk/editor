//
// Created by gnilk on 31.03.23.
//

#ifndef EDITOR_LINERENDER_H
#define EDITOR_LINERENDER_H

#include <vector>
#include "Core/Line.h"
#include "Core/DrawContext.h"

namespace gedit {
    class LineRender {
    public:
        LineRender(const DrawContext &drawContext) : dc(drawContext) {

        }
        void DrawLine(Line *line, int idxLine);
        void DrawLines(const std::vector<Line *> &lines, int idxTopLine, int idxBottomLine);
        void DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l);

    private:
        const DrawContext &dc;
    };
}


#endif //EDITOR_LINERENDER_H
