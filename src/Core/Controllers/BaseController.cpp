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
bool BaseController::DefaultEditLine(Cursor &cursor, Line *line, const KeyPress &keyPress) {
    if (keyPress.IsHumanReadable()) {
        AddCharToLine(cursor, line, keyPress.key);
        return true;
    }
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
                }
                break;
        }
    }
    return wasHandled;
}

void BaseController::AddCharToLine(Cursor &cursor, Line *line, int ch) {
    line->Insert(cursor.position.x, ch);
    cursor.position.x++;
    cursor.wantedColumn = cursor.position.x;
}
void BaseController::RemoveCharFromLine(gedit::Cursor &cursor, gedit::Line *line) {
    if (cursor.position.x > 0) {
        line->Delete(cursor.position.x-1);
        cursor.position.x--;
    }

}


