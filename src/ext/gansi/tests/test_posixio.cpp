//
// gansi PosixTerminalIO tests — exercise the real platform file over pipes + a raised SIGWINCH, so
// no interactive TTY is needed: poll-backed Read (data + timeout), full Write, the resize flag, and
// graceful behavior on a non-tty fd (raw mode / GetSize return false rather than crashing).
//
#include <testinterface.h>

#include <chrono>
#include <csignal>
#include <cstring>
#include <thread>
#include <unistd.h>

#include "gansi/PosixTerminalIO.h"

using namespace gnilk::ansi;

extern "C" {
DLL_EXPORT int test_posixio(ITesting *t);
DLL_EXPORT int test_posixio_read_poll(ITesting *t);
DLL_EXPORT int test_posixio_read_timeout(ITesting *t);
DLL_EXPORT int test_posixio_write(ITesting *t);
DLL_EXPORT int test_posixio_resize_flag(ITesting *t);
DLL_EXPORT int test_posixio_rawmode_nontty(ITesting *t);
DLL_EXPORT int test_posixio_getsize_nontty(ITesting *t);
DLL_EXPORT int test_posixio_wake_blocked(ITesting *t);
DLL_EXPORT int test_posixio_wake_before_read(ITesting *t);
DLL_EXPORT int test_posixio_wake_with_input(ITesting *t);
}

DLL_EXPORT int test_posixio(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_posixio_read_poll(ITesting *t) {
    int fds[2];
    TR_ASSERT(t, pipe(fds) == 0);
    PosixTerminalIO io(fds[0], fds[1]);
    TR_ASSERT(t, write(fds[1], "xy", 2) == 2);

    uint8_t buf[16];
    long n = io.Read(buf, sizeof(buf), 200);
    TR_ASSERT(t, n == 2);
    TR_ASSERT(t, buf[0] == 'x' && buf[1] == 'y');

    close(fds[0]);
    close(fds[1]);
    return kTR_Pass;
}

DLL_EXPORT int test_posixio_read_timeout(ITesting *t) {
    int fds[2];
    TR_ASSERT(t, pipe(fds) == 0);
    PosixTerminalIO io(fds[0], fds[1]);

    uint8_t buf[16];
    long n = io.Read(buf, sizeof(buf), 10);   // no data queued -> timeout
    TR_ASSERT(t, n == 0);

    close(fds[0]);
    close(fds[1]);
    return kTR_Pass;
}

DLL_EXPORT int test_posixio_write(ITesting *t) {
    int fds[2];
    TR_ASSERT(t, pipe(fds) == 0);
    PosixTerminalIO io(fds[0], fds[1]);

    io.Write(reinterpret_cast<const uint8_t *>("hello"), 5);
    char rbuf[16];
    ssize_t r = read(fds[0], rbuf, sizeof(rbuf));
    TR_ASSERT(t, r == 5);
    TR_ASSERT(t, memcmp(rbuf, "hello", 5) == 0);

    close(fds[0]);
    close(fds[1]);
    return kTR_Pass;
}

DLL_EXPORT int test_posixio_resize_flag(ITesting *t) {
    PosixTerminalIO io(STDIN_FILENO, STDOUT_FILENO);
    io.InstallResizeHandler();
    io.TakePendingResize();                 // clear any stale flag
    TR_ASSERT(t, !io.TakePendingResize());

    raise(SIGWINCH);
    TR_ASSERT(t, io.TakePendingResize());    // set by the handler
    TR_ASSERT(t, !io.TakePendingResize());   // consumed exactly once
    return kTR_Pass;
}

DLL_EXPORT int test_posixio_rawmode_nontty(ITesting *t) {
    int fds[2];
    TR_ASSERT(t, pipe(fds) == 0);
    PosixTerminalIO io(fds[0], fds[1]);

    // A pipe is not a terminal -> raw mode fails gracefully, restore is safe.
    TR_ASSERT(t, !io.EnterRawMode());
    io.RestoreMode();

    close(fds[0]);
    close(fds[1]);
    return kTR_Pass;
}

DLL_EXPORT int test_posixio_getsize_nontty(ITesting *t) {
    int fds[2];
    TR_ASSERT(t, pipe(fds) == 0);
    PosixTerminalIO io(fds[0], fds[1]);

    int c = -1, r = -1;
    TR_ASSERT(t, !io.GetSize(c, r));   // a pipe has no window size
    close(fds[0]);
    close(fds[1]);
    return kTR_Pass;
}

// A Read blocked with a long timeout returns as soon as another thread calls Wake()
DLL_EXPORT int test_posixio_wake_blocked(ITesting *t) {
    int fds[2];
    TR_ASSERT(t, pipe(fds) == 0);
    PosixTerminalIO io(fds[0], fds[1]);

    std::thread waker([&io]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        io.Wake();
    });
    auto tStart = std::chrono::steady_clock::now();
    uint8_t buf[16];
    long n = io.Read(buf, sizeof(buf), 5000);
    auto elapsed = std::chrono::steady_clock::now() - tStart;
    waker.join();

    TR_ASSERT(t, n == 0);
    TR_ASSERT(t, elapsed < std::chrono::milliseconds(2000));

    close(fds[0]);
    close(fds[1]);
    return kTR_Pass;
}

// A wake that comes before the Read is not lost; several wakes collapse into one return, so the next Read waits
// out its timeout again
DLL_EXPORT int test_posixio_wake_before_read(ITesting *t) {
    int fds[2];
    TR_ASSERT(t, pipe(fds) == 0);
    PosixTerminalIO io(fds[0], fds[1]);

    io.Wake();
    io.Wake();
    io.Wake();
    uint8_t buf[16];
    auto tStart = std::chrono::steady_clock::now();
    long n = io.Read(buf, sizeof(buf), 5000);
    TR_ASSERT(t, n == 0);
    TR_ASSERT(t, (std::chrono::steady_clock::now() - tStart) < std::chrono::milliseconds(2000));

    tStart = std::chrono::steady_clock::now();
    n = io.Read(buf, sizeof(buf), 50);
    TR_ASSERT(t, n == 0);
    TR_ASSERT(t, (std::chrono::steady_clock::now() - tStart) >= std::chrono::milliseconds(40));

    close(fds[0]);
    close(fds[1]);
    return kTR_Pass;
}

// Input that is ready together with a wake is still read
DLL_EXPORT int test_posixio_wake_with_input(ITesting *t) {
    int fds[2];
    TR_ASSERT(t, pipe(fds) == 0);
    PosixTerminalIO io(fds[0], fds[1]);

    TR_ASSERT(t, write(fds[1], "q", 1) == 1);
    io.Wake();
    uint8_t buf[16];
    long n = io.Read(buf, sizeof(buf), 200);
    TR_ASSERT(t, n == 1);
    TR_ASSERT(t, buf[0] == 'q');

    close(fds[0]);
    close(fds[1]);
    return kTR_Pass;
}
