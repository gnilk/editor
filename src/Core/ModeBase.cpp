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

bool ModeBase::DefaultEditLine(Line *line, KeyPress &keyPress) {

    if (keyPress.IsHumanReadable()) {
        line->Insert(cursor.activeColumn - columnOffset, keyPress.data.code);
        cursor.activeColumn++;
        cursor.wantedColumn = cursor.activeColumn;
        return true;
    }

    bool handled = true;

    switch(keyPress.data.code) {
        case kKey_ShiftTab :
        {
            auto nChars = line->Unindent();
            cursor.activeColumn -= nChars;
            if (cursor.activeColumn < columnOffset) {
                cursor.activeColumn = columnOffset;
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
            if (cursor.activeColumn < columnOffset) {
                cursor.activeColumn = columnOffset;
            }
            cursor.wantedColumn = cursor.activeColumn;
            break;
        case kKey_Right :
            if (keyPress.IsCtrlPressed()) {
                cursor.activeColumn+=4;
            } else {
                cursor.activeColumn++;
            }
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
            if (cursor.activeColumn > columnOffset) {
                cursor.activeColumn--;
                line->Delete(cursor.activeColumn - columnOffset);
            }
            cursor.wantedColumn = cursor.activeColumn;
            break;
        case kKey_Delete :
            line->Delete(cursor.activeColumn - columnOffset);
            break;
        case kKey_End :
            cursor.activeColumn = line->Length();
            cursor.wantedColumn = cursor.activeColumn;
            break;
        case kKey_Home :
            cursor.activeColumn = columnOffset;
            cursor.wantedColumn = cursor.activeColumn;
            break;
        default :
            handled = false;
            break;
    }
    return handled;
}
