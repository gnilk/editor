//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_NCURSESSCREEN_H
#define EDITOR_NCURSESSCREEN_H

#include <vector>
#include <utility>
#include "Core/ScreenBase.h"
#include "Core/Line.h"

#include "NCursesWindow.h"

class NCursesScreen : public ScreenBase {
public:
    NCursesScreen() = default;
    virtual ~NCursesScreen() = default;
    bool Open() override;
    void Close() override;
    void Clear() override;
    void Update() override;
    void BeginRefreshCycle() override;
    void EndRefreshCycle() override;


    void RegisterColor(int appIndex, const ColorRGBA &foreground, const ColorRGBA &background) override;

    void DrawGutter(int idxStart) override;
    void DrawBottomBar(const char *str) override;
    void DrawTopBar(const char *str) override;

    // TODO: Put these in the WinDrawContext instead...
    void DrawLines(const std::vector<Line *> &lines, int idxActiveLine) override;
    void DrawLineAt(int row, const std::string &prefix, const Line *line) override;
    // Raw drawing routines
    void DrawCharAt(int x, int y, const char ch) override;
    void DrawStringAt(int x, int y, const char *str) override;
    void DrawStringAt(int x, int y, int nCharToPrint, const char *str) override;
    void DrawRect(const gedit::Rect &rect) override;
    void DrawVLine(const gedit::Point &ptStart, const gedit::Point &ptEnd) override;
    void DrawHLine(const gedit::Point &ptStart, const gedit::Point &ptEnd) override;
    void DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l) override;


    gedit::NativeWindow *CreateWindow(const gedit::Rect &rect) override;


    //std::pair<int, int> Dimensions() override;
    gedit::Rect Dimensions() override;
    void Scroll(int nLines) override;
protected:
    void DrawLineWithAttributes(Line &l, int nCharToPrint);


    std::pair<int, int> ComputeView(int idxActiveLine);
private:
    std::pair<int, int> view = {0,0};

    int lastTopLine = 0;
    int tmp_dyLast = 0;
};

#endif //EDITOR_NCURSESSCREEN_H
