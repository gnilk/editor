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

        // All shell output (stdout + stderr) arrives through one callback since
        // both file descriptors share the pty slave.
        using OutputDelegate = std::function<void(const uint8_t *buffer, size_t length)>;

    public:
        bool Begin(const std::string &shell, const std::string &args,
                   const std::vector<std::string> &initScript);

        // Notify the pty of the visible terminal size so programs like vi and
        // less know how many rows and columns they have to work with.
        void SetWindowSize(int cols, int rows);

        int  SendCmd(std::u32string &cmd);
        int  Write(uint8_t chr);
        int  WriteBytes(const std::string &data);

        void Close();
        void CleanUp();

        void SetOutputDelegate(OutputDelegate handler) {
            onOutput = handler;
        }

        State GetState() {
            return state;
        }

        void OnSignal(int sig);

    private:
        void ConsumePty();
        bool StartShellProc(const std::string &shell, const std::string &shellInitStr);
        void SendInitScript(const std::vector<std::string> &initScript);
        void ChangeState(State newState) {
            logger->Debug("ChangeState: %s -> %s",
                          StateName(state).c_str(), StateName(newState).c_str());
            state = newState;
        }

    private:
        const std::string &StateName(State s) const {
            static std::string names[] = {"kIdle","kRunning","kTerminating","kTerminated","kUnknown"};
            switch (s) {
                case State::kIdle:       return names[0];
                case State::kRunning:    return names[1];
                case State::kTerminate:  return names[2];
                case State::kTerminated: return names[3];
            }
            return names[4];
        }

        gnilk::Logger::ILogger *logger = nullptr;

        int      amaster   = -1;   // pty master — single fd for all shell I/O
        pid_t    pid       = -1;
        int      exitStatus = 0;
        int      extSignal  = -1;
        State    state      = State::kIdle;

        OutputDelegate onOutput = nullptr;
    };
}

#endif //EDITOR_SHELL_H
