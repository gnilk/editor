//
// Created by gnilk on 22.02.23.
//

#ifndef NCWIN_NCURSESDRAWCONTEXT_H
#define NCWIN_NCURSESDRAWCONTEXT_H

#include "Core/DrawContext.h"
#include "Core/Line.h"

namespace gedit {
    class NCursesDrawContext : public DrawContext {
    public:
        NCursesDrawContext() = default;
        explicit NCursesDrawContext(NativeWindow window, Rect clientRect) : DrawContext(window, clientRect) {

        }
        virtual ~NCursesDrawContext() = default;
        void Clear() override;
        void DrawStringAt(int x, int y, const char *str) override;
        void DrawLine(Line *line, int idxLine) override;
        void DrawLines(const std::vector<Line *> &lines, int idxTopLine, int idxBottomLine) override;
        void DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l) override;

        void Scroll(int nRows) override;

    };
}

#endif //NCWIN_NCURSESDRAWCONTEXT_H
