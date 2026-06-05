//
// Created by gnilk on 19.01.23.
//

#if defined(GEDIT_MACOS)
#include <util.h>
#elif defined(GEDIT_LINUX)
#include <pty.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <termios.h>
#include <sys/ioctl.h>
#include <thread>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>

#include <logger.h>

#include "Core/UnicodeHelper.h"
#include "Shell.h"

#ifndef GEDIT_DEFAULT_POLL_TMO_MS
#define GEDIT_DEFAULT_POLL_TMO_MS 100
#endif

using namespace gedit;

static Shell *glbShell = nullptr;

static volatile sig_atomic_t gSignalStatus = {};
static void signal_handler(int signal) {
    gSignalStatus = signal;
    glbShell->OnSignal(signal);
}

void Shell::OnSignal(int sig) {
    logger->Debug("Signal raised, sig=%d, pid=%d", sig, getpid());
    if (GetState() == State::kRunning) {
        extSignal = sig;
        ChangeState(State::kTerminate);
    }
}

bool Shell::Begin(const std::string &shell, const std::string &args,
                  const std::vector<std::string> &initScript) {
    logger = gnilk::Logger::GetLogger("Shell");
    glbShell = this;
    if (!StartShellProc(shell, args)) {
        return false;
    }
    SendInitScript(initScript);
    return true;
}

bool Shell::StartShellProc(const std::string &shell, const std::string &shellInitStr) {
    logger->Debug("Starting shell process: %s %s", shell.c_str(), shellInitStr.c_str());

    struct stat shellstat;
    if (stat(shell.c_str(), &shellstat)) {
        logger->Error("Can't stat shell '%s' — verify path", shell.c_str());
        return false;
    }

#if defined(GEDIT_LINUX)
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGCHLD, signal_handler);
#endif

    logger->Info("Forking PTY");
    pid = forkpty(&amaster, NULL, NULL, NULL);

    if (pid > 0) {
        // Parent — amaster is the only fd we need
        logger->Debug("Started child with pid=%d", pid);
        std::thread(&Shell::ConsumePty, this).detach();
    } else if (pid == 0) {
        // Child — pty slave is already stdin/stdout/stderr; just exec.
        // Set TERM so ncurses-based programs (less, vi, ...) find their
        // terminfo entry and know the terminal is capable.
        ::setenv("TERM", "xterm-256color", 1);
        ::execl(shell.c_str(), shell.c_str(), shellInitStr.c_str(), nullptr);
        exit(EXIT_SUCCESS);
    } else {
        logger->Error("forkpty failed, errno=%d", errno);
        return false;
    }
    return true;
}

void Shell::SetWindowSize(int cols, int rows) {
    if (amaster < 0) {
        return;
    }
    struct winsize ws = {};
    ws.ws_col = (unsigned short)cols;
    ws.ws_row = (unsigned short)rows;
    ioctl(amaster, TIOCSWINSZ, &ws);
}

void Shell::SendInitScript(const std::vector<std::string> &initScript) {
    logger->Debug("Executing shell bootstrap script");
    for (const auto &s : initScript) {
        std::u32string strCmd;
        if (!UnicodeHelper::ConvertUTF8ToUTF32String(strCmd, s)) {
            logger->Error("SendInitScript, failed to convert '%s'", s.c_str());
        }
        strCmd += U"\n";
        SendCmd(strCmd);
    }
}

int Shell::SendCmd(std::u32string &cmd) {
    std::string utf8;
    if (!UnicodeHelper::ConvertUTF32ToUTF8String(utf8, cmd)) {
        logger->Error("SendCmd, failed to convert to UTF-8");
        return -1;
    }
    return write(amaster, utf8.c_str(), utf8.size());
}

int Shell::Write(uint8_t chr) {
    return write(amaster, &chr, 1);
}

int Shell::WriteBytes(const std::string &data) {
    return write(amaster, data.c_str(), data.size());
}

void Shell::Close() {
    ChangeState(State::kTerminate);
}

void Shell::CleanUp() {
    if (amaster >= 0) {
        ::close(amaster);
        amaster = -1;
    }
}

void Shell::ConsumePty() {
    if (pid < 0) {
        CleanUp();
        return;
    }

    ChangeState(State::kRunning);

#ifdef GEDIT_LINUX
    pthread_setname_np(pthread_self(), "ShlConsume");
#else
    pthread_setname_np("ShlConsume");
#endif

    uint8_t buffer[4096];

    while (GetState() != State::kTerminate) {
        struct pollfd pfd = { amaster, POLLIN, 0 };
        int ret = poll(&pfd, 1, GEDIT_DEFAULT_POLL_TMO_MS);
        if (ret < 0) {
            if (errno == EINTR) { continue; }
            break;
        }
        if (ret == 0) { continue; }

        ssize_t n = read(amaster, buffer, sizeof(buffer));
        if (n > 0) {
            if (onOutput) {
                onOutput(buffer, (size_t)n);
            }
        } else {
            if (errno == EAGAIN) { continue; }
            // EIO means child closed the pty — normal exit
            break;
        }
    }

    if (extSignal == SIGTERM) {
        logger->Debug("SIGTERM — killing child pid=%d", pid);
        kill(pid, SIGKILL);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        exitStatus = WEXITSTATUS(status);
    }

    logger->Debug("Leaving ConsumePty for pid=%d", pid);
    CleanUp();
    ChangeState(State::kTerminated);
}
