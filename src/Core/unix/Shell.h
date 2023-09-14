//
// Created by gnilk on 19.01.23.
//

#ifndef EDITOR_SHELL_H
#define EDITOR_SHELL_H


#include <vector>
#include <string>
#include <functional>
#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <logger.h>

namespace gedit {
    class Shell {
    public:
        using OutputDelegate = std::function<void(std::u32string &)>;
    public:
        bool Begin();
        void Close();
        int SendCmd(std::u32string &cmd);

        void ConsumePipes();
        void CleanUp();
        void SetStdoutDelegate(OutputDelegate handler) {
            onStdout = handler;
        }
    private:
        void ReadAndDispatch(FILE *fd, OutputDelegate onData);
        bool StartShellProc();
        void SendInitScript();
    private:
        gnilk::Logger::ILogger *logger = nullptr;
        const int READ_END = 0;
        const int WRITE_END = 1;

        int exitStatus = 0;

        OutputDelegate onStdout = nullptr;
        pid_t pid;
        int infd[2] = {0, 0};
        int outfd[2] = {0, 0};
        int errfd[2] = {0, 0};
    };
}

#endif //EDITOR_SHELL_H
