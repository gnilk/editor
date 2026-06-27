//
// gansi PosixTerminalIO tests — exercise the real platform file over pipes + a raised SIGWINCH, so
// no interactive TTY is needed: poll-backed Read (data + timeout), full Write, the resize flag, and
// graceful behavior on a non-tty fd (raw mode / GetSize return false rather than crashing).
//
#include <testinterface.h>

#include <csignal>
#include <cstring>
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
