//
// Created by gnilk on 15.02.23.
//

#ifndef EDITOR_COMMANDCONTROLLER_H
#define EDITOR_COMMANDCONTROLLER_H

#include <vector>
#include <string>
#include <mutex>

#include "logger.h"
#include "BaseController.h"
#include "Core/unix/Shell.h"

namespace gedit {
    class CommandController : public BaseController {
    public:
        using NewLineDelegate = std::function<void()>;
    public:
        CommandController() = default;
        virtual ~CommandController() = default;

        void Begin() override;
        bool HandleKeyPress(Cursor &cursor, size_t idxActiveLine, const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) override;

        const std::vector<Line *> &Lines() const {
            return historyBuffer;
        }
        const Line *CurrentLine() const {
            return currentLine;
        }

        void SetNewLineNotificationHandler(NewLineDelegate newOnNewLine) {
            onNewLine = newOnNewLine;
        }


        // This is from IOutputConsole - which is/was used by the expermintal API
        void WriteLine(const std::string &str);

    protected:
        void NewLine();
        void HandleReturn();
        bool TryExecuteInternalCmd(std::string &cmdline);
        void TryExecuteShellCmd(std::string &cmdline);

    private:
        Shell terminal;
        std::mutex lineLock;

        NewLineDelegate onNewLine = nullptr;

        gnilk::ILogger *logger = nullptr;
        Line *currentLine = nullptr;

        std::vector<Line *> historyBuffer;
    };
}


#endif //EDITOR_COMMANDCONTROLLER_H
