//
// MockTerminalIO tests — verifies the in-memory platform-seam mock behaves (write capture, chunked
// reads incl. byte-wise split, resize flag, raw-mode toggle). The mock underpins the Phase 2 Terminal
// tests, so it gets its own coverage.
//
#include <testinterface.h>

#include <cstdint>
#include <string>

#include "gansi/MockTerminalIO.h"

using namespace gnilk::ansi;

extern "C" {
DLL_EXPORT int test_mockio(ITesting *t);
DLL_EXPORT int test_mockio_write_capture(ITesting *t);
DLL_EXPORT int test_mockio_read_chunks(ITesting *t);
DLL_EXPORT int test_mockio_read_bytewise(ITesting *t);
DLL_EXPORT int test_mockio_resize(ITesting *t);
DLL_EXPORT int test_mockio_rawmode(ITesting *t);
}

DLL_EXPORT int test_mockio(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_mockio_write_capture(ITesting *t) {
    MockTerminalIO io;
    const char *msg = "hello";
    io.Write(reinterpret_cast<const uint8_t *>(msg), 5);
    io.Write(reinterpret_cast<const uint8_t *>("!"), 1);
    TR_ASSERT(t, io.Written() == "hello!");
    return kTR_Pass;
}

DLL_EXPORT int test_mockio_read_chunks(ITesting *t) {
    MockTerminalIO io;
    io.QueueInput("abc");
    uint8_t buf[16];
    long n = io.Read(buf, sizeof(buf), 0);
    TR_ASSERT(t, n == 3);
    TR_ASSERT(t, std::string(reinterpret_cast<char *>(buf), n) == "abc");
    // Drained -> next read times out (0).
    TR_ASSERT(t, io.Read(buf, sizeof(buf), 0) == 0);
    return kTR_Pass;
}

DLL_EXPORT int test_mockio_read_bytewise(ITesting *t) {
    MockTerminalIO io;
    io.QueueInputByteWise("hi");
    uint8_t buf[16];
    TR_ASSERT(t, io.Read(buf, sizeof(buf), 0) == 1);
    TR_ASSERT(t, buf[0] == 'h');
    TR_ASSERT(t, io.Read(buf, sizeof(buf), 0) == 1);
    TR_ASSERT(t, buf[0] == 'i');
    TR_ASSERT(t, io.Read(buf, sizeof(buf), 0) == 0);
    return kTR_Pass;
}

DLL_EXPORT int test_mockio_resize(ITesting *t) {
    MockTerminalIO io;
    io.SetSize(120, 40);
    int c = 0, r = 0;
    TR_ASSERT(t, io.GetSize(c, r));
    TR_ASSERT(t, c == 120 && r == 40);
    TR_ASSERT(t, !io.TakePendingResize());
    io.QueueResize();
    TR_ASSERT(t, io.TakePendingResize());     // consumed once
    TR_ASSERT(t, !io.TakePendingResize());    // and only once
    return kTR_Pass;
}

DLL_EXPORT int test_mockio_rawmode(ITesting *t) {
    MockTerminalIO io;
    TR_ASSERT(t, !io.IsRawMode());
    TR_ASSERT(t, io.EnterRawMode());
    TR_ASSERT(t, io.IsRawMode());
    io.RestoreMode();
    TR_ASSERT(t, !io.IsRawMode());
    TR_ASSERT(t, io.RestoreCalls() == 1);
    return kTR_Pass;
}
