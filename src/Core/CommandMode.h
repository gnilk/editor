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

#include "logger.h"


class CommandMode : public ModeBase, public IOutputConsole {
public:
    CommandMode();
    virtual ~CommandMode() = default;
    bool Begin() override;
    void Update() override;
    void DrawLines() override;
    const std::vector<Line *> &Lines() const override { return historyBuffer; }
    void OnSwitchMode(bool enter) override;

    void ResetVariablesFromConfig();

public: // IOutputConsole
    void WriteLine(const std::string &str) override;


protected:
    void NewLine(bool addCmdMarker = true);
    void HandleReturn();
    bool TryExecuteInternalCmd(std::string &cmdline);
    void TryExecuteShellCmd(std::string &cmdline);
    bool DoesCmdLineHavePrefix(std::string &cmdline);
private:
    typedef enum {
        kIdle,
        kExecuteShell,
    } kState;
private:
    Shell terminal;
    gnilk::ILogger *logger = nullptr;
    bool scrollOnNextUpdate = false;
    kState state = kState::kIdle;
    Line *currentLine = nullptr;
    std::vector<Line *> historyBuffer;
    std::mutex lineLock;
    std::string cmdPrompt = ">";
    char cmdletPrefix = '.';
    bool isModeActive = false;
};

#endif //EDITOR_COMMANDMODE_H
