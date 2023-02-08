//
// Created by gnilk on 15.01.23.
//

#ifndef EDITOR_KEYPRESS_H
#define EDITOR_KEYPRESS_H

#include <cstdint>

struct KeyPress {
    union {
        int64_t editorkey;  // this is special | code
        struct {
            int32_t special;
            int32_t code;
        } data;
    };

    // This is from the underlying keyboard driver...
    int64_t rawCode;

    bool IsValid();
    bool IsShiftPressed();
    bool IsCtrlPressed();
    bool IsHumanReadable();
};

#endif //EDITOR_KEYPRESS_H
