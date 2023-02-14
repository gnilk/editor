//
// Created by gnilk on 14.02.23.
//

#ifndef EDITOR_DRAWCONTEXT_H
#define EDITOR_DRAWCONTEXT_H

#include <vector>

#include "Core/Rect.h"
#include "Core/Point.h"
#include "Core/Line.h"

namespace gedit {

    class DrawContext {
    public:
        // Parameters?
        DrawContext() = default;

        void Update(Rect &newClipRect);

        void SetTextColor();
        void SetTextAttributes();

        void DrawStringAt(const Point &pt, const char *str);
        void DrawStringAt(int x, int y, const char *str);

        void DrawLines(const std::vector<Line *> &lines, int idxActiveLine);
    private:
        Rect clipRect;  // Absolute coords
        Point offset;   // Offset from absolute to start of client

    };

}


#endif //EDITOR_DRAWCONTEXT_H
