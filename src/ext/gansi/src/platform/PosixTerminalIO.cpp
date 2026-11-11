//
// gansi Posix platform I/O implementation. termios raw mode (RAII-restored), TIOCGWINSZ size, a
// SIGWINCH atomic flag, poll()-backed read-with-timeout, and a best-effort full write.
//
#include "gansi/PosixTerminalIO.h"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

using namespace gnilk::ansi;

namespace {
    // SIGWINCH is process-global; the handler only flips this flag (async-signal-safe).
    std::atomic<bool> glbResizePending{false};

    void OnSigwinch(int) {
        glbResizePending.store(true, std::memory_order_relaxed);
    }
}

PosixTerminalIO::PosixTerminalIO() : inFd(STDIN_FILENO), outFd(STDOUT_FILENO) {
    CreateWakePipe();
}

PosixTerminalIO::PosixTerminalIO(int inFdIn, int outFdIn) : inFd(inFdIn), outFd(outFdIn) {
    CreateWakePipe();
}

PosixTerminalIO::~PosixTerminalIO() {
    RestoreMode();
    if (wakeReadFd >= 0) {
        close(wakeReadFd);
    }
    if (wakeWriteFd >= 0) {
        close(wakeWriteFd);
    }
}

// Without the pipe (fd exhaustion) Wake() is a no-op and Read() waits out its timeout, as before.
void PosixTerminalIO::CreateWakePipe() {
    int fds[2];
    if (pipe(fds) != 0) {
        return;
    }
    for (int fd : fds) {
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
        fcntl(fd, F_SETFD, FD_CLOEXEC);     // not inherited by the editor's shell / child processes
    }
    wakeReadFd = fds[0];
    wakeWriteFd = fds[1];
}

void PosixTerminalIO::InstallResizeHandler() {
    struct sigaction sa{};
    sa.sa_handler = OnSigwinch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, nullptr);
}

bool PosixTerminalIO::EnterRawMode() {
    if (rawActive) {
        return true;
    }
    if (tcgetattr(inFd, &savedTermios) != 0) {
        return false;       // not a terminal (e.g. piped) — caller decides what to do
    }
    hasSaved = true;

    struct termios raw = savedTermios;
    cfmakeraw(&raw);
    if (tcsetattr(inFd, TCSAFLUSH, &raw) != 0) {
        return false;
    }
    rawActive = true;
    InstallResizeHandler();
    return true;
}

void PosixTerminalIO::RestoreMode() {
    if (rawActive && hasSaved) {
        tcsetattr(inFd, TCSAFLUSH, &savedTermios);
    }
    rawActive = false;
}

bool PosixTerminalIO::GetSize(int &cols, int &rows) {
    struct winsize ws{};
    if (ioctl(outFd, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        cols = ws.ws_col;
        rows = ws.ws_row;
        return true;
    }
    if (ioctl(inFd, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        cols = ws.ws_col;
        rows = ws.ws_row;
        return true;
    }
    return false;
}

long PosixTerminalIO::Read(uint8_t *buf, size_t n, int timeoutMs) {
    struct pollfd pfds[2]{};
    pfds[0].fd = inFd;
    pfds[0].events = POLLIN;
    pfds[1].fd = wakeReadFd;        // -1 (no pipe) is ignored by poll
    pfds[1].events = POLLIN;

    int pr = poll(pfds, 2, timeoutMs);
    if (pr < 0) {
        return (errno == EINTR) ? 0 : -1;   // EINTR (e.g. SIGWINCH) -> let the caller re-poll/resize
    }
    if (pr == 0) {
        return 0;       // timeout
    }
    if (pfds[1].revents & POLLIN) {
        DrainWakePipe();
    }
    struct pollfd &pfd = pfds[0];
    if (pfd.revents & (POLLIN | POLLHUP)) {
        ssize_t r = read(inFd, buf, n);
        if (r < 0) {
            return (errno == EINTR || errno == EAGAIN) ? 0 : -1;
        }
        return static_cast<long>(r);
    }
    return 0;
}

void PosixTerminalIO::Write(const uint8_t *buf, size_t n) {
    size_t written = 0;
    while (written < n) {
        ssize_t w = write(outFd, buf + written, n - written);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;      // give up on a hard error
        }
        written += static_cast<size_t>(w);
    }
}

bool PosixTerminalIO::TakePendingResize() {
    return glbResizePending.exchange(false, std::memory_order_relaxed);
}

// write() on a pipe is async-signal- and thread-safe. A full pipe (EAGAIN) already holds a wake - fine.
void PosixTerminalIO::Wake() {
    if (wakeWriteFd < 0) {
        return;
    }
    uint8_t byte = 1;
    ssize_t res = write(wakeWriteFd, &byte, 1);
    (void)res;
}

// All pending wakes collapse into this one return
void PosixTerminalIO::DrainWakePipe() {
    uint8_t buf[64];
    while (read(wakeReadFd, buf, sizeof(buf)) > 0) {
    }
}
