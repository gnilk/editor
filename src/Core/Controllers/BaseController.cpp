//
// Created by gnilk on 15.02.23.
//

#include <algorithm>

#include "logger.h"
#include "BaseController.h"

using namespace gedit;

//
// Note: This is all wrong... need to update this one...
//
bool BaseController::DefaultEditLine(Cursor &cursor, Line::Ref line, const KeyPress &keyPress, bool handleSpecialKeys) {
    if (keyPress.IsHumanReadable()) {
        AddCharToLine(cursor, line, keyPress.key);
        return true;
    }
    if (!handleSpecialKeys) {
        return false;
    }
    return DefaultEditSpecial(cursor, line, keyPress);
}
// This takes care of single line editing of 'special' keys (delete, home, end, backspace)
bool BaseController::DefaultEditSpecial(Cursor &cursor, Line::Ref line, const KeyPress &keyPress) {
    bool wasHandled = false;
    // We don't handle any modifiers!!!
    if ((keyPress.isSpecialKey) && (keyPress.modifiers == 0)) {
        auto logger = gnilk::Logger::GetLogger("BaseController");
        logger->Debug("DefaultEditLine, keyPress, specialKey=%d, modifiers=%x",keyPress.specialKey, keyPress.modifiers);

        switch(keyPress.specialKey) {
            case Keyboard::kKeyCode_Home :
                cursor.position.x = 0;
                cursor.wantedColumn = cursor.position.x;
                wasHandled = true;
                break;
            case Keyboard::kKeyCode_End :
                cursor.position.x = line->Length();
                cursor.wantedColumn = line->CharToVisualColumn(cursor.position.x, editTabSize);
                wasHandled = true;
                break;
            case Keyboard::kKeyCode_DeleteForward :
                if (cursor.position.x < (int)line->Length()) {
                    line->Delete(cursor.position.x);
                    wasHandled = true;
                }
                break;
                // We ONLY handle backspace within the current line..
            case Keyboard::kKeyCode_Backspace :
                if (cursor.position.x > 0){
                    RemoveCharFromLine(cursor, line);
                    wasHandled = true;
                } else {
                    wasHandled = false;
                }
                break;
        }
    }
    return wasHandled;
}

void BaseController::AddCharToLine(Cursor &cursor, Line::Ref line, int ch) {
    // Clamp at the single insertion chokepoint: the cursor can legitimately sit past a (short/empty)
    // line's end - e.g. on an auto-indented blank line whose whitespace isn't materialised - and
    // inserting past end-of-string throws std::out_of_range. Insert there means append.
    int at = std::min(cursor.position.x, (int)line->Length());
    line->Insert(at, ch);
    cursor.position.x = at + 1;
    cursor.wantedColumn = line->CharToVisualColumn(cursor.position.x, editTabSize);
}

void BaseController::RemoveCharFromLine(gedit::Cursor &cursor, Line::Ref line) {
    if (cursor.position.x > 0) {
        line->Delete(cursor.position.x-1);
        cursor.position.x--;
        cursor.wantedColumn = line->CharToVisualColumn(cursor.position.x, editTabSize);
    }
}


