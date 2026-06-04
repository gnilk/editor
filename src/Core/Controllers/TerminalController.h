//
// Created by gnilk on 22.02.2024.
//

#ifndef GOATEDIT_TERMINALCONTROLLER_H
#define GOATEDIT_TERMINALCONTROLLER_H

#include <vector>
#include <string>
#include <mutex>

#include "logger.h"
#include "BaseController.h"
#include "Core/RuntimeConfig.h"
#include "Core/unix/Shell.h"
#include "Core/TerminalScreen.h"
#include "Core/VTermParser.h"

namespace gedit {
    class TerminalController : public BaseController, IOutputConsole {
    public:
        TerminalController() = default;
        virtual ~TerminalController() = default;

        void Begin() override;
        void Resize(int cols, int rows);
        bool HandleKeyPress(Cursor &cursor, size_t &idxActiveLine, const KeyPress &keyPress) override;

        const TerminalScreen &GetScreen() const { return screen; }
        std::mutex &GetScreenLock() { return screenLock; }

        Line::Ref GetInputLine() const { return inputLine; }
        void CommitLine();
        int GetCursorXPos();

        bool OnAction(const KeyPressAction &kpAction);
        void WriteLine(const std::u32string &str) override;

    protected:
        void HandleTerminalData(const uint8_t *buffer, size_t length);
        void ApplyCommand(const VTermParser::CMD &cmd);
        void InitializeColorTable();

    private:
        Shell shell;
        gnilk::ILogger *logger = nullptr;

        Line::Ref inputLine = nullptr;
        Cursor inputCursor;

        TerminalScreen screen;
        std::mutex screenLock;
    };
}

#endif //GOATEDIT_TERMINALCONTROLLER_H
