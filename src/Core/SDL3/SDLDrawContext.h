//
// Created by gnilk on 29.03.23.
//

#ifndef STBMEETSDL_SDLDRAWCONTEXT_H
#define STBMEETSDL_SDLDRAWCONTEXT_H

#include "Core/DrawContext.h"
//#include "Core/Line.h"

namespace gedit {
    class SDLDrawContext : public DrawContext {
    public:
        SDLDrawContext() = default;
        explicit SDLDrawContext(NativeWindow window, Rect clientRect) : DrawContext(window, clientRect) {

        }
        virtual ~SDLDrawContext() = default;

        void Clear() override;
        void DrawStringAt(int x, int y, const char *str) override;
        void DrawStringWithAttributesAt(int x, int y, kTextAttributes attrib, const char *str) override;

//        void DrawLine(Line *line, int idxLine) override;
//        void DrawLines(const std::vector<Line *> &lines, int idxTopLine, int idxBottomLine) override;
//        void DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l) override;

        void ClearLine(int y) override;
        void FillLine(int y, kTextAttributes attrib, char c) override;
        void Scroll(int nRows) override;
    };
}


#endif //STBMEETSDL_SDLDRAWCONTEXT_H
