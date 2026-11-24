//
// Created by gnilk on 19.01.23.
//

#if defined(GEDIT_MACOS)
#include <util.h>
#include <libproc.h>
#elif defined(GEDIT_LINUX)
#include <pty.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <termios.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <thread>
#include <chrono>
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

Shell::~Shell() {
    Close();
    for (auto &fd : wakeFds) {
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }
}

bool Shell::Begin(const std::string &shell, const std::string &args,
                  const std::vector<std::string> &initScript) {
    logger = gnilk::Logger::GetLogger("Shell");
    if (reader.joinable()) {
        logger->Error("Begin: the shell is already started");
        return false;
    }
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
        // Parent — amaster is the only fd we need. Never inherited: a shell started later (another tab) holding this
        // master keeps the pty open after we close it - no hangup reaches this shell's jobs, and a dying shell waits
        // in the kernel for ever (open-bugs #36: quit hung with agent tabs)
        ::fcntl(amaster, F_SETFD, FD_CLOEXEC);
        logger->Debug("Started child with pid=%d", pid);
        if ((wakeFds[0] < 0) && (::pipe(wakeFds) == 0)) {
            ::fcntl(wakeFds[0], F_SETFD, FD_CLOEXEC);
            ::fcntl(wakeFds[1], F_SETFD, FD_CLOEXEC);
            ::fcntl(wakeFds[1], F_SETFL, O_NONBLOCK);
        }
        // Joined by Close() - never detached: it holds 'this' and calls the output delegate
        reader = std::thread(&Shell::ConsumePty, this);
    } else if (pid == 0) {
        // Child — pty slave is already stdin/stdout/stderr; just exec.
        // Set TERM so ncurses-based programs (less, vi, ...) find their
        // terminfo entry and know the terminal is capable.
        ::setenv("TERM", "xterm-256color", 1);
        // Nothing of the editor's but the pty: other tabs' masters (one forked between their forkpty and their
        // FD_CLOEXEC), files, pipes
        CloseInheritedFds();
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

std::string Shell::GetWorkingDirectory() const {
    if (pid <= 0) {
        return {};
    }
#if defined(GEDIT_MACOS)
    struct proc_vnodepathinfo info = {};
    if (proc_pidinfo(pid, PROC_PIDVNODEPATHINFO, 0, &info, sizeof(info)) != sizeof(info)) {
        return {};
    }
    return std::string(info.pvi_cdir.vip_path);
#else
    char buffer[4096];
    auto link = "/proc/" + std::to_string(pid) + "/cwd";
    auto len = readlink(link.c_str(), buffer, sizeof(buffer) - 1);
    if (len <= 0) {
        return {};
    }
    return std::string(buffer, (size_t)len);
#endif
}

void Shell::Close() {
    if (!reader.joinable()) {
        return;
    }
    if (reader.get_id() == std::this_thread::get_id()) {
        // From the delegate - the reader ends after it returns; whoever owns us joins it
        ChangeState(State::kTerminate);
        return;
    }
    if (GetState() != State::kTerminated) {
        ChangeState(State::kTerminate);
        WakeReader();
    }
    reader.join();
}

void Shell::WakeReader() {
    if (wakeFds[1] >= 0) {
        uint8_t byte = 1;
        [[maybe_unused]] auto n = ::write(wakeFds[1], &byte, 1);
    }
}

void Shell::CleanUp() {
    if (amaster >= 0) {
        ::close(amaster);
        amaster = -1;
    }
}

// Closed by us (Close) rather than by the shell exiting: hang up, as a terminal closing does. A shell that ignores
// it (trap '' HUP) gets a grace period and then SIGKILL - the quit must not hang on it. Our end of the pty closes
// first: nobody reads it any more, and on macOS a shell that exits with output still queued for us (bash 5 on the
// hangup) waits in the kernel for it to drain - SIGKILL doesn't end that wait, and waitpid never returned.
void Shell::ReapChild() {
    static constexpr int kGraceMs = 500;
    static constexpr int kKilledGraceMs = 1000;
    if (GetState() == State::kTerminate) {
        // The job in front (an agent, an editor) has its own process group - it gets the hangup too, as a closing
        // terminal gives it
        auto foreground = (amaster >= 0) ? tcgetpgrp(amaster) : -1;
        if ((foreground > 0) && (foreground != pid)) {
            kill(-foreground, SIGHUP);
        }
        kill(pid, SIGHUP);
        CleanUp();
    }
    int status = 0;
    auto reaped = WaitForChild(status, kGraceMs);
    if (reaped == 0) {
        logger->Warning("Shell pid=%d didn't exit on hangup - killing it", pid);
        kill(pid, SIGKILL);
        reaped = WaitForChild(status, kKilledGraceMs);
    }
    if (reaped == 0) {
        // Stuck in the kernel (a tty it can't let go of): left behind rather than hang the UI thread that joins us
        logger->Error("Shell pid=%d didn't go on SIGKILL either - left behind", pid);
        return;
    }
    if ((reaped == pid) && WIFEXITED(status)) {
        exitStatus = WEXITSTATUS(status);
    }
}

pid_t Shell::WaitForChild(int &status, int timeoutMs) {
    auto tEnd = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    pid_t reaped = 0;
    while ((reaped = waitpid(pid, &status, WNOHANG)) == 0) {
        if (std::chrono::steady_clock::now() >= tEnd) {
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return reaped;
}

void Shell::CloseInheritedFds() {
    // In the child, before exec: only close() - async-signal-safe after fork in a threaded process
    auto maxFd = sysconf(_SC_OPEN_MAX);
    if ((maxFd < 0) || (maxFd > MAX_FD_TO_CLOSE)) {
        maxFd = MAX_FD_TO_CLOSE;
    }
    for (int fd = STDERR_FILENO + 1; fd < (int)maxFd; fd++) {
        ::close(fd);
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
        struct pollfd pfds[2] = { { amaster, POLLIN, 0 }, { wakeFds[0], POLLIN, 0 } };
        int ret = poll(pfds, (wakeFds[0] >= 0) ? 2 : 1, GEDIT_DEFAULT_POLL_TMO_MS);
        if (ret < 0) {
            if (errno == EINTR) { continue; }
            break;
        }
        if (ret == 0) { continue; }
        if ((wakeFds[0] >= 0) && (pfds[1].revents & POLLIN)) {
            // Close() - the loop condition sees kTerminate
            continue;
        }

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

    ReapChild();

    logger->Debug("Leaving ConsumePty for pid=%d", pid);
    CleanUp();
    ChangeState(State::kTerminated);
}
