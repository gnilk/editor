//
// gvt — streaming DEC/ANSI parser.
//
// Paul Flo Williams' state machine (https://vt100.net/emu/dec_ansi_parser), with the changes every modern UTF-8
// terminal makes (xterm, libvterm, Ghostty, Alacritty):
//   - the stream is UTF-8: bytes 0x80-0x9F are never C1 controls (they are continuation bytes - '▛' is E2 96 9B)
//   - ':' is a sub-parameter separator, not a reason to ignore the sequence (SGR 38:2::r:g:b)
//   - OSC also ends at BEL; SOS/PM/APC end at "ESC \" only
//   - too many parameters or intermediates drop the whole sequence instead of dispatching a truncated one
//
// State persists between Feed calls, so a sequence (or a UTF-8 character) split across two pty reads is
// parsed exactly as if it came in one. Nothing is allocated per byte.
//
#ifndef GVT_PARSER_H
#define GVT_PARSER_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>

#include "gvt/IParserSink.h"

namespace gnilk::vt {

    class Parser {
    public:
        enum class State : uint8_t {
            kGround,
            kEscape,
            kEscapeIntermediate,
            kCsiEntry,
            kCsiParam,
            kCsiIntermediate,
            kCsiIgnore,
            kDcsEntry,
            kDcsParam,
            kDcsIntermediate,
            kDcsPassthrough,
            kDcsIgnore,
            kOscString,
            kSosPmApcString,
        };

        static constexpr size_t kMaxIntermediates = 4;
        static constexpr size_t kMaxStringLength = 64 * 1024;   // OSC / APC payload; longer ones are dropped

    public:
        explicit Parser(IParserSink &sink);

        void Feed(const uint8_t *data, size_t length);
        void Feed(std::string_view data);
        // Back to ground; anything half-parsed is forgotten
        void Reset();

        State GetState() const {
            return state;
        }

    protected:
        enum class StringKind : uint8_t {
            kSos,
            kPm,
            kApc,
        };

    protected:
        void Advance(uint8_t byte);
        void AdvanceGround(uint8_t byte);
        void AdvanceEscape(uint8_t byte);
        void AdvanceEscapeIntermediate(uint8_t byte);
        void AdvanceCsi(uint8_t byte);
        void AdvanceDcsHeader(uint8_t byte);
        void AdvanceDcsPassthrough(uint8_t byte);
        void AdvanceOscString(uint8_t byte);
        void AdvanceSosPmApcString(uint8_t byte);

        void DecodeUtf8(uint8_t byte);
        void FlushIncompleteUtf8();

        void EnterEscape();
        void EnterString(State newState);
        // ESC, CAN or SUB arrived: finish what the current state was collecting (an OSC/APC string is dispatched
        // only when terminated by ESC, a DCS is unhooked either way). True when that state was a string state.
        bool LeaveState(bool isTerminated);

        void Clear();
        void Collect(uint8_t byte);
        void Param(uint8_t byte);
        // Closes the parameter being collected. False when there are too many.
        bool FinishParams();
        void DispatchCsi(uint8_t final);
        void HookDcs(uint8_t final);

        static bool IsExecutable(uint8_t byte);
        static bool IsIntermediate(uint8_t byte);
        static bool IsParamByte(uint8_t byte);
        static bool IsPrivateMarker(uint8_t byte);
        static bool IsFinal(uint8_t byte);

    private:
        IParserSink &sink;
        State state = State::kGround;

        // UTF-8 decoding in ground
        char32_t utf8Codepoint = 0;
        char32_t utf8Minimum = 0;
        int utf8Remaining = 0;

        // Escape / CSI / DCS collection
        char intermediates[kMaxIntermediates] = {};
        size_t intermediateCount = 0;
        bool isIntermediateOverflow = false;
        uint8_t prefix = 0;
        CsiParams params;
        int32_t currentParam = CsiParams::kMissing;
        bool hasParamBytes = false;
        bool isParamOverflow = false;

        // OSC / APC payload
        std::string stringBuffer;
        StringKind stringKind = StringKind::kSos;
        bool isStringOverflow = false;
        // ESC just ended a string: the '\' that completes ST is swallowed, not dispatched as ESC '\'
        bool isStringTerminatorPending = false;
    };

}

#endif // GVT_PARSER_H
