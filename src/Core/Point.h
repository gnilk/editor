//
// Created by gnilk on 13.02.23.
//

#ifndef EDITOR_POINT_H
#define EDITOR_POINT_H

namespace gedit {
    struct Point {
        Point() = default;

        void Move(int xDelta, int yDelta) {
            x += xDelta;
            y += yDelta;
        }

        int x = 0;
        int y = 0;
    };

}

#endif //EDITOR_POINT_H
