//
// gansi input parser — a byte-stream state machine turning terminal input into neutral Events.
//
// Handles: UTF-8 text, C0 controls (Enter/Tab/Backspace/Escape/Ctrl-letter), CSI/SS3 special keys
// (arrows, Home/End, PageUp/Down, Insert/Delete, F1-F12) with xterm modifier params, the Kitty
// keyboard protocol (CSI u), SGR mouse (1006), bracketed paste, and focus in/out.
//
// Split-read tolerant: Feed() buffers any incomplete trailing sequence and completes it on the next
// call. A lone trailing ESC is held pending (it may begin a sequence); Flush() resolves it as Escape
// (call on a read timeout).
//
#ifndef GANSI_INPUTPARSER_H
#define GANSI_INPUTPARSER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "gansi/Event.h"

namespace gnilk::ansi {

    class InputParser {
    public:
        InputParser() = default;

        // Decode `bytes` (length `n`), appending complete events to `out`. Incomplete trailing
        // sequences are buffered internally.
        void Feed(const uint8_t *bytes, size_t n, std::vector<Event> &out);
        void Feed(const std::string &bytes, std::vector<Event> &out);

        // Resolve a pending lone ESC as an Escape key. Call when a read times out with bytes pending.
        void Flush(std::vector<Event> &out);

        // True if there are buffered, not-yet-emitted bytes (an incomplete sequence).
        bool HasPending() const { return !pending.empty(); }

    private:
        // Parse a single token at pending[pos]. Returns bytes consumed, or 0 if more bytes are needed.
        // Emits zero or more events; may toggle paste mode.
        size_t ParseToken(size_t pos, std::vector<Event> &out);
        size_t ParseCSI(size_t pos, std::vector<Event> &out);
        size_t ParseSS3(size_t pos, std::vector<Event> &out);

    private:
        std::string pending;        // buffered bytes not yet fully parsed
        bool inPaste = false;       // between CSI 200~ and CSI 201~
        std::string pasteBuf;       // accumulated paste payload (UTF-8)
    };

}

#endif // GANSI_INPUTPARSER_H
