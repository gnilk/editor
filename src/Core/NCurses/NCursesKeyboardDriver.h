//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_NCURSESKEYBOARDDRIVER_H
#define EDITOR_NCURSESKEYBOARDDRIVER_H

#include "Core/KeyboardDriverBase.h"
#include "Core/macOS/MacOSKeyboardMonitor.h"

class NCursesKeyboardDriver : public KeyboardDriverBase {
public:
    bool Initialize() override;
    KeyPress GetCh() override;
protected:
    KeyPress Translate(int ch);
private:
    MacOSKeyboardMonitor kbdMonitor;
};

#endif //EDITOR_NCURSESKEYBOARDDRIVER_H
