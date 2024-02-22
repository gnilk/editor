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

namespace gedit {
    class TerminalController : public BaseController, IOutputConsole {
    public:
        TerminalController() = default;
        virtual ~TerminalController() = default;

        void Begin() override;
        bool HandleKeyPress(Cursor &cursor, size_t &idxActiveLine, const KeyPress &keyPress) override;

        const std::vector<Line::Ref> &Lines() const {
            return historyBuffer;
        }
        Line::Ref CurrentLine();
        void CommitLine();

        void WriteLine(const std::u32string &str) override;
    protected:
        void NewLine();
        void ParseAndAppend(std::u32string &str);

    private:
        Shell shell;
        gnilk::ILogger *logger = nullptr;


        Line::Ref lastLine = nullptr;
        Line::Ref inputLine = nullptr;
        Cursor inputCursor;

        std::mutex lineLock;
        std::vector<Line::Ref> historyBuffer;


    };
}


#endif //GOATEDIT_TERMINALCONTROLLER_H
