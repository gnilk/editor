//
// Created by gnilk on 22.06.26.
//

#ifndef EDITOR_MOUSEEVENT_H
#define EDITOR_MOUSEEVENT_H

#include <cstdint>

namespace gedit {
    struct MouseEvent {
        // The buttons as the backends number them (SDL's numbering; Gansi maps onto it)
        static constexpr int kButtonLeft = 1;
        static constexpr int kButtonMiddle = 2;
        static constexpr int kButtonRight = 3;

        typedef enum : uint8_t {
            kMouseEventKind_Press,
            kMouseEventKind_Release,
            kMouseEventKind_Wheel,
            kMouseEventKind_Move,       // with a button held: a drag
            kMouseEventKind_Hover,      // no button held, into another cell - only the pointer's shape follows it
        } kMouseEventKind;

        kMouseEventKind kind = kMouseEventKind_Press;
        int x = 0;              // column, absolute screen coordinate (row/col, like view rects)
        int y = 0;              // row, absolute screen coordinate
        // Where in the cell [x, y] the pointer is, [0, 1) from its left/top edge - for what draws finer than a cell (the
        // minimap, docs/usability-round.md §7). A backend that only knows cells leaves the cell's corner.
        float subCellX = 0.0f;
        float subCellY = 0.0f;
        int button = 0;         // valid for Press/Release; raw backend button index
        int wheelDelta = 0;     // valid for Wheel; positive/negative notch count
        int clicks = 1;         // valid for Press; 1 = single, 2 = double. Default for release/wheel/move.
        uint8_t modifiers = 0;  // Keyboard::kModifierKeys mask, same as KeyPress::modifiers
    };
}

#endif //EDITOR_MOUSEEVENT_H
