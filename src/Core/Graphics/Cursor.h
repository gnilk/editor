//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_CURSOR_H
#define EDITOR_CURSOR_H

#include <memory>
#include "Core/Point.h"

namespace gedit {
    struct Cursor {
        // How the caret is drawn: the editor's bar, or what a terminal application asked for (DECSCUSR)
        enum class Shape {
            kBar,
            kBlock,
            kUnderline,
        };

        gedit::Point position;
        int wantedColumn = 0;   // This is the virtual position (i.e. last-good)
        Shape shape = Shape::kBar;
        bool isVisible = true;  // a terminal application can hide it (DECTCEM)
    };
    // Name??
    struct LineCursor {

        //using Ref = std::shared_ptr<LineCursor>;
        using Ref = LineCursor *;

        Cursor cursor;
        size_t idxActiveLine = 0;
        int32_t viewTopLine = 0;
        int32_t viewBottomLine = 0;



        bool IsInside(int idxLine) {

            if ((idxLine >= viewTopLine) && (idxLine < viewBottomLine)) {
                return true;
            }
            return false;

//            if ((idxLine >= viewBottomLine) || (idxLine < viewTopLine)) {
//                return false;
//            }
//            return true;
        }
        int32_t Height() {
            return viewBottomLine - viewTopLine;
        }
    };
}


#endif //EDITOR_CURSOR_H
