//
// Created by gnilk on 15.02.23.
//

#include "logger.h"

#include "BaseController.h"


using namespace gedit;

bool BaseController::DefaultEditLine(Cursor &cursor, Line *line, const KeyPress &keyPress) {
    if (keyPress.IsHumanReadable()) {
        line->Insert(cursor.position.x, keyPress.key);
        cursor.position.x++;
        cursor.wantedColumn = cursor.position.x;
        return true;
    }
    bool wasHandled = false;
    if (keyPress.isKeyValid) {
        auto logger = gnilk::Logger::GetLogger("BaseController");
        logger->Debug("DefaultEditLine, keyPress, key=%d",keyPress.key);
        if (keyPress.isHwEventValid) {
            logger->Debug("  HWEvent, scancode=%d, keyCode=%d", keyPress.hwEvent.scanCode, (int)keyPress.hwEvent.keyCode);


            switch(keyPress.hwEvent.keyCode) {
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
                    line->Delete(cursor.position.x);
                    wasHandled = true;
                    break;
                case Keyboard::kKeyCode_Backspace :
                    if (cursor.position.x > 0) {
                        line->Delete(cursor.position.x-1);
                        cursor.position.x--;
                    }
                    wasHandled = true;
                    break;
            }

        }
    }
    return wasHandled;
}
