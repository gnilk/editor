//
// gvt Parser — syntax events per state transition (vt100.net/emu/dec_ansi_parser), plus split invariance:
// a stream fed in any number of pieces must give exactly the events it gives fed whole.
//
// Cases marked "ghostty:" are ported from the test vectors in Ghostty's src/terminal/Parser.zig (MIT,
// https://github.com/ghostty-org/ghostty). Ghostty folds the private marker into its intermediates and reports
// a missing parameter as 0; here the marker is the separate 'prefix' and a missing parameter stays missing.
//
#include <string>
#include <string_view>

#include <testinterface.h>

#include "gvt/Parser.h"
#include "EventRecorder.h"
#include "Fixtures.h"

using namespace gnilk::vt;
using namespace gvttest;

extern "C" {
DLL_EXPORT int test_parser(ITesting *t);
DLL_EXPORT int test_parser_print(ITesting *t);
DLL_EXPORT int test_parser_utf8(ITesting *t);
DLL_EXPORT int test_parser_utf8invalid(ITesting *t);
DLL_EXPORT int test_parser_execute(ITesting *t);
DLL_EXPORT int test_parser_esc(ITesting *t);
DLL_EXPORT int test_parser_csi(ITesting *t);
DLL_EXPORT int test_parser_csiprefix(ITesting *t);
DLL_EXPORT int test_parser_csiintermediate(ITesting *t);
DLL_EXPORT int test_parser_csisubparams(ITesting *t);
DLL_EXPORT int test_parser_csiparamstruct(ITesting *t);
DLL_EXPORT int test_parser_csimaxparams(ITesting *t);
DLL_EXPORT int test_parser_csimalformed(ITesting *t);
DLL_EXPORT int test_parser_cancel(ITesting *t);
DLL_EXPORT int test_parser_osc(ITesting *t);
DLL_EXPORT int test_parser_oscoverflow(ITesting *t);
DLL_EXPORT int test_parser_dcs(ITesting *t);
DLL_EXPORT int test_parser_apc(ITesting *t);
DLL_EXPORT int test_parser_reset(ITesting *t);
DLL_EXPORT int test_parser_split(ITesting *t);
DLL_EXPORT int test_parser_splitfixtures(ITesting *t);
}

// Everything parsed in one Feed, as the recorder's normalized lines
static std::string ParseWhole(std::string_view bytes) {
    EventRecorder recorder;
    Parser parser(recorder);
    parser.Feed(bytes);
    return recorder.Joined();
}

// Fed in pieces of 'chunkSize' bytes
static std::string ParseChunked(std::string_view bytes, size_t chunkSize) {
    EventRecorder recorder;
    Parser parser(recorder);
    for (size_t i = 0; i < bytes.size(); i += chunkSize) {
        parser.Feed(bytes.substr(i, chunkSize));
    }
    return recorder.Joined();
}

// Control bytes made visible, so a failing comparison can be read
static std::string Visible(std::string_view text) {
    std::string out;
    for (unsigned char c : text) {
        if (c == '\n') {
            out += "\\n";
        } else if ((c < 0x20) || (c == 0x7F)) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\x%02x", c);
            out += buf;
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

#define EXPECT_EVENTS(t, input, expected)                                                               \
    do {                                                                                                \
        auto actual_ = ParseWhole(input);                                                               \
        if (actual_ != (expected)) {                                                                    \
            (t)->Error(__LINE__, __FILE__, "input '%s'\n  expected: %s\n  actual:   %s",                  \
                       Visible(input).c_str(), Visible(expected).c_str(), Visible(actual_).c_str());    \
            return kTR_Fail;                                                                            \
        }                                                                                               \
    } while (0)

DLL_EXPORT int test_parser(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_parser_print(ITesting *t) {
    EXPECT_EVENTS(t, "abc", "TEXT abc\n");
    // DEL is ignored
    EXPECT_EVENTS(t, "a\x7f" "b", "TEXT ab\n");
    return kTR_Pass;
}

DLL_EXPORT int test_parser_utf8(ITesting *t) {
    EXPECT_EVENTS(t, "å ▛ ✳ 😀", "TEXT å ▛ ✳ 😀\n");
    // E2 96 9B ('▛') and E2 9C B3 ('✳'): 0x9B is CSI and 0x9C is ST as C1 controls - not in UTF-8
    EXPECT_EVENTS(t, "\xE2\x96\x9B\xE2\x9C\xB3", "TEXT ▛✳\n");
    return kTR_Pass;
}

DLL_EXPORT int test_parser_utf8invalid(ITesting *t) {
    // Stray continuation byte
    EXPECT_EVENTS(t, "a\x80" "b", "TEXT a\xEF\xBF\xBD" "b\n");
    // Lead byte that can't start a character (C0 = overlong form of a 1-byte char)
    EXPECT_EVENTS(t, "\xC0" "A", "TEXT \xEF\xBF\xBD" "A\n");
    // Truncated by an ASCII byte
    EXPECT_EVENTS(t, "\xE2\x96" "A", "TEXT \xEF\xBF\xBD" "A\n");
    // Truncated by a new lead byte
    EXPECT_EVENTS(t, "\xE2\xC3\xA5", "TEXT \xEF\xBF\xBD\xC3\xA5\n");
    // Truncated by ESC: the sequence still parses
    EXPECT_EVENTS(t, "\xE2\x1b[H", "TEXT \xEF\xBF\xBD\nCSI H\n");
    // Surrogate (ED A0 80 = U+D800)
    EXPECT_EVENTS(t, "\xED\xA0\x80", "TEXT \xEF\xBF\xBD\n");
    return kTR_Pass;
}

DLL_EXPORT int test_parser_execute(ITesting *t) {
    EXPECT_EVENTS(t, "a\r\nb\x07\x08\t", "TEXT a\nC0 0d\nC0 0a\nTEXT b\nC0 07\nC0 08\nC0 09\n");
    return kTR_Pass;
}

DLL_EXPORT int test_parser_esc(ITesting *t) {
    // ghostty: esc: ESC ( B
    EXPECT_EVENTS(t, "\x1b(B", "ESC (B\n");
    EXPECT_EVENTS(t, "\x1b" "7\x1b" "8\x1bM\x1b=\x1b>", "ESC 7\nESC 8\nESC M\nESC =\nESC >\n");
    EXPECT_EVENTS(t, "\x1b#8", "ESC #8\n");
    // A control inside an escape runs and the escape continues
    EXPECT_EVENTS(t, "\x1b\r7", "C0 0d\nESC 7\n");
    // ESC \ on its own (no string before it) is an ordinary escape
    EXPECT_EVENTS(t, "\x1b\\", "ESC \\\n");
    return kTR_Pass;
}

DLL_EXPORT int test_parser_csi(ITesting *t) {
    // ghostty: csi: ESC [ H
    EXPECT_EVENTS(t, "\x1b[H", "CSI H\n");
    // ghostty: csi: ESC [ 1 ; 4 H
    EXPECT_EVENTS(t, "\x1b[1;4H", "CSI 1;4H\n");
    EXPECT_EVENTS(t, "\x1b[;5H", "CSI ;5H\n");
    EXPECT_EVENTS(t, "\x1b[5;H", "CSI 5;H\n");
    EXPECT_EVENTS(t, "\x1b[38;2;255;193;7m", "CSI 38;2;255;193;7m\n");
    // Numbers are clamped, not wrapped
    EXPECT_EVENTS(t, "\x1b[99999999999A", "CSI 65535A\n");
    // A control inside a CSI runs; the CSI continues
    EXPECT_EVENTS(t, "\x1b[1\r2H", "C0 0d\nCSI 12H\n");
    // ESC restarts
    EXPECT_EVENTS(t, "\x1b[12\x1b[3H", "CSI 3H\n");
    return kTR_Pass;
}

DLL_EXPORT int test_parser_csiprefix(ITesting *t) {
    EXPECT_EVENTS(t, "\x1b[?1049;25h", "CSI ?1049;25h\n");
    // The prefixed forms terminal-enhancements.md §4.B.4 saw misdispatched
    EXPECT_EVENTS(t, "\x1b[>4m", "CSI >4m\n");
    EXPECT_EVENTS(t, "\x1b[?u", "CSI ?u\n");
    EXPECT_EVENTS(t, "\x1b[<u", "CSI <u\n");
    EXPECT_EVENTS(t, "\x1b[>c", "CSI >c\n");
    EXPECT_EVENTS(t, "\x1b[=5u", "CSI =5u\n");
    // SGR mouse report shape
    EXPECT_EVENTS(t, "\x1b[<0;12;5M", "CSI <0;12;5M\n");
    return kTR_Pass;
}

DLL_EXPORT int test_parser_csiintermediate(ITesting *t) {
    // ghostty: csi: request mode decrqm
    EXPECT_EVENTS(t, "\x1b[?2026$p", "CSI ?2026$p\n");
    // ghostty: csi: change cursor
    EXPECT_EVENTS(t, "\x1b[3 q", "CSI 3 q\n");
    EXPECT_EVENTS(t, "\x1b[!p", "CSI !p\n");
    return kTR_Pass;
}

DLL_EXPORT int test_parser_csisubparams(ITesting *t) {
    // ghostty: csi: SGR ESC [ 38 : 2 m
    EXPECT_EVENTS(t, "\x1b[38:2m", "CSI 38:2m\n");
    // ghostty: csi: SGR colon followed by semicolon (a following CSI still parses)
    EXPECT_EVENTS(t, "\x1b[48:2m\x1b[H", "CSI 48:2m\nCSI H\n");
    // ghostty: csi: SGR mixed colon and semicolon
    EXPECT_EVENTS(t, "\x1b[38:5:1;48:5:0m", "CSI 38:5:1;48:5:0m\n");
    // ghostty: csi: SGR ESC [ 48 : 2 m
    EXPECT_EVENTS(t, "\x1b[48:2:240:143:104m", "CSI 48:2:240:143:104m\n");
    // ghostty: csi: SGR ESC [4:3m colon
    EXPECT_EVENTS(t, "\x1b[4:3m", "CSI 4:3m\n");
    // ghostty: csi: SGR with many blank and colon
    EXPECT_EVENTS(t, "\x1b[58:2::240:143:104m", "CSI 58:2::240:143:104m\n");
    // ghostty: csi: SGR mixed colon and semicolon with blank (Kakoune)
    EXPECT_EVENTS(t, "\x1b[;4:3;38;2;175;175;215;58:2::190:80:70m",
                  "CSI ;4:3;38;2;175;175;215;58:2::190:80:70m\n");
    // ghostty: csi: SGR mixed colon and semicolon setting underline, bg, fg (Kakoune)
    EXPECT_EVENTS(t, "\x1b[4:3;38;2;51;51;51;48;2;170;170;170;58;2;255;97;136m",
                  "CSI 4:3;38;2;51;51;51;48;2;170;170;170;58;2;255;97;136m\n");
    // ghostty: csi: colon for non-m final - Ghostty drops it in the parser. Here the parser reports what it saw
    // and the screen model decides (it ignores colons outside SGR).
    EXPECT_EVENTS(t, "\x1b[38:2h", "CSI 38:2h\n");
    return kTR_Pass;
}

// The structure behind the text form: values, missing, colon mask (ghostty's params_sep checks)
class ParamCapture : public IParserSink {
public:
    CsiParams last;
    uint8_t lastFinal = 0;
    int dispatchCount = 0;

    void Print(char32_t) override {
    }
    void Execute(uint8_t) override {
    }
    void CsiDispatch(const CsiParams &params, std::string_view, uint8_t, uint8_t final) override {
        last = params;
        lastFinal = final;
        dispatchCount++;
    }
    void EscDispatch(std::string_view, uint8_t) override {
    }
    void OscDispatch(std::string_view) override {
    }
};

DLL_EXPORT int test_parser_csiparamstruct(ITesting *t) {
    ParamCapture capture;
    Parser parser(capture);

    // ghostty: csi: SGR ESC [ 48 : 2 m (48:2:240:143:104)
    parser.Feed("\x1b[48:2:240:143:104m");
    TR_ASSERT(t, capture.lastFinal == 'm');
    TR_ASSERT(t, capture.last.count == 5);
    TR_ASSERT(t, capture.last.values[0] == 48);
    TR_ASSERT(t, capture.last.values[4] == 104);
    TR_ASSERT(t, capture.last.IsFollowedByColon(0));
    TR_ASSERT(t, capture.last.IsFollowedByColon(3));
    TR_ASSERT(t, !capture.last.IsFollowedByColon(4));

    // Missing values and defaults
    parser.Feed("\x1b[;7H");
    TR_ASSERT(t, capture.last.count == 2);
    TR_ASSERT(t, capture.last.values[0] == CsiParams::kMissing);
    TR_ASSERT(t, capture.last.Get(0, 1) == 1);
    TR_ASSERT(t, capture.last.Get(1, 1) == 7);
    TR_ASSERT(t, capture.last.Get(5, 1) == 1);

    parser.Feed("\x1b[H");
    TR_ASSERT(t, capture.last.count == 0);
    // "0" is not missing
    parser.Feed("\x1b[0J");
    TR_ASSERT(t, capture.last.count == 1);
    TR_ASSERT(t, capture.last.Get(0, 5) == 0);
    TR_ASSERT(t, parser.GetState() == Parser::State::kGround);
    return kTR_Pass;
}

DLL_EXPORT int test_parser_csimaxparams(ITesting *t) {
    // ghostty: csi: sgr with up to our max parameters
    for (size_t max = 1; max <= CsiParams::kMaxParams; max++) {
        ParamCapture capture;
        Parser parser(capture);
        std::string seq = "\x1b[";
        for (size_t i = 0; i + 1 < max; i++) {
            seq += "1;";
        }
        seq += "2H";
        parser.Feed(seq);
        TR_ASSERT(t, capture.dispatchCount == 1);
        TR_ASSERT(t, capture.last.count == max);
        TR_ASSERT(t, capture.last.values[max - 1] == 2);
    }
    // ghostty: csi: sgr beyond our max drops it
    {
        std::string seq = "\x1b[";
        for (size_t i = 0; i < CsiParams::kMaxParams; i++) {
            seq += "1;";
        }
        seq += "2H";
        EXPECT_EVENTS(t, seq, "");
    }
    // ghostty: csi: too many params - and what follows is not affected
    {
        std::string seq = "\x1b[";
        for (int i = 0; i < 100; i++) {
            seq += "1;";
        }
        seq += "1Cok";
        EXPECT_EVENTS(t, seq, "TEXT ok\n");
    }
    return kTR_Pass;
}

DLL_EXPORT int test_parser_csimalformed(ITesting *t) {
    // A private marker after parameters: the whole sequence is ignored up to its final byte
    EXPECT_EVENTS(t, "\x1b[1?2hX", "TEXT X\n");
    // A parameter byte after an intermediate
    EXPECT_EVENTS(t, "\x1b[1$2pX", "TEXT X\n");
    // Too many intermediates
    EXPECT_EVENTS(t, "\x1b[1 !\"#$pX", "TEXT X\n");
    // Bytes >= 0x80 inside a CSI are dropped, the CSI completes
    EXPECT_EVENTS(t, "\x1b[1\xC3\xA5" "2H", "CSI 12H\n");
    return kTR_Pass;
}

DLL_EXPORT int test_parser_cancel(ITesting *t) {
    // CAN / SUB abort a sequence and are themselves executed
    EXPECT_EVENTS(t, "\x1b[12\x18X", "C0 18\nTEXT X\n");
    EXPECT_EVENTS(t, "\x1b[12\x1aX", "C0 1a\nTEXT X\n");
    // An aborted OSC is not dispatched
    EXPECT_EVENTS(t, "\x1b]0;abc\x18X", "C0 18\nTEXT X\n");
    return kTR_Pass;
}

DLL_EXPORT int test_parser_osc(ITesting *t) {
    // ghostty: osc: change window title
    EXPECT_EVENTS(t, "\x1b]0;abc\x07", "OSC 0|abc\n");
    // ghostty: osc: change window title (end in esc) - the '\' of ST is not an escape of its own
    EXPECT_EVENTS(t, "\x1b]0;abc\x1b\\", "OSC 0|abc\n");
    // ghostty: osc: 112 incomplete sequence / 104 empty / 105 empty
    EXPECT_EVENTS(t, "\x1b]112\x07", "OSC 112|\n");
    EXPECT_EVENTS(t, "\x1b]104\x07", "OSC 104|\n");
    EXPECT_EVENTS(t, "\x1b]105\x07", "OSC 105|\n");
    // UTF-8 payload, including bytes that would be C1 (0x9C = ST as C1)
    EXPECT_EVENTS(t, "\x1b]2;\xE2\x9C\xB3 Claude\x07", "OSC 2|✳ Claude\n");
    // Shell integration
    EXPECT_EVENTS(t, "\x1b]133;A\x07\x1b]133;D;0\x1b\\$ ", "OSC 133|A\nOSC 133|D;0\nTEXT $ \n");
    EXPECT_EVENTS(t, "\x1b]7;file://host/tmp\x07", "OSC 7|file://host/tmp\n");
    // Hyperlink: two strings back to back
    EXPECT_EVENTS(t, "\x1b]8;;http://x\x1b\\link\x1b]8;;\x1b\\", "OSC 8|;http://x\nTEXT link\nOSC 8|;\n");
    // ESC then something other than '\' still ends the OSC; the escape after it is dispatched
    EXPECT_EVENTS(t, "\x1b]0;abc\x1b" "7", "OSC 0|abc\nESC 7\n");
    // Controls inside an OSC are dropped
    EXPECT_EVENTS(t, "\x1b]0;a\rb\x07", "OSC 0|ab\n");
    return kTR_Pass;
}

DLL_EXPORT int test_parser_oscoverflow(ITesting *t) {
    std::string seq = "\x1b]52;c;";
    seq.append(Parser::kMaxStringLength + 10, 'A');
    seq += "\x07ok\x1b]0;t\x07";
    EXPECT_EVENTS(t, seq, "TEXT ok\nOSC 0|t\n");

    // Exactly at the limit is still delivered
    std::string atLimit(Parser::kMaxStringLength, 'B');
    EventRecorder recorder;
    Parser parser(recorder);
    parser.Feed("\x1b]");
    parser.Feed(atLimit);
    parser.Feed("\x07");
    TR_ASSERT(t, recorder.Events().size() == 1);
    return kTR_Pass;
}

DLL_EXPORT int test_parser_dcs(ITesting *t) {
    // ghostty: dcs: XTGETTCAP
    {
        EventRecorder recorder;
        Parser parser(recorder);
        parser.Feed("\x1bP+q");
        TR_ASSERT(t, parser.GetState() == Parser::State::kDcsPassthrough);
        parser.Feed("544e\x1b\\");
        TR_ASSERT(t, parser.GetState() == Parser::State::kGround);
        TR_ASSERT(t, recorder.Joined() == "DCS +q|544e\n");
    }
    // ghostty: dcs: params
    EXPECT_EVENTS(t, "\x1bP1000p\x1b\\", "DCS 1000p|\n");
    // DECRQSS, with a private marker form too
    EXPECT_EVENTS(t, "\x1bP$qm\x1b\\", "DCS $q|m\n");
    EXPECT_EVENTS(t, "\x1bP>|x\x1b\\", "DCS >||x\n");
    // ghostty: dcs: too many params - dropped entirely, what follows is fine
    {
        std::string seq = "\x1bP6";
        seq.append(CsiParams::kMaxParams, ';');
        seq += "7pdata\x1b\\ok";
        EXPECT_EVENTS(t, seq, "TEXT ok\n");
    }
    // A cancelled DCS is still unhooked (the hook is always paired)
    EXPECT_EVENTS(t, "\x1bPqab\x18", "DCS q|ab\nC0 18\n");
    return kTR_Pass;
}

DLL_EXPORT int test_parser_apc(ITesting *t) {
    EXPECT_EVENTS(t, "\x1b_Gf=24;AAAA\x1b\\", "APC Gf=24;AAAA\n");
    // SOS and PM are dropped
    EXPECT_EVENTS(t, "\x1bXabc\x1b\\\x1b^def\x1b\\ok", "TEXT ok\n");
    return kTR_Pass;
}

DLL_EXPORT int test_parser_reset(ITesting *t) {
    EventRecorder recorder;
    Parser parser(recorder);
    parser.Feed("\x1b[12");
    TR_ASSERT(t, parser.GetState() == Parser::State::kCsiParam);
    parser.Reset();
    TR_ASSERT(t, parser.GetState() == Parser::State::kGround);
    parser.Feed("H");
    TR_ASSERT(t, recorder.Joined() == "TEXT H\n");
    return kTR_Pass;
}

// Every case above in one stream - the split tests run it through every cut
static const std::string kSplitCorpus =
    "abc å ▛✳😀\r\n\x1b(B\x1b" "7\x1b[1;4H\x1b[;5H\x1b[?1049;25h\x1b[>4m\x1b[?2026$p\x1b[3 q"
    "\x1b[4:3;38;2;51;51;51;48;2;170;170;170;58:2::255:97:136m\x1b]0;\xE2\x9C\xB3 title\x07"
    "\x1b]133;D;0\x1b\\\x1bP+q544e\x1b\\\x1b_Gf=24\x1b\\\x1bXsos\x1b\\\xE2\x96" "A\x1b[12\x18X\x1b]8;;u\x1b\\l";

DLL_EXPORT int test_parser_split(ITesting *t) {
    const auto whole = ParseWhole(kSplitCorpus);
    // Two pieces, cut at every offset
    for (size_t cut = 0; cut <= kSplitCorpus.size(); cut++) {
        EventRecorder recorder;
        Parser parser(recorder);
        parser.Feed(std::string_view(kSplitCorpus).substr(0, cut));
        parser.Feed(std::string_view(kSplitCorpus).substr(cut));
        if (recorder.Joined() != whole) {
            t->Error(__LINE__, __FILE__, "split at %zu differs", cut);
            return kTR_Fail;
        }
    }
    // Every chunk size
    for (size_t chunk = 1; chunk <= kSplitCorpus.size(); chunk++) {
        if (ParseChunked(kSplitCorpus, chunk) != whole) {
            t->Error(__LINE__, __FILE__, "chunk size %zu differs", chunk);
            return kTR_Fail;
        }
    }
    return kTR_Pass;
}

DLL_EXPORT int test_parser_splitfixtures(ITesting *t) {
    auto fixtures = ListFixtures();
    TR_ASSERT(t, !fixtures.empty());
    for (auto &path : fixtures) {
        auto bytes = LoadFixture(path);
        TR_ASSERT(t, !bytes.empty());
        const auto whole = ParseWhole(bytes);
        for (size_t chunk : {1, 2, 3, 7, 13, 64, 509, 4096}) {
            if (ParseChunked(bytes, chunk) != whole) {
                t->Error(__LINE__, __FILE__, "%s: chunk size %zu differs", path.c_str(), chunk);
                return kTR_Fail;
            }
        }
    }
    return kTR_Pass;
}
