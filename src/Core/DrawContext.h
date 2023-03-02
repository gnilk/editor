//
// Created by gnilk on 22.02.23.
//

#ifndef NCWIN_DRAWCONTEXT_H
#define NCWIN_DRAWCONTEXT_H

#include <vector>

#include "Core/NativeWindow.h"
#include "Core/Line.h"
#include "Core/Rect.h"

namespace gedit {
    class DrawContext {
    public:
        DrawContext() = default;
        explicit DrawContext(NativeWindow window, Rect clientRect) : win(window), rect(clientRect) {
        }
        virtual ~DrawContext() = default;
        virtual void DrawStringAt(int x, int y, const char *str) {}
        virtual void DrawLine(Line *line, int idxLine) {}
        virtual void DrawLines(const std::vector<Line *> &lines, int idxTopLine, int idxBottomLine) {}
        virtual void DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l) {}

        virtual void Clear() {}
        virtual void Scroll(int nRows) {}
        virtual const Rect &GetRect() {
            return rect;
        }
    protected:
        Rect rect = {};
        NativeWindow win = nullptr;
    };
}
#endif //NCWIN_DRAWCONTEXT_H
