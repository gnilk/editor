//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_KEYBOARDDRIVERBASE_H
#define EDITOR_KEYBOARDDRIVERBASE_H

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
};


class KeyboardDriverBase {
public:
    virtual bool Initialize() { return false; };
    virtual KeyPress GetCh();
    static bool IsValid(KeyPress &key);
    static bool IsShift(KeyPress &key);
    static bool IsHumanReadable(KeyPress &key);
protected:

};

#endif //EDITOR_KEYBOARDDRIVERBASE_H
