//
// Created by gnilk on 15.02.23.
//

#include "BaseController.h"

using namespace gedit;

bool BaseController::DefaultEditLine(Cursor &cursor, Line *line, const KeyPress &keyPress) {
    if (keyPress.IsHumanReadable()) {
        line->Insert(cursor.position.x, keyPress.key);
        cursor.position.x++;
        cursor.wantedColumn = cursor.position.x;
        return true;
    }
    return false;
}
