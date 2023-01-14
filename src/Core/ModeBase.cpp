//
// Created by gnilk on 14.01.23.
//

// TMP
#include <ncurses.h>

#include "Core/KeyCodes.h"
#include "Core/Line.h"
#include "Core/ModeBase.h"
#include "Core/EditorConfig.h"

bool ModeBase::DefaultEditLine(Line *line, int ch) {

    if ((ch > 31) && (ch < 127)) {
        line->Insert(cursor.activeColumn, ch);
        cursor.activeColumn++;
        cursor.wantedColumn = cursor.activeColumn;
        return true;
    }

    bool handled = true;

    switch(ch) {
        case kKey_ShiftTab :
        {
            auto nChars = line->Unindent();
            cursor.activeColumn -= nChars;
            if (cursor.activeColumn < 0) {
                cursor.activeColumn = 0;
            }
            cursor.wantedColumn = cursor.activeColumn;
        }
            break;
        case kKey_Tab :
            line->Insert(cursor.activeColumn, EditorConfig::Instance().tabSize,' ');
            cursor.activeColumn += EditorConfig::Instance().tabSize;
            cursor.wantedColumn = cursor.activeColumn;
            break;
        case KEY_LEFT :
            cursor.activeColumn--;
            if (cursor.activeColumn < 0) {
                cursor.activeColumn = 0;
            }
            cursor.wantedColumn = cursor.activeColumn;
            break;
        case KEY_RIGHT :
            cursor.activeColumn++;
            if (cursor.activeColumn > line->Length()) {
                cursor.activeColumn = line->Length();
            }
            cursor.wantedColumn = cursor.activeColumn;
            break;
        case kKey_Escape :
            if (onExitMode != nullptr) {
                onExitMode();
            }
            break;
        case KEY_BACKSPACE :
            [[fallthrough]];

        case kKey_Backspace :
            if (cursor.activeColumn > 0) {
                cursor.activeColumn--;
                line->Delete(cursor.activeColumn);
            }
            cursor.wantedColumn = cursor.activeColumn;
            break;
        case kKey_Delete :
            line->Delete(cursor.activeColumn);
            break;
        case KEY_END :
            cursor.activeColumn = line->Length();
            cursor.wantedColumn = cursor.activeColumn;
            break;
        case KEY_HOME :
            cursor.activeColumn = 0;
            cursor.wantedColumn = cursor.activeColumn;
            break;
        default :
            handled = false;
            break;
    }
    return false;
}
