//
// Created by gnilk on 19.01.23.
//

#ifndef EDITOR_SHELL_H
#define EDITOR_SHELL_H

#include <vector>
#include <string>
#include <functional>
#include <atomic>
#include <thread>
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
        Shell() = default;
        // Closes: the reader thread is joined, so it can't outlive the object
        ~Shell();

        bool Begin(const std::string &shell, const std::string &args,
                   const std::vector<std::string> &initScript);

        // Notify the pty of the visible terminal size so programs like vi and
        // less know how many rows and columns they have to work with.
        void SetWindowSize(int cols, int rows);

        int  SendCmd(std::u32string &cmd);
        int  Write(uint8_t chr);
        int  WriteBytes(const std::string &data);
        // A job other than the shell is in the foreground of the pty (tcgetpgrp != the shell's process group):
        // an interactive shell gives each command its own group, so this is "a program is running"
        bool IsForegroundJobRunning() const;
        // The shell process's current directory (what a relative path in a command's output is relative to);
        // empty when it can't be read
        std::string GetWorkingDirectory() const;

        // Ends the shell and returns once the pty reader thread is gone (state kTerminated) - after it, the
        // delegate is never called again and the Shell can be destroyed. A shell that ignores the hangup is
        // killed after a grace period, so this is bounded.
        void Close();
        void CleanUp();

        void SetOutputDelegate(OutputDelegate handler) {
            onOutput = handler;
        }

        // Written by the pty thread, polled by the UI thread - must be atomic or an optimised build may
        // hoist the load out of a wait loop and spin forever.
        State GetState() const {
            return state.load();
        }

    private:
        void ConsumePty();
        void ReapChild();
        void WakeReader();
        bool StartShellProc(const std::string &shell, const std::string &shellInitStr);
        void SendInitScript(const std::vector<std::string> &initScript);
        std::string ResolveShellBinary(const std::string &configured);
        void ChangeState(State newState) {
            logger->Debug("ChangeState: %s -> %s",
                          StateName(state.load()).c_str(), StateName(newState).c_str());
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
        std::atomic<State> state { State::kIdle };
        std::thread reader;
        int      wakeFds[2] = { -1, -1 };   // Close() writes to [1] so the reader's poll() returns at once

        OutputDelegate onOutput = nullptr;
    };
}

#endif //EDITOR_SHELL_H
