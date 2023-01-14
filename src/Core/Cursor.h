//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_CURSOR_H
#define EDITOR_CURSOR_H

struct Cursor {
    int activeColumn = 0;   // This is the actual position on the line... (can be truncated from wanted)
    int wantedColumn = 0;   // This is the virtual position (i.e. last-good)
};


#endif //EDITOR_CURSOR_H
