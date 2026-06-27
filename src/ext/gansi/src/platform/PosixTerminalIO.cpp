//
// gansi Posix platform I/O implementation. termios raw mode (RAII-restored), TIOCGWINSZ size, a
// SIGWINCH atomic flag, poll()-backed read-with-timeout, and a best-effort full write.
//
#include "gansi/PosixTerminalIO.h"

#include <atomic>
#include <cerrno>
#include <csignal>
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
}

PosixTerminalIO::PosixTerminalIO(int inFdIn, int outFdIn) : inFd(inFdIn), outFd(outFdIn) {
}

PosixTerminalIO::~PosixTerminalIO() {
    RestoreMode();
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
    struct pollfd pfd{};
    pfd.fd = inFd;
    pfd.events = POLLIN;

    int pr = poll(&pfd, 1, timeoutMs);
    if (pr < 0) {
        return (errno == EINTR) ? 0 : -1;   // EINTR (e.g. SIGWINCH) -> let the caller re-poll/resize
    }
    if (pr == 0) {
        return 0;       // timeout
    }
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
