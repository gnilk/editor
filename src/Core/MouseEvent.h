//
// Created by gnilk on 22.06.26.
//

#ifndef EDITOR_MOUSEEVENT_H
#define EDITOR_MOUSEEVENT_H

#include <cstdint>

namespace gedit {
    struct MouseEvent {
        typedef enum : uint8_t {
            kMouseEventKind_Press,
            kMouseEventKind_Release,
            kMouseEventKind_Wheel,
            kMouseEventKind_Move,
        } kMouseEventKind;

        kMouseEventKind kind = kMouseEventKind_Press;
        int x = 0;              // column, absolute screen coordinate (row/col, like view rects)
        int y = 0;              // row, absolute screen coordinate
        int button = 0;         // valid for Press/Release; raw backend button index
        int wheelDelta = 0;     // valid for Wheel; positive/negative notch count
        int clicks = 1;         // valid for Press; 1 = single, 2 = double. Default for release/wheel/move.
        uint8_t modifiers = 0;  // Keyboard::kModifierKeys mask, same as KeyPress::modifiers
    };
}

#endif //EDITOR_MOUSEEVENT_H
