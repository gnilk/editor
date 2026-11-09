//
// gvt — the parser's output: syntax events, nothing interpreted.
//
// The Parser only knows the *shape* of the byte stream (DEC/ANSI state machine, vt100.net/emu/dec_ansi_parser).
// What a sequence MEANS is the sink's business - a screen model, a recorder in a test, a logger, a filter.
//
#ifndef GVT_IPARSERSINK_H
#define GVT_IPARSERSINK_H

#include <cstdint>
#include <cstddef>
#include <string_view>

namespace gnilk::vt {

    // CSI / DCS parameters as received. "CSI 1;;3H" is three values: 1, kMissing, 3. "CSI H" is zero values.
    struct CsiParams {
        static constexpr size_t kMaxParams = 32;
        static constexpr int32_t kMissing = -1;
        static constexpr int32_t kMaxValue = 65535;     // larger numbers are clamped

        int32_t values[kMaxParams] = {};
        // Bit n set: the separator AFTER values[n] was ':' (a sub-parameter follows, as in "38:2::r:g:b"),
        // clear: it was ';' or values[n] is the last one. Same convention as Ghostty's params_sep.
        uint32_t colonMask = 0;
        size_t count = 0;

        // The value, or 'defaultValue' when it is missing or not there at all
        int32_t Get(size_t index, int32_t defaultValue) const {
            if ((index >= count) || (values[index] == kMissing)) {
                return defaultValue;
            }
            return values[index];
        }
        bool IsFollowedByColon(size_t index) const {
            return (index < count) && ((colonMask & (1u << index)) != 0);
        }
    };

    class IParserSink {
    public:
        virtual ~IParserSink() = default;

        // A decoded code point (ground state). Invalid UTF-8 arrives as U+FFFD.
        virtual void Print(char32_t codepoint) = 0;
        // A C0 control: BEL BS HT LF VT FF CR SO SI ... (CAN/SUB too: they also cancel a sequence in progress)
        virtual void Execute(uint8_t control) = 0;
        // prefix: 0 or the private marker '<' '=' '>' '?' that opened the parameters. intermediates: 0x20-0x2F.
        virtual void CsiDispatch(const CsiParams &params, std::string_view intermediates, uint8_t prefix,
                                 uint8_t final) = 0;
        // ESC <intermediates> <final>, e.g. "ESC ( B" is intermediates "(" and final 'B'
        virtual void EscDispatch(std::string_view intermediates, uint8_t final) = 0;
        // Everything between "ESC ]" and BEL / "ESC \", unparsed ("0;title", "133;A", "7;file://host/path")
        virtual void OscDispatch(std::string_view payload) = 0;

        // DCS: header (hook), data bytes (put), end (unhook). Default: ignored.
        virtual void DcsHook(const CsiParams &params, std::string_view intermediates, uint8_t prefix,
                             uint8_t final) {
        }
        virtual void DcsPut(uint8_t byte) {
        }
        virtual void DcsUnhook() {
        }
        // "ESC _ ... ESC \" (kitty graphics and friends). Default: ignored. SOS and PM are always dropped.
        virtual void ApcDispatch(std::string_view payload) {
        }
    };

}

#endif // GVT_IPARSERSINK_H
