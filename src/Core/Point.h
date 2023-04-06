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

        bool operator > (const Point &other) const {
            if (y != other.y)
                return (y > other.y);

            return (x > other. x);
        }

        bool operator < (const Point &other) const {
            if (y != other.y)
                return (y < other.y);

            return (x < other. x);
        }

        int x = 0;
        int y = 0;
    };

}

#endif //EDITOR_POINT_H
