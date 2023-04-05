//
// Created by gnilk on 13.02.23.
//

#ifndef EDITOR_POINT_H
#define EDITOR_POINT_H

namespace gedit {
    struct Point {
        Point() = default;
        Point(int xp, int yp) : x(xp), y(yp) {

        }

        void Move(int xDelta, int yDelta) {
            x += xDelta;
            y += yDelta;
        }

        // Need X as well
        bool operator > (const Point &other) const {
            return (y > other.y);   // returns true if y>
        }
        bool operator < (const Point &other) const {
            return (y < other.y);   // returns true if y<
        }

        int x = 0;
        int y = 0;
    };

}

#endif //EDITOR_POINT_H
