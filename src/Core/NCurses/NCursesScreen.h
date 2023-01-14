//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_NCURSESSCREEN_H
#define EDITOR_NCURSESSCREEN_H

#include <vector>
#include <utility>
#include "Core/ScreenBase.h"
#include "Core/Line.h"

class NCursesScreen : public ScreenBase {
public:
    NCursesScreen() = default;
    virtual ~NCursesScreen() = default;
    bool Open() override;
    void Close() override;
    void Clear() override;
    void Update() override;
    // Make these virtual and part of base class
    void DrawGutter(int idxStart) override;
    void DrawLines(const std::vector<Line *> &lines, int idxActiveLine) override;
    void DrawStatusBar(const char *str) override;
    std::pair<int, int> Dimensions() override;
protected:
    std::pair<int, int> ComputeView(int idxActiveLine);
private:
    std::pair<int, int> view = {0,0};
    int lastTopLine = 0;
    int tmp_dyLast = 0;
};

#endif //EDITOR_NCURSESSCREEN_H
