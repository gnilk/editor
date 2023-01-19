//
// Created by gnilk on 19.01.23.
//

#ifndef EDITOR_SHELLCOMMAND_H
#define EDITOR_SHELLCOMMAND_H

#include <vector>
#include <string>
#include <functional>
#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


class ShellCommand {
public:
    using OutputDelegate = std::function<void(std::string &)>;
    public:
    int             ExitStatus = 0;
    std::string     Command;
    std::string     StdIn;
    std::string     StdOut;
    std::string     StdErr;



    void Execute(std::string &cmdString);
    void ConsumePipes();
    void CleanUp();
    bool IsDone() { return done; }
    void SetStdoutDelegate(OutputDelegate handler) {
        onStdout = handler;
    }

private:
    const int READ_END = 0;
    const int WRITE_END = 1;
    OutputDelegate onStdout = nullptr;
    bool done = false;
    pid_t pid;
    int infd[2] = {0, 0};
    int outfd[2] = {0, 0};
    int errfd[2] = {0, 0};
};



#endif //EDITOR_SHELLCOMMAND_H
