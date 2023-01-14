//
// Created by gnilk on 14.01.23.
//

// TMP
#include <ncurses.h>

#include "Core/KeyCodes.h"
#include "Core/Line.h"
#include "Core/ModeBase.h"
#include "Core/EditorConfig.h"
#include "Core/KeyboardDriverBase.h"
#include "Core/RuntimeConfig.h"

bool ModeBase::DefaultEditLine(Line *line, KeyPress &ch) {

    auto kbd = RuntimeConfig::Instance().Keyboard();

    if (kbd->IsHumanReadable(ch )) {
        line->Insert(cursor.activeColumn, ch.data.code);
        cursor.activeColumn++;
        cursor.wantedColumn = cursor.activeColumn;
        return true;
    }

    bool handled = true;

    switch(ch.data.code) {
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
        case kKey_Left :
            cursor.activeColumn--;
            if (cursor.activeColumn < 0) {
                cursor.activeColumn = 0;
            }
            cursor.wantedColumn = cursor.activeColumn;
            break;
        case kKey_Right :
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
        case kKey_End :
            cursor.activeColumn = line->Length();
            cursor.wantedColumn = cursor.activeColumn;
            break;
        case kKey_Home :
            cursor.activeColumn = 0;
            cursor.wantedColumn = cursor.activeColumn;
            break;
        default :
            handled = false;
            break;
    }
    return false;
}
