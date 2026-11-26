//
// Created by gnilk on 19.01.23.
//

// PtySession for Linux and macOS: forkpty, a reader thread on the pty master (docs/windows-port.md §2.1).
//

#ifndef EDITOR_UNIXPTY_H
#define EDITOR_UNIXPTY_H

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

#include "Core/PtySession.h"

namespace gedit {
    class UnixPty : public PtySession {
    public:
        UnixPty() = default;
        // Closes: the reader thread is joined, so it can't outlive the object
        ~UnixPty() override;

        bool Begin(const std::string &shell, const std::string &args,
                   const std::vector<std::string> &initScript) override;

        void SetWindowSize(int cols, int rows) override;

        int  SendCmd(std::u32string &cmd) override;
        int  Write(uint8_t chr) override;
        int  WriteBytes(const std::string &data) override;
        // tcgetpgrp != the shell's process group: an interactive shell gives each command its own group
        bool IsForegroundJobRunning() const override;
        std::string GetWorkingDirectory() const override;

        // A shell that ignores the hangup is killed after a grace period, so this is bounded
        void Close() override;

    private:
        void CleanUp();
        void ConsumePty();
        // Hang up (the foreground job's process group too), wait, then SIGKILL - bounded: a child that can't be reaped is
        // left behind, never waited for (Close() runs on the UI thread)
        void ReapChild();
        // The shell's pid when it was reaped within 'timeoutMs', 0 when it is still there, -1 on an error
        pid_t WaitForChild(int &status, int timeoutMs);
        // In the forked child: every fd but stdin/out/err closed - the editor's files, other tabs' pty masters
        static void CloseInheritedFds();
        void WakeReader();
        bool StartShellProc(const std::string &shell, const std::string &shellInitStr);
        void SendInitScript(const std::vector<std::string> &initScript);
        std::string ResolveShellBinary(const std::string &configured);
        void ChangeState(State newState) {
            logger->Debug("ChangeState: %s -> %s",
                          StateName(GetState()).c_str(), StateName(newState).c_str());
            SetState(newState);
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

        static constexpr long MAX_FD_TO_CLOSE = 65536;

        int      amaster   = -1;   // pty master — single fd for all shell I/O
        pid_t    pid       = -1;
        int      exitStatus = 0;
        std::thread reader;
        int      wakeFds[2] = { -1, -1 };   // Close() writes to [1] so the reader's poll() returns at once
    };
}

#endif //EDITOR_UNIXPTY_H
