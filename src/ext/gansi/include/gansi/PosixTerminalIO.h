//
// gansi Posix platform I/O — the ONE file that touches termios/poll/signals. macOS + Linux share it
// entirely. A future WindowsTerminalIO would be the sibling impl; nothing above ITerminalIO changes.
//
#ifndef GANSI_POSIXTERMINALIO_H
#define GANSI_POSIXTERMINALIO_H

#include <termios.h>

#include "gansi/ITerminalIO.h"

namespace gnilk::ansi {

    class PosixTerminalIO : public ITerminalIO {
    public:
        // Default: stdin for input, stdout for output.
        PosixTerminalIO();
        // Explicit fds (used by tests to drive a pipe/pty).
        PosixTerminalIO(int inFd, int outFd);
        ~PosixTerminalIO() override;

        bool EnterRawMode() override;
        void RestoreMode() override;
        bool GetSize(int &cols, int &rows) override;
        long Read(uint8_t *buf, size_t n, int timeoutMs) override;
        void Write(const uint8_t *buf, size_t n) override;
        bool TakePendingResize() override;

        // Install the SIGWINCH handler (also done by EnterRawMode). Exposed for tests.
        void InstallResizeHandler();

    private:
        int inFd;
        int outFd;
        bool rawActive = false;
        bool hasSaved = false;
        struct termios savedTermios{};
    };

}

#endif // GANSI_POSIXTERMINALIO_H
