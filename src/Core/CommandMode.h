//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_COMMANDMODE_H
#define EDITOR_COMMANDMODE_H

#include "Core/ScreenBase.h"
#include "Core/ModeBase.h"

class CommandMode : public ModeBase {
public:
    CommandMode();
    virtual ~CommandMode() = default;
    void Update() override;
    void DrawLines() override;
    const std::vector<Line *> &Lines() const override { return historyBuffer; }
    void OnSwitchMode(bool enter) override;

protected:
    void NewLine();
private:
    bool scrollOnNextUpdate = false;
    Line *currentLine = nullptr;
    std::vector<Line *> historyBuffer;
};

#endif //EDITOR_COMMANDMODE_H
