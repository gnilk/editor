//
// Created by gnilk on 31.03.23.
//

#ifndef EDITOR_LINERENDER_H
#define EDITOR_LINERENDER_H

#include <vector>
#include "Core/Line.h"
#include "Core/UI/Graphics/DrawContext.h"
#include "Core/Document.h"   // Because of selection in drawing routines...

namespace gedit {
    class LineRender {
    public:
        LineRender(DrawContext &drawContext, int tabSize = 4) : dc(drawContext), tabSize(tabSize) {

        }
        void DrawLines(const std::vector<Line::Ref> &lines, int idxTopLine, int idxBottomLine, const Selection &selection);
        void DrawLine(int x, int y, const Line::Ref line);

    protected:
        void DrawLineWithAttributesAt(int x, int y, const Line::Ref line);
        void DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l, const Selection &selection);

        // Expand tabs in 'in' to spaces, advancing to the next tabSize stop. 'startCol' is the
        // line-relative column of the first character (so tab stops are computed correctly for a
        // chunk that does not start at column 0). The buffer is never modified - render-only copy.
    public:
        static std::u32string ExpandTabs(const std::u32string &in, int startCol, int tabSize);
    protected:
    private:
//        using AttributeStringDelegate = std::function<void(const Line::LineAttribIterator &itAttrib, std::u32string &strOut)>;
//        void Iterate(const Line::Ref line, AttributeStringDelegate callback);
        const DrawContext &dc;
        int tabSize = 4;
    };
}


#endif //EDITOR_LINERENDER_H
