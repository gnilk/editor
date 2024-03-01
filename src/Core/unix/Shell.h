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
#include <termios.h>

#include <logger.h>

namespace gedit {
    class Shell {
    public:
        enum class State {
            kIdle,
            kRunning,
            kTerminate,
            kTerminated,
        };
        enum class Stream {
            kStdOut,
            kStdErr,
        };
        using OutputDelegate = std::function<void(Stream stream, const uint8_t *buffer, size_t length)>;

    public:
        bool Begin(const std::string &shell, const std::string &args, const std::vector<std::string> &initScript);
        void Close();
        int SendCmd(std::u32string &cmd);
        int Write(uint8_t chr);

        void CleanUp();
        void SetStdoutDelegate(OutputDelegate handler) {
            onStdout = handler;
        }
        void SetStderrDelegate(OutputDelegate handler) {
            onStderr = handler;
        }

        State GetState() {
            return state;
        }

        void OnSignal(int sig);

    private:
        void ConsumePipes();
        bool ReadAndDispatch(Stream stream, FILE *fd, OutputDelegate onData);
        void InitializeDefaultTermiosAttr();
        bool StartShellProc(const std::string &shell, const std::string &shellInitStr);
        void SendInitScript(const std::vector<std::string> &initScript);
        void ChangeState(State newState) {
            logger->Debug("ChangeState: %s -> %s", StateName(state).c_str(), StateName(newState).c_str());
            state = newState;
        }
    private:
        const std::string &StateName(State s) const {
            static std::string stateNames[]={"kIdle", "kRunning", "kTerminating", "kTerminated", "kUnknown"};
            switch(s) {
                case State::kIdle :
                    return stateNames[0];
                case State::kRunning :
                    return stateNames[1];
                case State::kTerminate :
                    return stateNames[2];
                case State::kTerminated :
                    return stateNames[3];
            }
            return stateNames[4];
        }
        gnilk::Logger::ILogger *logger = nullptr;
        const int READ_END = 0;
        const int WRITE_END = 1;

        // Need to control this flag...
        // And perhaps control this a bit better in general (from the application level).
        bool bSetTermios = false;
        struct termios shellTermios;

        // Unused but assigned..
        int exitStatus = 0;

        State state = State::kIdle;

        OutputDelegate onStdout = nullptr;
        OutputDelegate onStderr = nullptr;
        FILE *fRawAnsi = nullptr;

        int extSignal = -1;
        pid_t pid;
        int infd[2] = {0, 0};
        int outfd[2] = {0, 0};
        int errfd[2] = {0, 0};
    };
}

#endif //EDITOR_SHELL_H
