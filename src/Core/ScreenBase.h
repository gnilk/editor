//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_SCREENBASE_H
#define EDITOR_SCREENBASE_H

#include <utility>
#include <vector>

#include "Core/Line.h"

class ScreenBase {
public:
    ScreenBase() = default;
    virtual ~ScreenBase() = default;
    virtual bool Open() { return false; }
    virtual void Close() { }
    virtual void Clear() { }
    virtual void Update() { }


    void SetCursorColumn(int cCol) { cursorColumn = cCol; }

    virtual void DrawGutter(int idxStart) {}
    virtual void DrawLines(const std::vector<Line *> &lines, int idxActiveLine) {}
    virtual void DrawStatusBar(const char *str) {}
    virtual std::pair<int, int> Dimensions() { return std::make_pair(0,0); }


    void InvalidateAll() { invalidateAll = true; }
protected:
    bool invalidateAll = false;
    int cursorColumn = 0;
    int szGutter = 0;

};

#endif //EDITOR_SCREENBASE_H
