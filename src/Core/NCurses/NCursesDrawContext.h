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

        void Clear() const override;
        void Scroll(int nRows) const override;

        void ClearLine(int y) const override;
        void FillLine(int y, kTextAttributes attrib, char c) const override;
        void DrawLineOverlays(int y) const override;

        void DrawStringAt(int x, int y, const char *str) const override;
        void DrawStringWithAttributesAt(int x, int y, kTextAttributes attrib, const char *str) const override;
        void DrawStringWithAttributesAndColAt(int x, int y, kTextAttributes attrib, int idxColor, const char *str) const override;


//        void DrawLines(const std::vector<Line *> &lines, int idxTopLine, int idxBottomLine) override;
//        void DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l) override;


    };
}

#endif //NCWIN_NCURSESDRAWCONTEXT_H
