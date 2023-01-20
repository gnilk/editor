//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_COMMANDMODE_H
#define EDITOR_COMMANDMODE_H

#include <vector>
#include <string>
#include <mutex>

#include "Core/ScreenBase.h"
#include "Core/ModeBase.h"
#include "Core/unix/Shell.h"


class CommandMode : public ModeBase {
public:
    CommandMode();
    virtual ~CommandMode() = default;
    bool Begin() override;
    void Update() override;
    void DrawLines() override;
    const std::vector<Line *> &Lines() const override { return historyBuffer; }
    void OnSwitchMode(bool enter) override;

    static void TestExecuteShellCmd();
protected:
    void NewLine(bool addCmdMarker = true);
    bool ExecuteShellCmd(std::string &cmd);
private:
    typedef enum {
        kIdle,
        kExecuteShell,
    } kState;
private:
    Shell terminal;
    bool scrollOnNextUpdate = false;
    kState state = kState::kIdle;
    Line *currentLine = nullptr;
    std::vector<Line *> historyBuffer;
    FILE *log = nullptr;
    std::mutex lineLock;
    bool isModeActive = false;
};

#endif //EDITOR_COMMANDMODE_H
