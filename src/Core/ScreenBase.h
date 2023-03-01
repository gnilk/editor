//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_SCREENBASE_H
#define EDITOR_SCREENBASE_H

#include <utility>
#include <vector>

#include "Core/Line.h"
#include "Core/ColorRGBA.h"
#include "Core/Cursor.h"
#include "Core/NativeWindow.h"
#include "Rect.h"

class ScreenBase {
public:
    ScreenBase() = default;
    virtual ~ScreenBase() = default;
    virtual bool Open() { return false; }
    virtual void Close() { }
    virtual void Clear() { }
    virtual void Update() { }
    virtual void BeginRefreshCycle() {}
    virtual void EndRefreshCycle() {}

    virtual void RegisterColor(int appIndex, const ColorRGBA &foreground, const ColorRGBA &background) {}

    void SetCursorColumn(int cCol) { cursorColumn = cCol; }

    void SetCursor(const Cursor &newCursor) {
        cursor = newCursor;
    }
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
    // Note: This is very specific for the editor-view
    // FIXME: Remove this
    virtual void DrawGutter(int idxStart) {}
    virtual void DrawLines(const std::vector<Line *> &lines, int idxActiveLine) {}
    virtual void DrawLineAt(int row, const std::string &prefix, const Line *line) {}

    virtual void DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l) {}

    virtual void DrawBottomBar(const char *str) {}
    virtual void DrawTopBar(const char *str) {}

    // TODO: Recactor this to take a Rect instead!!
//    virtual std::pair<int, int> Dimensions() { return std::make_pair(0,0); }
    virtual gedit::Rect Dimensions() { return {}; }

    // FIXME: Remove this
    virtual void Scroll(int nLines) {}

    // RAW Drawing routines
    virtual void DrawCharAt(int x, int y, const char ch) {}
    virtual void DrawStringAt(int x, int y, const char *str) {}
    virtual void DrawStringAt(int x, int y, int nCharToPrint, const char *str) {}
    // TODO: Fix drawing flags for rect in screen
    virtual void DrawRect(const gedit::Rect &rect) {}
    virtual void DrawVLine(const gedit::Point &ptStart, const gedit::Point &ptEnd) {}
    virtual void DrawHLine(const gedit::Point &ptStart, const gedit::Point &ptEnd) {}

    virtual gedit::NativeWindow *CreateWindow(const gedit::Rect &rect) { return nullptr; }

    virtual void SetClipRect(const gedit::Rect &newClipRect) {
        clipRect = newClipRect;
    }

    void InvalidateAll() { invalidateAll = true; }
protected:
    bool sizeChanged = false;
    bool invalidateAll = false;
    int cursorColumn = 0;   // Replace
    Cursor cursor;
    int szGutter = 0;

    gedit::Rect clipRect = {};


};

#endif //EDITOR_SCREENBASE_H
