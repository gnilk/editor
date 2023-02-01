//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_SCREENBASE_H
#define EDITOR_SCREENBASE_H

#include <utility>
#include <vector>

#include "Core/Line.h"
#include "Core/ColorRGBA.h"

class ScreenBase {
public:
    ScreenBase() = default;
    virtual ~ScreenBase() = default;
    virtual bool Open() { return false; }
    virtual void Close() { }
    virtual void Clear() { }
    virtual void Update() { }

    virtual void RegisterColor(int appIndex, const ColorRGBA &foreground, const ColorRGBA &background) {}

    void SetCursorColumn(int cCol) { cursorColumn = cCol; }
    void SetExtScreenSizeNotificationFlag() {
        sizeChanged = true;
    }
    bool IsSizeChanged(bool bResetFlag) {
        auto result = sizeChanged;
        if (bResetFlag) {
            sizeChanged = false;
        }
        return result;
    }

    virtual void NoGutter() { szGutter = 0; }
    virtual void DrawGutter(int idxStart) {}
    virtual void DrawLines(const std::vector<Line *> &lines, int idxActiveLine) {}
    virtual void DrawLineAt(int row, const Line *line) {}
    virtual void DrawBottomBar(const char *str) {}
    virtual void DrawTopBar(const char *str) {}
    virtual std::pair<int, int> Dimensions() { return std::make_pair(0,0); }
    virtual void Scroll(int nLines) {}


    void InvalidateAll() { invalidateAll = true; }
protected:
    bool sizeChanged = false;
    bool invalidateAll = false;
    int cursorColumn = 0;
    int szGutter = 0;



};

#endif //EDITOR_SCREENBASE_H
