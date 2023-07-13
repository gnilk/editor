//
// Created by gnilk on 15.02.23.
//

#include "logger.h"

#include "BaseController.h"
#include "Core/EditorConfig.h"

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
                cursor.wantedColumn = cursor.position.x;
                wasHandled = true;
                break;
            case Keyboard::kKeyCode_DeleteForward :
                if (cursor.position.x < line->Length()) {
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
    line->Insert(cursor.position.x, ch);
    cursor.position.x++;
    cursor.wantedColumn = cursor.position.x;
}
void BaseController::RemoveCharFromLine(gedit::Cursor &cursor, Line::Ref line) {
    if (cursor.position.x > 0) {
        line->Delete(cursor.position.x-1);
        cursor.position.x--;
    }

}


