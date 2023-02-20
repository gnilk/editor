//
// Created by gnilk on 14.02.23.
//

#ifndef EDITOR_DRAWCONTEXT_H
#define EDITOR_DRAWCONTEXT_H

#include <vector>

#include "Core/Rect.h"
#include "Core/Point.h"
#include "Core/Line.h"
#include "Core/NativeWindowHandle.h"

namespace gedit {

    class DrawContext {
    public:
        // Parameters?
        DrawContext() : win(nullptr) {

        }
        explicit DrawContext(NativeWindowHandle window) : win(window) {

        }

        const Rect &ContextRect() {
            return clipRect;
        }

        void SetNativeWindow(NativeWindowHandle window) {
            win = window;
        }

        virtual void Update(Rect &newClipRect);
        virtual void Clear();
        virtual void Fill(const Rect &rect, char ch);

        virtual void SetTextColor();
        virtual void SetTextAttributes();

        virtual Point ToScreen(Point pt);

        // It should now be possible to remove these..
        virtual void DrawCharAt(int x, int y, const char ch) {};
        virtual void DrawStringAt(const Point &pt, const char *str);
        virtual void DrawStringAt(int x, int y, const char *str);
        virtual void DrawStringAt(int x, int y, int nCharToPrint, const char *str) {};

        virtual void DrawRect(const gedit::Rect &rect) {}
        virtual void DrawVLine(const gedit::Point &ptStart, const gedit::Point &ptEnd) {}
        virtual void DrawHLine(const gedit::Point &ptStart, const gedit::Point &ptEnd) {}


        virtual void DrawLine(Line *line, int idxLine);
        virtual void DrawLines(const std::vector<Line *> &lines, int idxTopLine, int idxBottomLine);
        virtual void DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l) {}

        virtual void Scroll(int nLines) {}


    protected:
        NativeWindowHandle win;
        Rect clipRect;  // Absolute coords
    private:
        Point offset;   // Offset from absolute to start of client

    };

}


#endif //EDITOR_DRAWCONTEXT_H
