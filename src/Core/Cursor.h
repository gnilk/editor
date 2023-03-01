//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_CURSOR_H
#define EDITOR_CURSOR_H

#include "Core/Point.h"
namespace gedit {
    struct Cursor {
        gedit::Point position;
        int wantedColumn = 0;   // This is the virtual position (i.e. last-good)
    };
}


#endif //EDITOR_CURSOR_H
