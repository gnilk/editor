//
// Created by gnilk on 15.02.23.
//

#ifndef EDITOR_BASECONTROLLER_H
#define EDITOR_BASECONTROLLER_H

#include "Core/Line.h"
#include "Core/Graphics/Cursor.h"
#include "Core/KeyPress.h"
//#include "Core/Graphics/NCurses/NCursesKeyboardDriver.h"

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

        // Visual tab width used when capturing the wanted (visual) column. Defaults to 1, which makes
        // visual column == character index (a no-op) for tab-agnostic input views (terminal/command).
        // The editor sets this from the active language so vertical navigation tracks the visual column.
        void SetEditTabSize(int newTabSize) { editTabSize = newTabSize; }
    protected:
        int editTabSize = 1;
    private:

    };
}


#endif //EDITOR_BASECONTROLLER_H
