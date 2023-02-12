//
// Created by gnilk on 12.02.23.
//

#ifndef EDITOR_VIEWBASE_H
#define EDITOR_VIEWBASE_H
// Move to own file
struct Point {
    Point() = default;

    void Move(int xDelta, int yDelta) {
        x += xDelta;
        y += yDelta;
    }

    int x = 0;
    int y = 0;
};
// move to own file
struct Rect {
    Rect(const Point &topLeft, const Point  &bottomRight) :
        p1(topLeft), p2(bottomRight) {
        // TODO: Sort
    }
    Rect(const Point &topLeft, int width, int height) {
        p1 = topLeft;
        p2 = topLeft;
        p2.x += width;
        p2.y += height;
    }
    int Height() {
        return p2.y - p1.y;
    }
    int Width() {
        return p2.x - p1.x;
    }
    const Point &TopLeft() {
        return p1;
    }
    const Point &BottomRight() {
        return p2;
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
    // We don't want users to fiddle with this on their onw...
private:
    Point p1;
    Point p2;
};

// Make views contain views - this will give us a nice docking feature layout..
// if you want 'floating' we just de-couple a view from the parent...
// Might need something to handle the layout and reposition the views
class ViewBase {
public:
    ViewBase() = default;
    ~ViewBase() = default;


private:
    Rect rect;
};

#endif //EDITOR_VIEWBASE_H
