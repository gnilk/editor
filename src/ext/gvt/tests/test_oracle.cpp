//
// gvt oracle — differential tests against libvterm (test-only reference, never linked into gvt).
//
// Parser level (VT-2): the same bytes go through gvt::Parser and libvterm's parser callbacks; both are
// normalized by EventRecorder into the same text lines and compared. Where libvterm and gvt disagree ON
// PURPOSE, the difference is listed in kKnownDivergences with the reason - the oracle informs, xterm's
// documented behaviour decides.
//
// Screen level: OracleScreen runs bytes through libvterm's screen and snapshots text, cursor, alt-screen flag and the
// rows pushed to scrollback; CompareScreens holds a gvt::Terminal against it (module case `terminal`).
//
#include <string>
#include <vector>
#include <cstring>

#include <vterm.h>
#include <testinterface.h>

#include "gvt/Parser.h"
#include "gvt/Terminal.h"
#include "EventRecorder.h"
#include "ScreenDump.h"
#include "Fixtures.h"

using namespace gnilk::vt;
using namespace gvttest;

extern "C" {
DLL_EXPORT int test_oracle(ITesting *t);
DLL_EXPORT int test_oracle_sequences(ITesting *t);
DLL_EXPORT int test_oracle_fixtures(ITesting *t);
DLL_EXPORT int test_oracle_screen(ITesting *t);
DLL_EXPORT int test_oracle_terminal(ITesting *t);
DLL_EXPORT int test_oracle_terminalfixtures(ITesting *t);
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
            callbacks.settermprop = OnSetTermProp;
            callbacks.sb_pushline = OnPushLine;
            vterm_screen_set_callbacks(screen, &callbacks, this);
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

        int Cols() const {
            return cols;
        }

        // Attributes and colours of one cell in a common form (see CellStyle below)
        std::string Style(int row, int col) const {
            VTermScreenCell cell;
            vterm_screen_get_cell(screen, VTermPos{row, col}, &cell);
            std::string out;
            out += cell.attrs.bold ? 'B' : '-';
            out += cell.attrs.italic ? 'I' : '-';
            out += static_cast<char>('0' + cell.attrs.underline);
            out += cell.attrs.blink ? 'K' : '-';
            out += cell.attrs.reverse ? 'R' : '-';
            out += cell.attrs.conceal ? 'H' : '-';
            out += cell.attrs.strike ? 'S' : '-';
            out += " fg=" + ColorText(cell.fg) + " bg=" + ColorText(cell.bg);
            return out;
        }

        int Rows() const {
            return rows;
        }

    public:
        bool isAltScreen = false;
        std::vector<std::string> scrolledOut;

    private:
        static std::string ColorText(const VTermColor &color) {
            if (VTERM_COLOR_IS_DEFAULT_FG(&color) || VTERM_COLOR_IS_DEFAULT_BG(&color)) {
                return "default";
            }
            if (VTERM_COLOR_IS_INDEXED(&color)) {
                return std::to_string(color.indexed.idx);
            }
            return "#" + std::to_string(color.rgb.red) + "," + std::to_string(color.rgb.green) + "," +
                   std::to_string(color.rgb.blue);
        }

        static int OnSetTermProp(VTermProp prop, VTermValue *value, void *user) {
            if (prop == VTERM_PROP_ALTSCREEN) {
                static_cast<OracleScreen *>(user)->isAltScreen = value->boolean;
            }
            return 1;
        }

        static int OnPushLine(int width, const VTermScreenCell *cells, void *user) {
            std::string out;
            for (int col = 0; col < width; col++) {
                if (cells[col].chars[0] == kWideContinuation) {
                    continue;
                }
                if (cells[col].chars[0] == 0) {
                    out.push_back(' ');
                    continue;
                }
                for (int i = 0; (i < VTERM_MAX_CHARS_PER_CELL) && (cells[col].chars[i] != 0); i++) {
                    AppendUtf8(out, cells[col].chars[i]);
                }
            }
            while (!out.empty() && (out.back() == ' ')) {
                out.pop_back();
            }
            static_cast<OracleScreen *>(user)->scrolledOut.push_back(out);
            return 1;
        }

    private:
        // libvterm's marker in the cell right of a wide character
        static constexpr uint32_t kWideContinuation = 0xFFFFFFFF;

    private:
        int cols;
        int rows;
        VTerm *vt = nullptr;
        VTermScreen *screen = nullptr;
        VTermScreenCallbacks callbacks = {};
    };

    // A gvt cell in OracleScreen::Style's form. libvterm has no dim, overline or dotted/dashed underline: dim and
    // overline are left out, dotted/dashed compare as single (libvterm's reading of 4:4 / 4:5).
    std::string CellStyle(const Cell &cell) {
        auto color = [](const Color &c) -> std::string {
            switch (c.kind) {
                case Color::Kind::kDefault:
                    return "default";
                case Color::Kind::kIndexed:
                    return std::to_string(c.index);
                case Color::Kind::kRgb:
                    return "#" + std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b);
            }
            return "?";
        };
        int underline = 0;
        switch (cell.underlineStyle) {
            case UnderlineStyle::kNone:
                underline = 0;
                break;
            case UnderlineStyle::kDouble:
                underline = 2;
                break;
            case UnderlineStyle::kCurly:
                underline = 3;
                break;
            default:
                underline = 1;
                break;
        }
        std::string out;
        out += (cell.attrs & kAttrBold) ? 'B' : '-';
        out += (cell.attrs & kAttrItalic) ? 'I' : '-';
        out += static_cast<char>('0' + underline);
        out += (cell.attrs & kAttrBlink) ? 'K' : '-';
        out += (cell.attrs & kAttrInverse) ? 'R' : '-';
        out += (cell.attrs & kAttrHidden) ? 'H' : '-';
        out += (cell.attrs & kAttrStrike) ? 'S' : '-';
        out += " fg=" + color(cell.fg) + " bg=" + color(cell.bg);
        return out;
    }

    // gvt::Terminal vs libvterm's screen after the same bytes: every row's text, every cell's attributes and colours,
    // the cursor, the alt-screen flag and the rows that went to scrollback. Reports the first difference.
    bool CompareScreens(ITesting *t, const char *name, std::string_view bytes, int cols, int rows) {
        RecordingHost host;
        Terminal ours(host, cols, rows);
        ours.Feed(bytes);
        OracleScreen theirs(cols, rows);
        theirs.Feed(bytes);

        for (int y = 0; y < rows; y++) {
            auto ourRow = RowText(ours, y);
            auto theirRow = theirs.RowText(y);
            if (ourRow == theirRow) {
                continue;
            }
            std::string context;
            for (int i = std::max(0, y - 2); i <= std::min(rows - 1, y + 2); i++) {
                context += "\n  [" + std::to_string(i) + "] gvt:      '" + RowText(ours, i) + "'";
                context += "\n  [" + std::to_string(i) + "] libvterm: '" + theirs.RowText(i) + "'";
            }
            t->Error(__LINE__, __FILE__, "%s: row %d differs%s", name, y, context.c_str());
            return false;
        }
        for (int y = 0; y < rows; y++) {
            for (int x = 0; x < cols; x++) {
                auto ourStyle = CellStyle(ours.At(x, y));
                auto theirStyle = theirs.Style(y, x);
                if (ourStyle != theirStyle) {
                    t->Error(__LINE__, __FILE__, "%s: cell (%d,%d) '%s'\n  gvt:      %s\n  libvterm: %s", name, x, y,
                             RowText(ours, y).c_str(), ourStyle.c_str(), theirStyle.c_str());
                    return false;
                }
            }
        }
        auto cursor = ours.GetCursor();
        auto theirCursor = theirs.Cursor();
        if ((cursor.x != theirCursor.col) || (cursor.y != theirCursor.row)) {
            t->Error(__LINE__, __FILE__, "%s: cursor gvt (%d,%d) libvterm (%d,%d)", name, cursor.x, cursor.y,
                     theirCursor.col, theirCursor.row);
            return false;
        }
        if (ours.IsAltScreen() != theirs.isAltScreen) {
            t->Error(__LINE__, __FILE__, "%s: alt screen gvt %d libvterm %d", name, ours.IsAltScreen(),
                     theirs.isAltScreen);
            return false;
        }
        if (host.scrolledOut != theirs.scrolledOut) {
            std::string context;
            size_t n = std::max(host.scrolledOut.size(), theirs.scrolledOut.size());
            for (size_t i = 0; i < n; i++) {
                auto a = (i < host.scrolledOut.size()) ? host.scrolledOut[i] : std::string("<none>");
                auto b = (i < theirs.scrolledOut.size()) ? theirs.scrolledOut[i] : std::string("<none>");
                if (a != b) {
                    context = "\n  [" + std::to_string(i) + "] gvt: '" + a + "'\n  [" + std::to_string(i) +
                              "] libvterm: '" + b + "'";
                    break;
                }
            }
            t->Error(__LINE__, __FILE__, "%s: scrollback gvt %zu rows, libvterm %zu%s", name,
                     host.scrolledOut.size(), theirs.scrolledOut.size(), context.c_str());
            return false;
        }
        return true;
    }

    // Hand-built screen cases, one VT-3 concern each, compared with libvterm
    struct ScreenCase {
        const char *name;
        int cols;
        int rows;
        const char *bytes;
    };
    const ScreenCase kScreenCases[] = {
        {"print-wrap", 10, 4, "0123456789\r\nX\r\n0123456789AB"},
        {"nowrap", 10, 3, "\x1b[?7l0123456789AB\x1b[?7h\r\nabc"},
        {"bs-pendingwrap", 10, 3, "0123456789\x08X"},
        {"bs-cr-lf", 10, 3, "abc\x08\x08X\nY\rZ\r\x08W"},
        {"cursor", 20, 10, "\x1b[5;10Ha\x1b[2Ab\x1b[Bc\x1b[3Cd\x1b[0De\x1b[2Ef\x1b[4G\x1b[Fg\x1b[7`\x1b[9dh"
                           "\x1b[2a\x1b[ei\x1b[;5Hj\x1b[3Hk\x1b[99;99Hl\x1b[99A\x1b[99Dm"},
        {"erase-ed", 6, 4, "AAAAAA\r\nBBBBBB\r\nCCCCCC\r\nDDDDDD\x1b[2;3H\x1b[J"},
        {"erase-ed1", 6, 4, "AAAAAA\r\nBBBBBB\r\nCCCCCC\r\nDDDDDD\x1b[2;3H\x1b[1J"},
        {"erase-el", 6, 4, "AAAAAA\r\nBBBBBB\r\nCCCCCC\r\nDDDDDD\x1b[1;3H\x1b[K\x1b[2;3H\x1b[1K\x1b[3;3H\x1b[2K"
                           "\x1b[4;2H\x1b[3X\x1b[4;5H\x1b[99X"},
        {"ich-dch", 6, 4, "abcdef\x1b[1;3H\x1b[2@\r\nabcdef\x1b[2;2H\x1b[3P\r\nabcdef\x1b[3;2H\x1b[99P"},
        {"il-dl", 6, 5, "1\r\n2\r\n3\r\n4\r\n5\x1b[2;4H\x1b[L\x1b[4;1H\x1b[2M"},
        {"il-dl-region", 6, 5, "1\r\n2\r\n3\r\n4\r\n5\x1b[2;4r\x1b[5;1H\x1b[L\x1b[1;1H\x1b[M\x1b[2;1H\x1b[L"},
        {"region-lf", 5, 5, "1\r\n2\r\n3\r\n4\r\n5\x1b[2;4r\x1b[4;1H\nX\x1b[2;1H\x1bMY"},
        {"region-su-sd", 5, 5, "1\r\n2\r\n3\r\n4\r\n5\x1b[2;4r\x1b[S\x1b[2T"},
        {"region-cud-cuu", 5, 5, "\x1b[2;4r\x1b[3;1H\x1b[9Ba\x1b[5;1H\x1b[9Ab\x1b[1;1H\x1b[9Bc"},
        {"region-below", 5, 5, "\x1b[2;3r\x1b[5;1H\n\nZ"},
        {"scroll-out", 4, 3, "a\r\nb\r\nc\r\nd\r\nabcdefg\r\n\r\n"},
        {"scroll-region-top", 4, 4, "a\r\nb\r\nc\r\nd\x1b[1;3r\x1b[3;1H\n\n"},
        {"scroll-su", 4, 3, "a\r\nb\r\nc\x1b[2S"},
        {"tabs", 20, 2, "a\tb\tc\t\t\tX\r\n\x1b[3g\x1b[5G\x1bH\r\tY\tZ"},
        {"cht-cbt", 30, 2, "\x1b[2Ia\x1b[Zb\x1b[9Zc"},
        {"rep", 10, 2, "-\x1b[4b|\x1b[9b"},
        {"decsc", 10, 5, "\x1b[3;4H\x1b" "7\x1b[H\x1b" "8X"},
        {"decsc-none", 10, 5, "\x1b[3;3H\x1b" "8Z"},
        {"scosc", 10, 5, "\x1b[2;2H\x1b[s\x1b[5;5H\x1b[uY"},
        {"decom", 10, 6, "\x1b[2;5r\x1b[?6h\x1b[2;3HA\x1b[99;1HB\x1b[?6l\x1b[1;1HC"},
        {"alt-1049", 10, 4, "shell$ \x1b[?1049h\x1b[HVIM\x1b[4;1H~\x1b[?1049l"},
        {"alt-1049-inside", 10, 4, "shell$ \x1b[?1049h\x1b[HVIM"},
        {"alt-47", 10, 4, "shell$ \x1b[?47hALT\x1b[?47l"},
        {"alt-1047", 10, 4, "shell$ \x1b[?1047hALT\x1b[?1047l"},
        {"charset", 10, 3, "\x1b(0lqk\x1b(Bx\r\n\x1b)0a\x0eqx\x0fq\r\n\x1b(0AZ~\x1b(B"},
        {"irm", 6, 2, "abcdef\r\x1b[4hXY\x1b[4lZ"},
        {"lnm", 6, 3, "\x1b[20ha\nb"},
        {"decaln", 4, 3, "\x1b#8\x1b[2;2Hx"},
        {"ris-primary", 6, 3, "abc\x1b[2;3r\x1b[?7l\x1b(0q\x1b" "cq\r\n\n\nx"},
        {"ris", 6, 3, "abc\x1b[2;3r\x1b[?7l\x1b(0\x1b[?1049hq\x1b" "cq\r\n\n\nx"},
        {"sgr-attrs", 20, 2, "\x1b[1;3;4;5;7;8;9mA\x1b[22;23;24;25;27;28;29mB\x1b[1;21mC\x1b[22;24mD"},
        {"sgr-16", 20, 2, "\x1b[30;47mA\x1b[97;100mB\x1b[39mC\x1b[49mD\x1b[31;42m\x1b[mE"},
        {"sgr-256", 20, 2, "\x1b[38;5;208;48;5;17mA\x1b[38;5;3mB\x1b[0m"},
        {"sgr-rgb", 20, 2, "\x1b[38;2;255;193;7mA\x1b[48;2;1;2;3mB\x1b[0m"},
        {"sgr-colon-t416", 20, 2, "\x1b[38:2::255:193:7mA\x1b[0m"},
        {"sgr-colon", 20, 2, "\x1b[38:2:255:193:7mA\x1b[38:5:17mB\x1b[4:3mC\x1b[4:0mD\x1b[4:2mE\x1b[0m"},
        {"sgr-kakoune", 20, 2, "\x1b[;4:3;38;2;175;175;215;58:2::190:80:70mK\x1b[0m"},
        {"sgr-mixed", 20, 2, "\x1b[1;38;5;208;48;2;1;2;3;4mE\x1b[0;38:5:1;44mM"},
        {"sgr-erase", 6, 3, "abcdef\x1b[1;4;31;42m\x1b[1;3H\x1b[K\x1b[2;1H\x1b[2X\x1b[3;1H\n"},
        {"sgr-saved", 20, 2, "\x1b[1;35m\x1b" "7\x1b[0m\x1b" "8K"},
        {"decstr", 6, 3, "\x1b[2;3r\x1b[?7l\x1b[!p\x1b[3;1H\nabcdefgh"},
    };

    // Screen-level differences from libvterm that are on purpose, or not implemented yet
    struct ScreenDivergence {
        const char *name;
        const char *reason;
    };
    const ScreenDivergence kScreenDivergences[] = {
        // VT510 / xterm (CursorUp/CursorDown): CUU stops at the top margin unless the cursor is already above it,
        // CUD at the bottom margin unless already below it. libvterm clamps only when the cursor is inside the region.
        {"region-cud-cuu", "CUU/CUD from outside the region stop at the margin (VT510, xterm)"},
        // xterm prints REP through the normal path, so it wraps; libvterm stops at the end of the line
        {"rep", "REP wraps like printing (xterm)"},
        // SCOSC / SCORC (CSI s / CSI u) are xterm's; libvterm has neither
        {"scosc", "CSI s / CSI u save and restore the cursor (xterm)"},
        // Mode 47 switches to the alternate screen in xterm; libvterm only knows 1047/1049
        {"alt-47", "DECSET 47 switches screens (xterm)"},
        // RIS returns to the primary screen in xterm and Ghostty; libvterm stays on the alternate one
        {"ris", "RIS leaves the alternate screen (xterm, Ghostty)"},
        // DECRC with nothing saved: xterm restores the default pen; libvterm restores a zeroed one (black on black)
        {"decsc-none", "ESC 8 without a save restores default colours (xterm)"},
        // "38:2:<colorspace>:r:g:b" (ITU T.416, Ghostty, xterm): libvterm has no colorspace slot and reads the
        // channels one off (r = 255 for the empty slot, g = 255, b = 193)
        {"sgr-colon-t416", "38:2::r:g:b skips the colorspace slot (T.416)"},
        // Erase (BCE) fills with the pen's background only, as xterm and Ghostty; libvterm also keeps the
        // foreground on the blank cells (invisible there)
        {"sgr-erase", "erased cells take the background only (xterm, Ghostty)"},
        // Wide characters take two cells - VT-6. Remove when the width table lands.
        {"dialog_menu.bin", "wide characters (VT-6)"},
    };

    bool IsScreenDivergence(std::string_view name) {
        for (auto &d : kScreenDivergences) {
            if ((d.name != nullptr) && (name == d.name)) {
                return true;
            }
        }
        return false;
    }

    // The size each fixture was recorded at (tests/fixtures/README.md)
    std::pair<int, int> FixtureSize(const std::string &filename) {
        if (filename == "git_log_graph.bin") {
            return {100, 30};
        }
        if (filename == "cat_utf8.bin") {
            return {40, 10};
        }
        return {80, 24};
    }

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

DLL_EXPORT int test_oracle_terminal(ITesting *t) {
    int failures = 0;
    for (auto &c : kScreenCases) {
        if (IsScreenDivergence(c.name)) {
            continue;
        }
        if (!CompareScreens(t, c.name, c.bytes, c.cols, c.rows)) {
            failures++;
        }
    }
    TR_ASSERT(t, failures == 0);
    return kTR_Pass;
}

DLL_EXPORT int test_oracle_terminalfixtures(ITesting *t) {
    auto fixtures = ListFixtures();
    TR_ASSERT(t, !fixtures.empty());
    int failures = 0;
    for (auto &path : fixtures) {
        auto filename = path.filename().string();
        if (IsScreenDivergence(filename)) {
            continue;
        }
        auto [cols, rows] = FixtureSize(filename);
        if (!CompareScreens(t, filename.c_str(), LoadFixture(path), cols, rows)) {
            failures++;
        }
    }
    TR_ASSERT(t, failures == 0);
    return kTR_Pass;
}
