//
// Created by gnilk on 20.02.23.
//

#ifndef EDITOR_NCURSESDRAWCONTEXT_H
#define EDITOR_NCURSESDRAWCONTEXT_H

#include <ncurses.h>
#include "Core/NativeWindow.h"
#include "Core/DrawContext.h"
#include "Core/Line.h"

namespace gedit {
    class NCursesDrawContext : public DrawContext {
    public:
        // In case we call the default CTOR - forward with the NCurses root-screen
        NCursesDrawContext() : DrawContext(stdscr) {

        }
        explicit NCursesDrawContext(NativeWindowHandle window) : DrawContext(window) {

        }
        virtual ~NCursesDrawContext() = default;

        virtual void Clear();
        virtual void Fill(const Rect &rect, char ch);


        void DrawCharAt(int x, int y, const char ch) override;
        void DrawStringAt(const Point &pt, const char *str) override;
        void DrawStringAt(int x, int y, const char *str) override;
        void DrawStringAt(int x, int y, int nCharToPrint, const char *str) override;

        void DrawRect(const gedit::Rect &rect) override;
        void DrawVLine(const gedit::Point &ptStart, const gedit::Point &ptEnd) override;
        void DrawHLine(const gedit::Point &ptStart, const gedit::Point &ptEnd) override;

        void DrawLine(Line *line, int idxLine) override;
        void DrawLines(const std::vector<Line *> &lines, int idxTopLine, int idxBottomLine) override;
        void DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l) override;

        void Scroll(int nLines) override;
    protected:
        void DrawLineWithAttributes(Line &l, int nCharToPrint);

    private:
    };
}


#endif //EDITOR_NCURSESWINDRAWCONTEXT_H
