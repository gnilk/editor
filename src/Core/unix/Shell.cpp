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

bool Shell::Begin(const std::string &shell, const std::string &args,
                  const std::vector<std::string> &initScript) {
    logger = gnilk::Logger::GetLogger("Shell");
    if (!StartShellProc(shell, args)) {
        return false;
    }
    SendInitScript(initScript);
    return true;
}

bool Shell::StartShellProc(const std::string &configuredShell, const std::string &shellInitStr) {
    auto shell = ResolveShellBinary(configuredShell);
    if (shell.empty()) {
        logger->Error("No usable shell found (configured '%s', $SHELL, /bin/sh)", configuredShell.c_str());
        return false;
    }
    logger->Debug("Starting shell process: %s %s", shell.c_str(), shellInitStr.c_str());

    // No signal handlers here. The shell's end is the pty read failing (EIO) in ConsumePty. A SIGCHLD handler
    // fires for ANY child of the editor (git status, ...) and stopped the reader; SIGINT/SIGTERM are SDL's
    // (it turns them into a quit) - installing ours replaced that, on Linux only.
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

// The configured shell if it is executable, otherwise the user's login shell ($SHELL), otherwise /bin/sh.
// An empty configured value means "use $SHELL". Returns empty when none of them is usable.
std::string Shell::ResolveShellBinary(const std::string &configured) {
    auto isExecutable = [](const std::string &path) {
        return !path.empty() && (::access(path.c_str(), X_OK) == 0);
    };

    if (isExecutable(configured)) {
        return configured;
    }
    if (!configured.empty()) {
        logger->Warning("Configured shell '%s' is not executable, falling back", configured.c_str());
    }

    const char *envShell = ::getenv("SHELL");
    if ((envShell != nullptr) && isExecutable(envShell)) {
        return envShell;
    }
    if (isExecutable("/bin/sh")) {
        return "/bin/sh";
    }
    return {};
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

bool Shell::IsForegroundJobRunning() const {
    if ((amaster < 0) || (pid <= 0)) {
        return false;
    }
    pid_t foreground = tcgetpgrp(amaster);
    return (foreground > 0) && (foreground != pid);
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
            // Coalesce: drain whatever else is already waiting (non-blocking poll) into
            // the same batch so a burst — e.g. a tab-completion list followed by the
            // prompt+line redraw — becomes a single onOutput/redraw. Delivering one read
            // at a time renders the intermediate cursor positions and flickers. Capped at
            // one buffer fill so a continuously-streaming child can't starve the redraw.
            size_t total = (size_t)n;
            while (total < sizeof(buffer)) {
                struct pollfd more = { amaster, POLLIN, 0 };
                if (poll(&more, 1, 0) <= 0 || !(more.revents & POLLIN)) {
                    break;
                }
                ssize_t m = read(amaster, buffer + total, sizeof(buffer) - total);
                if (m <= 0) {
                    break;
                }
                total += (size_t)m;
            }
            if (onOutput) {
                onOutput(buffer, total);
            }
        } else {
            if (errno == EAGAIN) { continue; }
            // EIO means child closed the pty — normal exit
            break;
        }
    }

    // Closed by us (Close) rather than by the shell exiting: hang up, as a terminal closing does, so the
    // wait below returns instead of waiting on a shell that is still running
    if (GetState() == State::kTerminate) {
        kill(pid, SIGHUP);
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
