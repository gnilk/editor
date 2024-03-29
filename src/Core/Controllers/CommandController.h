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
#include "Core/Language/LanguageSupport/MakeBuildLang.h"

namespace gedit {
    class CommandController : public BaseController {
    public:
        using NewLineDelegate = std::function<void()>;
    public:
        CommandController() = default;
        virtual ~CommandController() = default;

        void Begin() override;
        bool HandleKeyPress(Cursor &cursor, size_t &idxActiveLine, const KeyPress &keyPress) override;

        const std::vector<Line::Ref> &Lines() const {
            return historyBuffer;
        }
        const Line::Ref CurrentLine() const {
            return currentLine;
        }

        void SetNewLineNotificationHandler(NewLineDelegate newOnNewLine) {
            onNewLine = newOnNewLine;
        }
        void CommitLine();


        // This is from IOutputConsole - which is/was used by the expermintal API
        void WriteLine(const std::u32string &str);


    protected:
        void NewLine();
        void TryExecuteShellCmd(std::u32string &cmdline);


        void TestShowDialog();
    private:
        LanguageBase::Ref makeParser = nullptr;

        Shell terminal;
        std::mutex lineLock;

        NewLineDelegate onNewLine = nullptr;

        gnilk::ILogger *logger = nullptr;
        Line::Ref currentLine = nullptr;

        std::vector<Line::Ref> historyBuffer;
    };
}


#endif //EDITOR_COMMANDCONTROLLER_H
