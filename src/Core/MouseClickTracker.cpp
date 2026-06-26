//
// Created by gnilk on 27.06.26.
//

#include "Core/MouseClickTracker.h"

using namespace gedit;

int MouseClickTracker::RegisterPress(TimePoint now, int x, int y, int button) {
    // Only the left button participates in double-click detection; everything else is always a single
    // click and never primes the tracker (so a right-then-left pair can't be promoted).
    if (button != kLeftButton) {
        hasLastPress = false;
        return 1;
    }

    bool isDouble = hasLastPress &&
                    (button == lastButton) &&
                    (x == lastX) && (y == lastY) &&
                    ((now - lastPress) <= threshold);

    if (isDouble) {
        // Reset after a double so a third quick click counts as 1 again (1,2,1,2…, never 2,2,2).
        hasLastPress = false;
        return 2;
    }

    // First click of a potential pair — remember it as the anchor for the next press.
    hasLastPress = true;
    lastPress = now;
    lastX = x;
    lastY = y;
    lastButton = button;
    return 1;
}
