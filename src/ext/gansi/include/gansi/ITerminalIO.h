//
// gansi platform seam — the ONLY interface a platform backend implements. Everything above this
// (grid, encoder, parser, Terminal glue) is platform-independent. Posix impl now; Windows later.
//
#ifndef GANSI_ITERMINALIO_H
#define GANSI_ITERMINALIO_H

#include <cstddef>
#include <cstdint>

namespace gnilk::ansi {

    class ITerminalIO {
    public:
        virtual ~ITerminalIO() = default;

        // Enter/leave raw mode (termios on Posix). RestoreMode must be safe to call even if
        // EnterRawMode failed or was never called (RAII / crash-path safety).
        virtual bool EnterRawMode() = 0;
        virtual void RestoreMode() = 0;

        // Current terminal size in cells. Returns false if unavailable.
        virtual bool GetSize(int &cols, int &rows) = 0;

        // Read up to `n` bytes into `buf`, waiting at most `timeoutMs` (0 = non-blocking, <0 = block).
        // Returns the number of bytes read (0 on timeout), or -1 on error.
        virtual long Read(uint8_t *buf, size_t n, int timeoutMs) = 0;

        // Write exactly `n` bytes (best effort, blocking).
        virtual void Write(const uint8_t *buf, size_t n) = 0;

        // Consume the pending SIGWINCH flag: true exactly once after a resize signal arrived.
        virtual bool TakePendingResize() = 0;
    };

}

#endif // GANSI_ITERMINALIO_H
