//
// MockTerminalIO — in-memory ITerminalIO test double (a public header so the editor's own backend
// tests can drive gansi headlessly too, not just the library's tests). Scripted input bytes feed
// Read() (optionally chunked to simulate sequences split across reads); Write() captures all output;
// resize + size are scripted directly. Not used in production paths.
//
#ifndef GANSI_MOCKTERMINALIO_H
#define GANSI_MOCKTERMINALIO_H

#include <algorithm>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "gansi/ITerminalIO.h"

namespace gnilk::ansi {

    class MockTerminalIO : public ITerminalIO {
    public:
        // --- ITerminalIO ---
        bool EnterRawMode() override {
            rawMode = true;
            return true;
        }
        void RestoreMode() override {
            rawMode = false;
            restoreCalls++;
        }
        bool GetSize(int &outCols, int &outRows) override {
            outCols = cols;
            outRows = rows;
            return true;
        }
        long Read(uint8_t *buf, size_t n, int /*timeoutMs*/) override {
            if (inputChunks.empty()) {
                return 0;       // timeout / no data
            }
            std::string &chunk = inputChunks.front();
            size_t take = std::min(n, chunk.size());
            std::memcpy(buf, chunk.data(), take);
            if (take == chunk.size()) {
                inputChunks.pop_front();
            } else {
                chunk.erase(0, take);
            }
            return static_cast<long>(take);
        }
        void Write(const uint8_t *buf, size_t n) override {
            written.append(reinterpret_cast<const char *>(buf), n);
        }
        bool TakePendingResize() override {
            bool p = pendingResize;
            pendingResize = false;
            return p;
        }

        // --- Test scripting helpers ---
        void SetSize(int c, int r) {
            cols = c;
            rows = r;
        }
        void QueueResize() {
            pendingResize = true;
        }
        // Queue input as a single read chunk.
        void QueueInput(const std::string &bytes) {
            inputChunks.push_back(bytes);
        }
        // Queue input split into 1-byte chunks (simulate a sequence dribbled across reads).
        void QueueInputByteWise(const std::string &bytes) {
            for (char c : bytes) {
                inputChunks.push_back(std::string(1, c));
            }
        }

        const std::string &Written() const { return written; }
        void ClearWritten() { written.clear(); }
        bool IsRawMode() const { return rawMode; }
        int RestoreCalls() const { return restoreCalls; }

    private:
        int cols = 80;
        int rows = 24;
        bool rawMode = false;
        bool pendingResize = false;
        int restoreCalls = 0;
        std::deque<std::string> inputChunks;
        std::string written;
    };

}

#endif // GANSI_TEST_MOCKTERMINALIO_H
