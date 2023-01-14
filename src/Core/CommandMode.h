//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_COMMANDMODE_H
#define EDITOR_COMMANDMODE_H

class NCursesScreen;

#include "Core/ModeBase.h"

class CommandMode : public ModeBase {
public:
    CommandMode();
    virtual ~CommandMode() = default;
    void Update(NCursesScreen &screen) override;
    void DrawLines(NCursesScreen &screen) override;
    const std::vector<Line *> &Lines() const override { return historyBuffer; }
protected:
    void NewLine();
private:
    Line *currentLine = nullptr;
    std::vector<Line *> historyBuffer;
};

#endif //EDITOR_COMMANDMODE_H
