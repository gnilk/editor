//
// gvt oracle — differential tests against libvterm (test-only reference, never linked into gvt).
//
// Parser level (VT-2): the same bytes go through gvt::Parser and libvterm's parser callbacks; both are
// normalized by EventRecorder into the same text lines and compared. Where libvterm and gvt disagree ON
// PURPOSE, the difference is listed in kKnownDivergences with the reason - the oracle informs, xterm's
// documented behaviour decides.
//
// Screen level: OracleScreen runs bytes through libvterm's screen and snapshots text + cursor. It is the
// reference gvt::Terminal (VT-3) will be compared against; until then it is checked on its own here.
//
#include <string>
#include <vector>
#include <cstring>

#include <vterm.h>
#include <testinterface.h>

#include "gvt/Parser.h"
#include "EventRecorder.h"
#include "Fixtures.h"

using namespace gnilk::vt;
using namespace gvttest;

extern "C" {
DLL_EXPORT int test_oracle(ITesting *t);
DLL_EXPORT int test_oracle_sequences(ITesting *t);
DLL_EXPORT int test_oracle_fixtures(ITesting *t);
DLL_EXPORT int test_oracle_screen(ITesting *t);
}

namespace {

    // libvterm's parser layer, recorded in EventRecorder's normal form
    class LibVTermParserRecorder {
    public:
        LibVTermParserRecorder() {
            vt = vterm_new(24, 80);
            vterm_set_utf8(vt, 1);
            callbacks.text = OnText;
            callbacks.control = OnControl;
            callbacks.escape = OnEscape;
            callbacks.csi = OnCsi;
            callbacks.osc = OnOsc;
            callbacks.dcs = OnDcs;
            callbacks.apc = OnApc;
            vterm_parser_set_callbacks(vt, &callbacks, this);
        }

        ~LibVTermParserRecorder() {
            vterm_free(vt);
        }

        std::string Parse(std::string_view bytes) {
            vterm_input_write(vt, bytes.data(), bytes.size());
            return recorder.Joined();
        }

    private:
        static LibVTermParserRecorder &Self(void *user) {
            return *static_cast<LibVTermParserRecorder *>(user);
        }

        // libvterm hands over the rest of the buffer; the callback takes the printable run at its start and
        // returns its length (bytes >= 0x80 are UTF-8 text, as in gvt)
        static int OnText(const char *bytes, size_t len, void *user) {
            size_t n = 0;
            while (n < len) {
                auto c = static_cast<unsigned char>(bytes[n]);
                if ((c < 0x20) || (c == 0x7F)) {
                    break;
                }
                n++;
            }
            Self(user).recorder.AddText(std::string_view(bytes, n));
            return static_cast<int>(n);
        }

        // libvterm reports a 7-bit C1 escape (ESC 0x40-0x5F, e.g. ESC M) as the C1 control (0x8D);
        // gvt dispatches it as the escape it is. Mapped back here.
        static int OnControl(unsigned char control, void *user) {
            if (control >= 0x80) {
                std::string out = "ESC ";
                out.push_back(static_cast<char>(control - 0x40));
                Self(user).recorder.Add(out);
                return 1;
            }
            Self(user).recorder.Execute(control);
            return 1;
        }

        static int OnEscape(const char *bytes, size_t len, void *user) {
            Self(user).recorder.Add("ESC " + std::string(bytes, len));
            return 1;
        }

        static int OnCsi(const char *leader, const long args[], int argcount, const char *intermed, char command,
                         void *user) {
            std::string out = "CSI ";
            if (leader != nullptr) {
                out += leader;
            }
            // libvterm reports "CSI H" as one missing argument; gvt as none. Both print as nothing.
            bool isSingleMissing = (argcount == 1) && CSI_ARG_IS_MISSING(args[0]);
            for (int i = 0; !isSingleMissing && (i < argcount); i++) {
                if (!CSI_ARG_IS_MISSING(args[i])) {
                    out += std::to_string(CSI_ARG(args[i]));
                }
                if (i + 1 < argcount) {
                    out += CSI_ARG_HAS_MORE(args[i]) ? ":" : ";";
                }
            }
            if (intermed != nullptr) {
                out += intermed;
            }
            out.push_back(command);
            Self(user).recorder.Add(out);
            return 1;
        }

        static int OnOsc(int command, VTermStringFragment frag, void *user) {
            auto &self = Self(user);
            if (frag.initial) {
                self.stringData.clear();
            }
            self.stringData.append(frag.str, frag.len);
            if (frag.final) {
                std::string number = (command >= 0) ? std::to_string(command) : std::string();
                self.recorder.Add("OSC " + number + "|" + self.stringData);
            }
            return 1;
        }

        static int OnDcs(const char *command, size_t commandlen, VTermStringFragment frag, void *user) {
            auto &self = Self(user);
            if (frag.initial) {
                self.stringData.clear();
            }
            self.stringData.append(frag.str, frag.len);
            if (frag.final) {
                self.recorder.Add("DCS " + std::string(command, commandlen) + "|" + self.stringData);
            }
            return 1;
        }

        static int OnApc(VTermStringFragment frag, void *user) {
            auto &self = Self(user);
            if (frag.initial) {
                self.stringData.clear();
            }
            self.stringData.append(frag.str, frag.len);
            if (frag.final) {
                self.recorder.Add("APC " + self.stringData);
            }
            return 1;
        }

    private:
        VTerm *vt = nullptr;
        VTermParserCallbacks callbacks = {};
        EventRecorder recorder;
        std::string stringData;
    };

    std::string ParseWithGvt(std::string_view bytes) {
        EventRecorder recorder;
        // libvterm drops NUL/CAN/SUB unless asked; gvt hands them on (CAN/SUB cancel - the screen may show it)
        recorder.isNulCanSubDropped = true;
        Parser parser(recorder);
        parser.Feed(bytes);
        return recorder.Joined();
    }

    std::vector<std::string> SplitLines(const std::string &text) {
        std::vector<std::string> lines;
        size_t start = 0;
        while (start < text.size()) {
            auto end = text.find('\n', start);
            if (end == std::string::npos) {
                end = text.size();
            }
            lines.push_back(text.substr(start, end - start));
            start = end + 1;
        }
        return lines;
    }

    std::string Visible(std::string_view text) {
        std::string out;
        for (unsigned char c : text) {
            if ((c < 0x20) || (c == 0x7F)) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\x%02x", c);
                out += buf;
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
        return out;
    }

    // Differences that are deliberate: input -> why. Matched on the input sequence of a hand-built case.
    struct KnownDivergence {
        const char *input;
        const char *reason;
    };
    const KnownDivergence kKnownDivergences[] = {
        // A malformed CSI (a private marker after a parameter, a parameter after an intermediate): Williams'
        // csi_ignore swallows everything up to the final byte, as xterm, Ghostty and Alacritty's vte do.
        // libvterm abandons the sequence at the bad byte and prints the rest ("2h", "p") as text.
        {"\x1b[1?2hX\x1b[1$2pY", "malformed CSI: ignored up to its final byte (Williams csi_ignore)"},
    };

    bool IsKnownDivergence(std::string_view input) {
        for (auto &d : kKnownDivergences) {
            if ((d.input != nullptr) && (input == d.input)) {
                return true;
            }
        }
        return false;
    }

    // Compares the two event lists; on a difference reports the first differing line with some context
    bool CompareWithOracle(ITesting *t, const char *name, std::string_view bytes) {
        auto ours = SplitLines(ParseWithGvt(bytes));
        LibVTermParserRecorder oracle;
        auto theirs = SplitLines(oracle.Parse(bytes));
        if (ours == theirs) {
            return true;
        }
        size_t idx = 0;
        while ((idx < ours.size()) && (idx < theirs.size()) && (ours[idx] == theirs[idx])) {
            idx++;
        }
        std::string context;
        size_t from = (idx >= 2) ? idx - 2 : 0;
        for (size_t i = from; i < idx + 3; i++) {
            context += "\n  [" + std::to_string(i) + "] gvt:      " + ((i < ours.size()) ? Visible(ours[i]) : "<end>");
            context += "\n  [" + std::to_string(i) + "] libvterm: " + ((i < theirs.size()) ? Visible(theirs[i]) : "<end>");
        }
        t->Error(__LINE__, __FILE__, "%s: event %zu differs (gvt %zu events, libvterm %zu)%s", name, idx, ours.size(),
                 theirs.size(), context.c_str());
        return false;
    }

    // libvterm's screen layer: what a real terminal would show after the bytes
    class OracleScreen {
    public:
        OracleScreen(int cols, int rows) : cols(cols), rows(rows) {
            vt = vterm_new(rows, cols);
            vterm_set_utf8(vt, 1);
            screen = vterm_obtain_screen(vt);
            vterm_screen_enable_altscreen(screen, 1);
            vterm_screen_reset(screen, 1);
        }

        ~OracleScreen() {
            vterm_free(vt);
        }

        void Feed(std::string_view bytes) {
            vterm_input_write(vt, bytes.data(), bytes.size());
            vterm_screen_flush_damage(screen);
        }

        // Row text, UTF-8, trailing blanks trimmed. A wide character's second cell contributes nothing.
        std::string RowText(int row) const {
            std::string out;
            for (int col = 0; col < cols; col++) {
                VTermScreenCell cell;
                vterm_screen_get_cell(screen, VTermPos{row, col}, &cell);
                // The right half of a wide character
                if (cell.chars[0] == kWideContinuation) {
                    continue;
                }
                if (cell.chars[0] == 0) {
                    out.push_back(' ');
                    continue;
                }
                for (int i = 0; (i < VTERM_MAX_CHARS_PER_CELL) && (cell.chars[i] != 0); i++) {
                    AppendUtf8(out, cell.chars[i]);
                }
            }
            while (!out.empty() && (out.back() == ' ')) {
                out.pop_back();
            }
            return out;
        }

        VTermPos Cursor() const {
            VTermPos pos;
            vterm_state_get_cursorpos(vterm_obtain_state(vt), &pos);
            return pos;
        }

    private:
        // libvterm's marker in the cell right of a wide character
        static constexpr uint32_t kWideContinuation = 0xFFFFFFFF;

    private:
        int cols;
        int rows;
        VTerm *vt = nullptr;
        VTermScreen *screen = nullptr;
    };

    // Hand-built sequences, one concern each. The parser half of terminal-enhancements.md §4.B lives here.
    struct OracleCase {
        const char *name;
        const char *bytes;
    };
    const OracleCase kOracleCases[] = {
        {"print", "abc def"},
        {"utf8", "å ▛ ✳ 😀 中文"},
        {"utf8-c1-lookalikes", "\xE2\x96\x9B\xE2\x9C\xB3"},
        {"c0", "a\r\nb\x07\x08\tc"},
        {"esc-charset", "\x1b(B\x1b)0\x1b#8"},
        {"esc-simple", "\x1b" "7\x1b" "8\x1bM\x1b=\x1b>\x1b" "c"},
        {"csi-basic", "\x1b[H\x1b[1;4H\x1b[;5H\x1b[5;H\x1b[2J\x1b[K"},
        {"csi-moves", "\x1b[3A\x1b[B\x1b[10C\x1b[2D\x1b[5G\x1b[7d\x1b[2E\x1b[F\x1b[4X\x1b[2S\x1b[T\x1b[3b\x1b[Z"},
        {"csi-private", "\x1b[?1049;25h\x1b[?1049l\x1b[?2004h\x1b[?1000;1006h"},
        {"csi-prefix", "\x1b[>4m\x1b[?u\x1b[<u\x1b[>1u\x1b[>c\x1b[>0q"},
        {"csi-intermediate", "\x1b[?2026$p\x1b[3 q\x1b[!p"},
        {"sgr-256", "\x1b[38;5;208m\x1b[48;5;17mX\x1b[0m"},
        {"sgr-truecolor", "\x1b[38;2;255;193;7mY\x1b[48;2;1;2;3m"},
        {"sgr-colon", "\x1b[38:2::255:193:7m\x1b[4:3m\x1b[58:2::240:143:104m"},
        {"sgr-kakoune", "\x1b[;4:3;38;2;175;175;215;58:2::190:80:70m"},
        {"csi-c0-inside", "\x1b[1\r2H"},
        {"csi-esc-restart", "\x1b[12\x1b[3H"},
        {"csi-malformed", "\x1b[1?2hX\x1b[1$2pY"},
        {"cancel", "\x1b[12\x18X\x1b[3\x1aY"},
        {"osc-bel", "\x1b]0;title\x07"},
        {"osc-st", "\x1b]2;title\x1b\\"},
        {"osc-utf8", "\x1b]2;\xE2\x9C\xB3 Claude\x07"},
        {"osc-133", "\x1b]133;A\x07$ \x1b]133;B\x07ls\r\n\x1b]133;C\x07out\r\n\x1b]133;D;0\x1b\\"},
        {"osc-7", "\x1b]7;file://host/tmp\x07"},
        {"osc-8", "\x1b]8;;http://x\x1b\\link\x1b]8;;\x1b\\"},
        {"osc-query", "\x1b]10;?\x07\x1b]11;?\x1b\\"},
        {"dcs-xtgettcap", "\x1bP+q544e\x1b\\"},
        {"dcs-decrqss", "\x1bP$qm\x1b\\"},
        {"apc", "\x1b_Gf=24;AAAA\x1b\\"},
        {"sos-pm", "\x1bXabc\x1b\\\x1b^def\x1b\\ok"},
    };

}

DLL_EXPORT int test_oracle(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_oracle_sequences(ITesting *t) {
    int failures = 0;
    for (auto &c : kOracleCases) {
        if (IsKnownDivergence(c.bytes)) {
            continue;
        }
        if (!CompareWithOracle(t, c.name, c.bytes)) {
            failures++;
        }
    }
    TR_ASSERT(t, failures == 0);
    return kTR_Pass;
}

DLL_EXPORT int test_oracle_fixtures(ITesting *t) {
    auto fixtures = ListFixtures();
    TR_ASSERT(t, !fixtures.empty());
    int failures = 0;
    for (auto &path : fixtures) {
        auto bytes = LoadFixture(path);
        if (!CompareWithOracle(t, path.filename().c_str(), bytes)) {
            failures++;
        }
    }
    TR_ASSERT(t, failures == 0);
    return kTR_Pass;
}

// The screen reference works: known content lands where a terminal puts it
DLL_EXPORT int test_oracle_screen(ITesting *t) {
    {
        OracleScreen screen(40, 10);
        screen.Feed(LoadFixture(kFixtureDir / "cat_utf8.bin"));
        TR_ASSERT(t, screen.RowText(0) == "hello");
        TR_ASSERT(t, screen.RowText(1) == "wörld ▛ 😀 中文");
        TR_ASSERT(t, screen.Cursor().row == 2);
        TR_ASSERT(t, screen.Cursor().col == 0);
    }
    {
        // Pending wrap: a full-width line followed by CR LF advances exactly one row (old doc §4.C.1)
        OracleScreen screen(10, 5);
        screen.Feed("0123456789\r\nX");
        TR_ASSERT(t, screen.RowText(0) == "0123456789");
        TR_ASSERT(t, screen.RowText(1) == "X");
    }
    {
        // CHA puts words apart without spaces (old doc §4.B.3)
        OracleScreen screen(20, 3);
        screen.Feed("Quick\x1b[7Gsafety\x1b[14Gcheck");
        TR_ASSERT(t, screen.RowText(0) == "Quick safety check");
    }
    return kTR_Pass;
}
