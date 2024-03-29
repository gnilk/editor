//
// Created by gnilk on 15.02.23.
//

#ifndef EDITOR_BASECONTROLLER_H
#define EDITOR_BASECONTROLLER_H

#include "Core/Line.h"
#include "Core/Cursor.h"
#include "Core/NCurses/NCursesKeyboardDriver.h"

namespace gedit {
    class BaseController {
    public:
        BaseController() = default;
        virtual ~BaseController() = default;

        virtual void Begin() {}

        // This implements some default behavior for editing on a single line
        bool DefaultEditLine(Cursor &cursor, Line::Ref line, const KeyPress &keyPress, bool handleSpecialKeys = true);
        bool DefaultEditSpecial(Cursor &cursor, Line::Ref line, const KeyPress &keyPress);

        // Return true if keypress was handled, false otherwise
        virtual bool HandleKeyPress(Cursor &cursor, size_t &idxActiveLine, const KeyPress &keyPress) {
            return false;
        }

        virtual void AddCharToLine(Cursor &cursor, Line::Ref line, int ch);
        virtual void RemoveCharFromLine(Cursor &cursor, Line::Ref line);
    private:

    };
}


#endif //EDITOR_BASECONTROLLER_H
