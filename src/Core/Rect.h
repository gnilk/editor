//
// Created by gnilk on 13.02.23.
//

#ifndef EDITOR_RECT_H
#define EDITOR_RECT_H

#include "Point.h"

namespace gedit {
// move to own file
    struct Rect {
        Rect() {
            p1.x = 0;
            p1.y = 0;
            p2.x = 0;
            p2.y = 0;
        }
        Rect(const Point &topLeft, const Point &bottomRight) :
                p1(topLeft), p2(bottomRight) {
            // TODO: Sort
        }
        Rect(const Point &topLeft, int width, int height) {
            p1 = topLeft;
            p2 = topLeft;
            p2.x += width;
            p2.y += height;
        }
        Rect(const int width, const int height) {
            p1.x = 0;
            p1.y = 0;
            p2.x = width;
            p2.y = height;
        }

        bool IsEmpty() {
            if ((Width() == 0) && (Height() == 0)) {
                return true;
            }
            return false;
        }

        int Height() const {
            return p2.y - p1.y;
        }
        int Width() const {
            return p2.x - p1.x;
        }
        void SetWidth(int width) {
            p2.x = p1.x + width;
        }
        void SetHeight(int height) {
            p2.y = p1.y + height;
        }
        const Point &TopLeft() const {
            return p1;
        }
        const Point &BottomRight() const {
            return p2;
        }
        bool PointInRect(int x, int y) const {
            Point pt = {x,y};
            return PointInRect(pt);
        }
        bool PointInRect(const Point &pt) const {
            if (pt.x < p1.x) return false;
            if (pt.y < p1.y) return false;
            if (pt.x > p2.x) return false;
            if (pt.y > p2.y) return false;
            return true;
        }
        void Move(int xDelta, int yDelta) {
            p1.Move(xDelta, yDelta);
            p2.Move(xDelta, yDelta);
        }
        void MoveTo(const Point &pOrigin) {
            int w = Width();
            int h = Height();
            p1 = pOrigin;
            p2.x = p1.x + w;
            p2.y = p2.y + h;
        }

        void Deflate(int dx, int dy) {
            p1.x += dx;
            p2.x -= dx;
            p1.y += dy;
            p2.y -= dy;
        }
        // We don't want users to fiddle with this on their onw...
    private:
        Point p1 = {};
        Point p2 = {};
    };

}

#endif //EDITOR_RECT_H
