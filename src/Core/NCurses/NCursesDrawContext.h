//
// Created by gnilk on 22.02.23.
//

#ifndef NCWIN_NCURSESDRAWCONTEXT_H
#define NCWIN_NCURSESDRAWCONTEXT_H

#include "Core/DrawContext.h"

namespace gedit {
    class NCursesDrawContext : public DrawContext {
    public:
        NCursesDrawContext() = default;
        explicit NCursesDrawContext(NativeWindow window, Rect clientRect) : DrawContext(window, clientRect) {

        }
        virtual ~NCursesDrawContext() = default;
        void Clear() override;
        void DrawStringAt(int x, int y, const char *str) override;
        void Scroll(int nRows) override;

    };
}

#endif //NCWIN_NCURSESDRAWCONTEXT_H
