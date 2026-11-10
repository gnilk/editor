//
// gvt Terminal — screen model unit tests (VT-3). Expected results are xterm's (ctlseqs); the oracle module checks the
// same ground against libvterm.
//
#include <testinterface.h>

#include "gvt/Terminal.h"
#include "gvt/CharWidth.h"
#include "ScreenDump.h"
#include "Fixtures.h"

using namespace gnilk::vt;
using namespace gvttest;

extern "C" {
DLL_EXPORT int test_terminal(ITesting *t);
DLL_EXPORT int test_terminal_print(ITesting *t);
DLL_EXPORT int test_terminal_pendingwrap(ITesting *t);
DLL_EXPORT int test_terminal_nowrap(ITesting *t);
DLL_EXPORT int test_terminal_c0(ITesting *t);
DLL_EXPORT int test_terminal_cursor(ITesting *t);
DLL_EXPORT int test_terminal_erase(ITesting *t);
DLL_EXPORT int test_terminal_insdel(ITesting *t);
DLL_EXPORT int test_terminal_scrollregion(ITesting *t);
DLL_EXPORT int test_terminal_scrollout(ITesting *t);
DLL_EXPORT int test_terminal_tabs(ITesting *t);
DLL_EXPORT int test_terminal_rep(ITesting *t);
DLL_EXPORT int test_terminal_savecursor(ITesting *t);
DLL_EXPORT int test_terminal_originmode(ITesting *t);
DLL_EXPORT int test_terminal_altscreen(ITesting *t);
DLL_EXPORT int test_terminal_charset(ITesting *t);
DLL_EXPORT int test_terminal_insertmode(ITesting *t);
DLL_EXPORT int test_terminal_resize(ITesting *t);
DLL_EXPORT int test_terminal_reset(ITesting *t);
DLL_EXPORT int test_terminal_split(ITesting *t);
DLL_EXPORT int test_terminal_sgr(ITesting *t);
DLL_EXPORT int test_terminal_sgrcolor(ITesting *t);
DLL_EXPORT int test_terminal_sgrcolon(ITesting *t);
DLL_EXPORT int test_terminal_sgrerase(ITesting *t);
DLL_EXPORT int test_terminal_replies(ITesting *t);
DLL_EXPORT int test_terminal_decrqm(ITesting *t);
DLL_EXPORT int test_terminal_modes(ITesting *t);
DLL_EXPORT int test_terminal_osc(ITesting *t);
DLL_EXPORT int test_terminal_osccolor(ITesting *t);
DLL_EXPORT int test_terminal_charwidth(ITesting *t);
DLL_EXPORT int test_terminal_wide(ITesting *t);
DLL_EXPORT int test_terminal_widehalves(ITesting *t);
DLL_EXPORT int test_terminal_combining(ITesting *t);
}

DLL_EXPORT int test_terminal(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_print(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 10, 3);
    term.Feed("abc");
    TR_ASSERT(t, RowText(term, 0) == "abc");
    TR_ASSERT(t, term.GetCursor().x == 3);
    TR_ASSERT(t, term.GetCursor().y == 0);
    term.Feed("\r\nå中");
    TR_ASSERT(t, RowText(term, 1) == "å中");
    TR_ASSERT(t, term.GetGeneration() == 2);
    return kTR_Pass;
}

// Writing the last column does not wrap yet; CR LF afterwards advances exactly one row (TE-3)
DLL_EXPORT int test_terminal_pendingwrap(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 10, 5);
    term.Feed("0123456789");
    TR_ASSERT(t, term.GetCursor().x == 9);
    TR_ASSERT(t, term.GetCursor().y == 0);
    TR_ASSERT(t, term.GetCursor().isPendingWrap);
    term.Feed("\r\nX");
    TR_ASSERT(t, ScreenText(term) == "0123456789\nX");
    TR_ASSERT(t, !term.IsRowWrapped(0));

    // The next printable wraps first and marks the row as soft-wrapped
    term.Feed("\r\n0123456789AB");
    TR_ASSERT(t, RowText(term, 2) == "0123456789");
    TR_ASSERT(t, RowText(term, 3) == "AB");
    TR_ASSERT(t, term.IsRowWrapped(2));

    // A cursor move cancels the pending wrap
    term.Feed("\x1b[H0123456789\x1b[5GZ");
    TR_ASSERT(t, RowText(term, 0) == "0123Z56789");
    TR_ASSERT(t, RowText(term, 1) == "X");
    return kTR_Pass;
}

// DECAWM off: the last column is overwritten
DLL_EXPORT int test_terminal_nowrap(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 10, 3);
    term.Feed("\x1b[?7l0123456789AB");
    TR_ASSERT(t, ScreenText(term) == "012345678B");
    TR_ASSERT(t, !term.GetModes().isAutoWrap);
    term.Feed("\x1b[?7h\rabcdefghijKL");
    TR_ASSERT(t, ScreenText(term) == "abcdefghij\nKL");
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_c0(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 10, 3);
    term.Feed("abc\x08\x08X\x07");
    TR_ASSERT(t, RowText(term, 0) == "aXc");
    TR_ASSERT(t, host.bells == 1);
    // LF keeps the column, CR returns
    term.Feed("\nY\rZ");
    TR_ASSERT(t, RowText(term, 1) == "Z Y");
    // BS at column 0 stays
    term.Feed("\r\x08\x08W");
    TR_ASSERT(t, RowText(term, 1) == "W Y");
    // LNM: LF also returns the carriage
    term.Feed("\x1b[20h\nQ");
    TR_ASSERT(t, RowText(term, 2) == "Q");
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_cursor(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 20, 10);
    auto at = [&](int x, int y) {
        return (term.GetCursor().x == x) && (term.GetCursor().y == y);
    };
    term.Feed("\x1b[5;10H");
    TR_ASSERT(t, at(9, 4));
    term.Feed("\x1b[2A");
    TR_ASSERT(t, at(9, 2));
    term.Feed("\x1b[B");
    TR_ASSERT(t, at(9, 3));
    term.Feed("\x1b[3C");
    TR_ASSERT(t, at(12, 3));
    term.Feed("\x1b[0D");               // 0 counts as 1
    TR_ASSERT(t, at(11, 3));
    term.Feed("\x1b[2E");
    TR_ASSERT(t, at(0, 5));
    term.Feed("\x1b[4G\x1b[F");
    TR_ASSERT(t, at(0, 4));
    term.Feed("\x1b[7`\x1b[9d");
    TR_ASSERT(t, at(6, 8));
    term.Feed("\x1b[2a\x1b[e");
    TR_ASSERT(t, at(8, 9));
    term.Feed("\x1b[H");
    TR_ASSERT(t, at(0, 0));
    term.Feed("\x1b[;5H");
    TR_ASSERT(t, at(4, 0));
    term.Feed("\x1b[3H");
    TR_ASSERT(t, at(0, 2));
    // Clamped to the screen
    term.Feed("\x1b[99;99H");
    TR_ASSERT(t, at(19, 9));
    term.Feed("\x1b[99A\x1b[99D");
    TR_ASSERT(t, at(0, 0));
    // CHA puts words apart without spaces (old doc §4.B.3)
    term.Feed("Quick\x1b[7Gsafety\x1b[14Gcheck");
    TR_ASSERT(t, RowText(term, 0) == "Quick safety check");
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_erase(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 6, 4);
    const char *fill = "\x1b[HAAAAAA\r\nBBBBBB\r\nCCCCCC\r\nDDDDDD";
    term.Feed(fill);
    term.Feed("\x1b[2;3H\x1b[J");
    TR_ASSERT(t, ScreenText(term) == "AAAAAA\nBB");
    term.Feed(fill);
    term.Feed("\x1b[2;3H\x1b[1J");
    TR_ASSERT(t, ScreenText(term) == "\n   BBB\nCCCCCC\nDDDDDD");
    term.Feed(fill);
    term.Feed("\x1b[2J");
    TR_ASSERT(t, ScreenText(term).empty());
    TR_ASSERT(t, host.scrollbackClears == 0);
    // ED 3 leaves the screen alone and tells the host to drop its scrollback
    term.Feed("X\x1b[3J");
    TR_ASSERT(t, host.scrollbackClears == 1);
    TR_ASSERT(t, ScreenText(term) == "\n\n\n     X");
    // The cursor stays where it was
    TR_ASSERT(t, term.GetCursor().x == 5);
    TR_ASSERT(t, term.GetCursor().y == 3);

    term.Feed(fill);
    term.Feed("\x1b[1;3H\x1b[K\x1b[2;3H\x1b[1K\x1b[3;3H\x1b[2K\x1b[4;2H\x1b[3X");
    TR_ASSERT(t, ScreenText(term) == "AA\n   BBB\n\nD   DD");
    // ECH stops at the right edge
    term.Feed("\x1b[4;5H\x1b[99X");
    TR_ASSERT(t, RowText(term, 3) == "D");
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_insdel(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 6, 4);
    term.Feed("abcdef\x1b[1;3H\x1b[2@");
    TR_ASSERT(t, RowText(term, 0) == "ab  cd");
    term.Feed("\x1b[1;2H\x1b[3P");
    TR_ASSERT(t, RowText(term, 0) == "acd");
    term.Feed("\x1b[1;2H\x1b[99P");
    TR_ASSERT(t, RowText(term, 0) == "a");

    term.Feed("\x1b[H1\r\n2\r\n3\r\n4\x1b[2;4H\x1b[L");
    TR_ASSERT(t, ScreenText(term) == "1\n\n2\n3");
    TR_ASSERT(t, term.GetCursor().x == 0);     // IL/DL return the carriage
    term.Feed("\x1b[M\x1b[M");
    TR_ASSERT(t, ScreenText(term) == "1\n3");
    // IL/DL outside the scroll region do nothing
    term.Feed("\x1b[H1\r\n2\r\n3\r\n4\x1b[2;3r\x1b[4;1H\x1b[L\x1b[1;1H\x1b[M");
    TR_ASSERT(t, ScreenText(term) == "1\n2\n3\n4");
    // ... and inside it only the region moves
    term.Feed("\x1b[2;1H\x1b[L");
    TR_ASSERT(t, ScreenText(term) == "1\n\n2\n4");
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_scrollregion(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 5, 5);
    term.Feed("1\r\n2\r\n3\r\n4\r\n5");
    // DECSTBM homes the cursor
    term.Feed("\x1b[2;4r");
    TR_ASSERT(t, term.GetCursor().x == 0);
    TR_ASSERT(t, term.GetCursor().y == 0);
    // LF at the bottom margin scrolls the region only
    term.Feed("\x1b[4;1H\nX");
    TR_ASSERT(t, ScreenText(term) == "1\n3\n4\nX\n5");
    // RI at the top margin scrolls it down
    term.Feed("\x1b[2;1H\x1bMY");
    TR_ASSERT(t, ScreenText(term) == "1\nY\n3\n4\n5");
    // SU / SD
    term.Feed("\x1b[S");
    TR_ASSERT(t, ScreenText(term) == "1\n3\n4\n\n5");
    term.Feed("\x1b[2T");
    TR_ASSERT(t, ScreenText(term) == "1\n\n\n3\n5");
    // CUD stops at the bottom margin from inside the region, not from below it
    term.Feed("\x1b[3;1H\x1b[9B");
    TR_ASSERT(t, term.GetCursor().y == 3);
    term.Feed("\x1b[5;1H\x1b[9A");
    TR_ASSERT(t, term.GetCursor().y == 1);
    // LF below the region, on the last row, does not scroll
    term.Feed("\x1b[5;1H\n\nZ");
    TR_ASSERT(t, RowText(term, 4) == "Z");
    // Invalid region (top >= bottom) is ignored; reset with no params
    term.Feed("\x1b[3;3r");
    term.Feed("\x1b[r\x1b[5;1H\nW");
    TR_ASSERT(t, RowText(term, 4) == "W");
    TR_ASSERT(t, RowText(term, 0).empty());
    TR_ASSERT(t, host.scrolledOut.empty() == false);
    return kTR_Pass;
}

// Rows leaving the top of the primary screen go to the host - scrollback is the editor's (OnRowScrolledOut)
DLL_EXPORT int test_terminal_scrollout(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 4, 3);
    term.Feed("a\r\nb\r\nc\r\nd\r\nabcdefg");
    TR_ASSERT(t, host.scrolledOut == std::vector<std::string>({"a", "b", "c"}));
    TR_ASSERT(t, ScreenText(term) == "d\nabcd\nefg");
    term.Feed("\r\n");
    TR_ASSERT(t, host.scrolledOut.back() == "d");
    term.Feed("\r\n");
    TR_ASSERT(t, host.scrolledOut.back() == "abcd");
    TR_ASSERT(t, host.scrolledOutWrapped.back());       // the soft wrap travels with the row

    // A region that starts below the top keeps its rows; DL never feeds scrollback
    host.scrolledOut.clear();
    term.Feed("\x1b[2;3r\x1b[3;1H\n\n\x1b[r\x1b[H\x1b[M");
    TR_ASSERT(t, host.scrolledOut.empty());
    // The alternate screen has no scrollback
    term.Feed("\x1b[?1049h\x1b[3;1H\n\n\n\x1b[?1049l");
    TR_ASSERT(t, host.scrolledOut.empty());
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_tabs(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 20, 2);
    term.Feed("a\tb\tc");
    TR_ASSERT(t, RowText(term, 0) == "a       b       c");
    // HT stops at the last column
    term.Feed("\t\t\tX");
    TR_ASSERT(t, term.At(19, 0).codepoint == U'X');
    // Clear all, set one at column 4
    term.Feed("\x1b[3g\r\n\x1b[5G\x1bH\r\tY\tZ");
    TR_ASSERT(t, RowText(term, 1) == "    Y              Z");
    // CHT / CBT
    term.Feed("\x1b[3g\x1b[1;1H\x1b[2I");
    TR_ASSERT(t, term.GetCursor().x == 19);
    term.Feed("\x1b[1;1H\x1b[?5W");      // ignored (DECST8C not supported) - stops stay cleared
    term.Feed("\x1b[1;20H\x1b[Z");
    TR_ASSERT(t, term.GetCursor().x == 0);
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_rep(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 10, 2);
    term.Feed("\x1b[3b");               // nothing printed yet: ignored
    TR_ASSERT(t, ScreenText(term).empty());
    term.Feed("-\x1b[4b|");
    TR_ASSERT(t, RowText(term, 0) == "-----|");
    // Repeats wrap like printing does
    term.Feed("\x1b[9b");
    TR_ASSERT(t, ScreenText(term) == "-----|||||\n|||||");
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_savecursor(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 10, 5);
    term.Feed("\x1b[3;4H\x1b" "7\x1b[H\x1b" "8X");
    TR_ASSERT(t, RowText(term, 2) == "   X");
    term.Feed("\x1b[2;2H\x1b[s\x1b[5;5H\x1b[uY");
    TR_ASSERT(t, RowText(term, 1) == " Y");
    // Restore without a save: home
    Terminal fresh(host, 10, 5);
    fresh.Feed("\x1b[3;3H\x1b" "8Z");
    TR_ASSERT(t, RowText(fresh, 0) == "Z");
    // The charset is part of the saved state
    term.Feed("\x1b(0\x1b" "7\x1b(B\x1b" "8\x1b[5;1Hq");
    TR_ASSERT(t, term.At(0, 4).codepoint == U'─');
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_originmode(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 10, 6);
    term.Feed("\x1b[2;5r\x1b[?6h");
    TR_ASSERT(t, term.GetCursor().y == 1);
    term.Feed("\x1b[2;3HA");
    TR_ASSERT(t, RowText(term, 2) == "  A");
    // Clamped into the region
    term.Feed("\x1b[99;1HB");
    TR_ASSERT(t, RowText(term, 4) == "B");
    term.Feed("\x1b[?6l\x1b[1;1HC");
    TR_ASSERT(t, RowText(term, 0) == "C");
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_altscreen(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 10, 4);
    term.Feed("shell$ \x1b[?1049h");
    TR_ASSERT(t, term.IsAltScreen());
    TR_ASSERT(t, ScreenText(term).empty());
    term.Feed("\x1b[HVIM\x1b[4;1H~");
    TR_ASSERT(t, ScreenText(term) == "VIM\n\n\n~");
    term.Feed("\x1b[?1049l");
    TR_ASSERT(t, !term.IsAltScreen());
    TR_ASSERT(t, ScreenText(term) == "shell$");
    TR_ASSERT(t, term.GetCursor().x == 7);      // restored
    TR_ASSERT(t, term.GetCursor().y == 0);

    // 1049 reset on the primary screen still restores the saved cursor (dialog does this on exit)
    term.Feed("\x1b[3;3H\x1b[?1049l");
    TR_ASSERT(t, term.GetCursor().x == 7);
    TR_ASSERT(t, term.GetCursor().y == 0);

    // 1049 entered again starts clean
    term.Feed("\x1b[?1049h");
    TR_ASSERT(t, ScreenText(term).empty());
    term.Feed("\x1b[?1049l");

    // 47 keeps the alternate content and the cursor
    term.Feed("\x1b[?47hALT\x1b[?47l");
    TR_ASSERT(t, ScreenText(term) == "shell$");
    TR_ASSERT(t, term.GetCursor().x == 10 - 1);
    term.Feed("\x1b[?47h");
    TR_ASSERT(t, RowText(term, 0) == "       ALT");
    term.Feed("\x1b[?47l");
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_charset(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 10, 3);
    term.Feed("\x1b(0lqk\x1b(Bx");
    TR_ASSERT(t, RowText(term, 0) == "┌─┐x");
    // G1 via SO / SI
    term.Feed("\r\n\x1b)0a\x0eqx\x0fq");
    TR_ASSERT(t, RowText(term, 1) == "a─│q");
    // Only 0x5F-0x7E are translated
    term.Feed("\r\n\x1b(0AZ_~\x1b(B");
    TR_ASSERT(t, RowText(term, 2) == "AZ ·");
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_insertmode(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 6, 2);
    term.Feed("abcdef\r\x1b[4hXY\x1b[4lZ");
    TR_ASSERT(t, RowText(term, 0) == "XYZbcd");
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_resize(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 6, 4);
    term.Feed("1\r\n2\r\n3\r\n4abcde");
    // Shrinking below the cursor pushes rows out at the top, so the cursor's row stays
    term.Resize(4, 2);
    TR_ASSERT(t, host.scrolledOut == std::vector<std::string>({"1", "2"}));
    TR_ASSERT(t, ScreenText(term) == "3\n4abc");
    TR_ASSERT(t, term.GetCursor().x == 3);
    TR_ASSERT(t, term.GetCursor().y == 1);
    // Growing adds blank rows and columns; nothing comes back
    term.Resize(8, 4);
    TR_ASSERT(t, ScreenText(term) == "3\n4abc");
    term.Feed("\x1b[4;8HZ");
    TR_ASSERT(t, RowText(term, 3) == "       Z");
    // Default tab stops on the new columns
    term.Feed("\r\t\tT");
    TR_ASSERT(t, term.GetCursor().x == 7);
    // Shrinking with the cursor high up keeps the top rows
    host.scrolledOut.clear();
    term.Feed("\x1b[H");
    term.Resize(8, 2);
    TR_ASSERT(t, host.scrolledOut.empty());
    TR_ASSERT(t, RowText(term, 0) == "3");
    // The scroll region resets
    term.Feed("\x1b[2;1H\n");
    TR_ASSERT(t, RowText(term, 0) == "4abc");

    // On the alternate screen the primary's saved cursor decides what the primary keeps
    RecordingHost altHost;
    Terminal alt(altHost, 5, 4);
    alt.Feed("p\r\nq\x1b[?1049h\x1b[4;1Hv");
    alt.Resize(5, 2);
    TR_ASSERT(t, altHost.scrolledOut.empty());
    TR_ASSERT(t, RowText(alt, 1) == "v");
    alt.Feed("\x1b[?1049l");
    TR_ASSERT(t, ScreenText(alt) == "p\nq");
    TR_ASSERT(t, alt.GetCursor().y == 1);

    // Never 0x0
    term.Resize(0, 0);
    TR_ASSERT(t, term.Cols() == 1);
    TR_ASSERT(t, term.Rows() == 1);
    term.Feed("abc\x1b[5;5H\x1b[L\x1b[3@\x1b[J");
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_reset(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 6, 3);
    term.Feed("abc\x1b[2;3r\x1b[?7l\x1b(0\x1b[?1049hq\x1b" "c");
    TR_ASSERT(t, !term.IsAltScreen());
    TR_ASSERT(t, ScreenText(term).empty());
    TR_ASSERT(t, term.GetModes().isAutoWrap);
    term.Feed("q\r\n\n\nx");
    TR_ASSERT(t, ScreenText(term) == "\n\nx");
    TR_ASSERT(t, host.scrolledOut == std::vector<std::string>({"q"}));
    return kTR_Pass;
}

// Split invariance at the screen level: each fixture fed in small chunks produces the same grid as fed whole
DLL_EXPORT int test_terminal_split(ITesting *t) {
    auto fixtures = ListFixtures();
    TR_ASSERT(t, !fixtures.empty());
    for (auto &path : fixtures) {
        auto bytes = LoadFixture(path);
        RecordingHost wholeHost;
        Terminal whole(wholeHost, 80, 24);
        whole.Feed(bytes);
        for (size_t chunk : {1, 3, 7}) {
            RecordingHost splitHost;
            Terminal split(splitHost, 80, 24);
            for (size_t i = 0; i < bytes.size(); i += chunk) {
                split.Feed(std::string_view(bytes).substr(i, chunk));
            }
            if ((ScreenText(split) != ScreenText(whole)) || (splitHost.scrolledOut != wholeHost.scrolledOut)) {
                t->Error(__LINE__, __FILE__, "%s: split by %zu differs", path.filename().c_str(), chunk);
                return kTR_Fail;
            }
        }
    }
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_sgr(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 20, 2);
    term.Feed("\x1b[1;3;4;5;7;8;9;53mA\x1b[22;23;24;25;27;28;29;55mB\x1b[2mC\x1b[22mD");
    TR_ASSERT(t, term.At(0, 0).attrs == (kAttrBold | kAttrItalic | kAttrUnderline | kAttrBlink | kAttrInverse |
                                         kAttrHidden | kAttrStrike | kAttrOverline));
    TR_ASSERT(t, term.At(0, 0).underlineStyle == UnderlineStyle::kSingle);
    TR_ASSERT(t, term.At(1, 0).attrs == kAttrNone);
    TR_ASSERT(t, term.At(1, 0).underlineStyle == UnderlineStyle::kNone);
    TR_ASSERT(t, term.At(2, 0).attrs == kAttrDim);
    TR_ASSERT(t, term.At(3, 0).attrs == kAttrNone);
    // 22 clears bold AND dim; 21 is double underline
    term.Feed("\x1b[1;2;21mE\x1b[22mF");
    TR_ASSERT(t, term.At(4, 0).attrs == (kAttrBold | kAttrDim | kAttrUnderline));
    TR_ASSERT(t, term.At(4, 0).underlineStyle == UnderlineStyle::kDouble);
    TR_ASSERT(t, term.At(5, 0).attrs == kAttrUnderline);
    // CSI m and CSI 0m reset everything, including colours
    term.Feed("\x1b[31;44mG\x1b[mH\x1b[1;32m\x1b[0mI");
    TR_ASSERT(t, term.At(6, 0).fg == Color::Indexed(1));
    TR_ASSERT(t, term.At(7, 0) == Cell{U'H'});
    TR_ASSERT(t, term.At(8, 0) == Cell{U'I'});
    // Empty parameters are 0: "CSI 1;m" ends with a reset
    term.Feed("\x1b[1;mJ");
    TR_ASSERT(t, term.At(9, 0).attrs == kAttrNone);
    // The pen is saved and restored with the cursor
    term.Feed("\x1b[1;35m\x1b" "7\x1b[0m\x1b" "8K");
    TR_ASSERT(t, term.At(10, 0).attrs == kAttrBold);
    TR_ASSERT(t, term.At(10, 0).fg == Color::Indexed(5));
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_sgrcolor(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 20, 2);
    term.Feed("\x1b[30;47mA\x1b[97;100mB\x1b[39mC\x1b[49mD");
    TR_ASSERT(t, term.At(0, 0).fg == Color::Indexed(0));
    TR_ASSERT(t, term.At(0, 0).bg == Color::Indexed(7));
    TR_ASSERT(t, term.At(1, 0).fg == Color::Indexed(15));
    TR_ASSERT(t, term.At(1, 0).bg == Color::Indexed(8));
    TR_ASSERT(t, term.At(2, 0).fg == Color{});
    TR_ASSERT(t, term.At(2, 0).bg == Color::Indexed(8));
    TR_ASSERT(t, term.At(3, 0).bg == Color{});
    // 256 and truecolor, ';' form, mixed with plain codes
    term.Feed("\x1b[1;38;5;208;48;2;1;2;3;4mE");
    auto &e = term.At(4, 0);
    TR_ASSERT(t, e.fg == Color::Indexed(208));
    TR_ASSERT(t, e.bg == Color::Rgb(1, 2, 3));
    TR_ASSERT(t, e.attrs == (kAttrBold | kAttrUnderline));
    // Clamped to a byte
    term.Feed("\x1b[0;38;5;999;48;2;300;0;256mF");
    TR_ASSERT(t, term.At(5, 0).fg == Color::Indexed(255));
    TR_ASSERT(t, term.At(5, 0).bg == Color::Rgb(255, 0, 255));
    // A cut-short colour ends the sequence: what came before applies, nothing after
    term.Feed("\x1b[0;1;38;2;10;20mG");
    TR_ASSERT(t, term.At(6, 0).attrs == kAttrBold);
    TR_ASSERT(t, term.At(6, 0).fg == Color{});
    // Underline colour, and 59 resets it
    term.Feed("\x1b[0;4;58;5;9mH\x1b[59mI");
    TR_ASSERT(t, term.At(7, 0).underlineColor == Color::Indexed(9));
    TR_ASSERT(t, term.At(8, 0).underlineColor == Color{});
    return kTR_Pass;
}

// Colon sub-parameters: the forms from Ghostty's Parser.zig / sgr.zig tests and real applications
DLL_EXPORT int test_terminal_sgrcolon(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 20, 2);
    term.Feed("\x1b[38:2::255:193:7mA");                // T.416 with empty colorspace
    TR_ASSERT(t, term.At(0, 0).fg == Color::Rgb(255, 193, 7));
    term.Feed("\x1b[48:2:1:2:3mB");                     // common form without colorspace
    TR_ASSERT(t, term.At(1, 0).bg == Color::Rgb(1, 2, 3));
    term.Feed("\x1b[38:5:17mC");
    TR_ASSERT(t, term.At(2, 0).fg == Color::Indexed(17));
    term.Feed("\x1b[0;4:3mD\x1b[4:0mE\x1b[4:5mF\x1b[4:4mG\x1b[4:2mH\x1b[4:1mI");
    TR_ASSERT(t, term.At(3, 0).underlineStyle == UnderlineStyle::kCurly);
    TR_ASSERT(t, (term.At(3, 0).attrs & kAttrUnderline) != 0);
    TR_ASSERT(t, term.At(4, 0).underlineStyle == UnderlineStyle::kNone);
    TR_ASSERT(t, (term.At(4, 0).attrs & kAttrUnderline) == 0);
    TR_ASSERT(t, term.At(5, 0).underlineStyle == UnderlineStyle::kDashed);
    TR_ASSERT(t, term.At(6, 0).underlineStyle == UnderlineStyle::kDotted);
    TR_ASSERT(t, term.At(7, 0).underlineStyle == UnderlineStyle::kDouble);
    TR_ASSERT(t, term.At(8, 0).underlineStyle == UnderlineStyle::kSingle);
    // kakoune: reset, curly underline, ';' truecolor fg, ':' underline colour - all in one
    term.Feed("\r\n\x1b[;4:3;38;2;175;175;215;58:2::190:80:70mK");
    auto &k = term.At(0, 1);
    TR_ASSERT(t, k.underlineStyle == UnderlineStyle::kCurly);
    TR_ASSERT(t, k.fg == Color::Rgb(175, 175, 215));
    TR_ASSERT(t, k.underlineColor == Color::Rgb(190, 80, 70));
    TR_ASSERT(t, k.bg == Color{});
    // A colon group on another code is ignored whole, the rest still applies
    term.Feed("\x1b[0;1:2;3mL");
    TR_ASSERT(t, term.At(1, 1).attrs == kAttrItalic);
    // A colon group does not swallow the ';' parameters after it
    term.Feed("\x1b[0;38:5:1;44mM");
    TR_ASSERT(t, term.At(2, 1).fg == Color::Indexed(1));
    TR_ASSERT(t, term.At(2, 1).bg == Color::Indexed(4));
    // Outside SGR a colon drops the sequence: no cursor move, no mode change
    term.Feed("\x1b[1:2H\x1b[?7:1l");
    TR_ASSERT(t, term.GetCursor().x == 3);
    TR_ASSERT(t, term.GetCursor().y == 1);
    TR_ASSERT(t, term.GetModes().isAutoWrap);
    TR_ASSERT(t, host.unhandled.size() >= 2);
    return kTR_Pass;
}

// Erase fills with the pen's background only (BCE), never its attributes
DLL_EXPORT int test_terminal_sgrerase(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 6, 3);
    term.Feed("abcdef\x1b[1;4;31;42m\x1b[1;3H\x1b[K\x1b[2;1H\x1b[2X");
    Cell expected;
    expected.bg = Color::Indexed(2);
    TR_ASSERT(t, term.At(2, 0) == expected);
    TR_ASSERT(t, term.At(5, 0) == expected);
    TR_ASSERT(t, term.At(1, 1) == expected);
    TR_ASSERT(t, term.At(2, 1) == Cell{});
    // Rows scrolled in take it too
    term.Feed("\x1b[3;1H\n");
    TR_ASSERT(t, term.At(0, 2) == expected);
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_replies(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 20, 10);
    term.Feed("\x1b[c");
    TR_ASSERT(t, host.replies == "\x1b[?62;22c");
    host.replies.clear();
    term.Feed("\x1b[0c\x1b[1c");                 // only Ps = 0 asks
    TR_ASSERT(t, host.replies == "\x1b[?62;22c");
    host.replies.clear();
    term.Feed("\x1b[>c");
    TR_ASSERT(t, host.replies == "\x1b[>1;10;0c");
    host.replies.clear();
    term.Feed("\x1b[5n");
    TR_ASSERT(t, host.replies == "\x1b[0n");
    host.replies.clear();
    term.Feed("\x1b[3;7H\x1b[6n");
    TR_ASSERT(t, host.replies == "\x1b[3;7R");
    host.replies.clear();
    term.Feed("\x1b[?6n");
    TR_ASSERT(t, host.replies == "\x1b[?3;7R");
    host.replies.clear();
    // CPR under DECOM is relative to the region
    term.Feed("\x1b[2;8r\x1b[?6h\x1b[3;1H\x1b[6n\x1b[?6l\x1b[r");
    TR_ASSERT(t, host.replies == "\x1b[3;1R");
    host.replies.clear();
    term.Feed("\x1b[>q");
    TR_ASSERT(t, host.replies == "\x1bP>|gvt 0.1.0\x1b\\");
    host.replies.clear();
    term.Feed("\x1b[18t");
    TR_ASSERT(t, host.replies == "\x1b[8;10;20t");
    host.replies.clear();
    // The kitty keyboard query stays unanswered: apps then fall back to legacy keys (DA1 answers after it)
    term.Feed("\x1b[?u\x1b[c");
    TR_ASSERT(t, host.replies == "\x1b[?62;22c");
    // Split across feeds, the reply still comes once
    host.replies.clear();
    term.Feed("\x1b[");
    term.Feed("6");
    term.Feed("n");
    TR_ASSERT(t, host.replies == "\x1b[1;1R");     // DECSTBM above homed the cursor
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_decrqm(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 20, 10);
    term.Feed("\x1b[?2026$p");
    TR_ASSERT(t, host.replies == "\x1b[?2026;2$y");
    host.replies.clear();
    term.Feed("\x1b[?2026h\x1b[?2026$p\x1b[?7$p\x1b[?9999$p");
    TR_ASSERT(t, host.replies == "\x1b[?2026;1$y\x1b[?7;1$y\x1b[?9999;0$y");
    host.replies.clear();
    term.Feed("\x1b[4$p\x1b[4h\x1b[4$p\x1b[3$p");
    TR_ASSERT(t, host.replies == "\x1b[4;2$y\x1b[4;1$y\x1b[3;0$y");
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_modes(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 20, 10);
    auto &modes = term.GetModes();
    term.Feed("\x1b[?1h\x1b=\x1b[?25l\x1b[?2004h\x1b[?1004h\x1b[?12h");
    TR_ASSERT(t, modes.isCursorKeysApp);
    TR_ASSERT(t, modes.isKeypadApp);
    TR_ASSERT(t, !term.GetCursor().isVisible);
    TR_ASSERT(t, modes.isBracketedPaste);
    TR_ASSERT(t, modes.isFocusEvents);
    TR_ASSERT(t, modes.isCursorBlink);
    // Mouse: the last tracking mode set wins; resetting another one leaves it
    term.Feed("\x1b[?1000h\x1b[?1002h\x1b[?1006h");
    TR_ASSERT(t, modes.mouseTracking == MouseTracking::kButtonEvent);
    TR_ASSERT(t, modes.mouseEncoding == MouseEncoding::kSgr);
    term.Feed("\x1b[?1000l");
    TR_ASSERT(t, modes.mouseTracking == MouseTracking::kButtonEvent);
    term.Feed("\x1b[?1002l\x1b[?1006l");
    TR_ASSERT(t, modes.mouseTracking == MouseTracking::kOff);
    TR_ASSERT(t, modes.mouseEncoding == MouseEncoding::kDefault);
    // Synchronized output
    term.Feed("\x1b[?2026h");
    TR_ASSERT(t, term.IsSynchronizedUpdate());
    term.Feed("\x1b[?2026l");
    TR_ASSERT(t, !term.IsSynchronizedUpdate());
    // DECSCUSR
    term.Feed("\x1b[6 q");
    TR_ASSERT(t, modes.cursorShape == CursorShape::kBar);
    TR_ASSERT(t, !modes.isCursorBlink);
    term.Feed("\x1b[3 q");
    TR_ASSERT(t, modes.cursorShape == CursorShape::kUnderline);
    TR_ASSERT(t, modes.isCursorBlink);
    term.Feed("\x1b[0 q");
    TR_ASSERT(t, modes.cursorShape == CursorShape::kDefault);
    // RIS clears them all
    term.Feed("\x1b" "c");
    TR_ASSERT(t, !modes.isCursorKeysApp);
    TR_ASSERT(t, modes.mouseTracking == MouseTracking::kOff);
    TR_ASSERT(t, term.GetCursor().isVisible);
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_osc(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 20, 10);
    term.Feed("\x1b]0;hello\x07\x1b]2;\xE2\x9C\xB3 Claude\x1b\\\x1b]1;icon\x07");
    TR_ASSERT(t, host.titles == std::vector<std::string>({"hello", "\xE2\x9C\xB3 Claude"}));
    term.Feed("\x1b]7;file://host/tmp/a%20b\x07");
    TR_ASSERT(t, host.directories == std::vector<std::string>({"file://host/tmp/a%20b"}));
    term.Feed("\x1b]133;A\x07$ \x1b]133;B\x07ls\r\n\x1b]133;C\x07out\r\n\x1b]133;D;0\x1b\\\x1b]133;D\x07");
    TR_ASSERT(t, host.prompts == std::vector<std::string>({"A|", "B|", "C|", "D|0", "D|"}));
    TR_ASSERT(t, ScreenText(term) == "$ ls\nout");
    term.Feed("\x1b]8;id=1;http://x\x1b\\link\x1b]8;;\x1b\\");
    TR_ASSERT(t, host.links == std::vector<std::string>({"id=1|http://x", "|"}));
    term.Feed("\x1b]52;c;aGVsbG8=\x07\x1b]52;c;?\x07");
    TR_ASSERT(t, host.clipboard == std::vector<std::string>({"c|aGVsbG8="}));
    // Nothing printed by any of them
    TR_ASSERT(t, RowText(term, 2) == "link");
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_osccolor(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 20, 10);
    // A host that does not answer: no reply at all
    term.Feed("\x1b]10;?\x07");
    TR_ASSERT(t, host.replies.empty());
    host.isAnsweringColors = true;
    term.Feed("\x1b]10;?\x07\x1b]11;?\x1b\\");
    TR_ASSERT(t, host.replies == "\x1b]10;rgb:0a0a/8080/ffff\x1b\\\x1b]11;rgb:0b0b/8080/ffff\x1b\\");
    host.replies.clear();
    term.Feed("\x1b]4;1;?;200;?\x07\x1b]4;2;#ff0000\x07");
    TR_ASSERT(t, host.replies == "\x1b]4;1;rgb:0101/8080/ffff\x1b\\\x1b]4;200;rgb:c8c8/8080/ffff\x1b\\");
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_charwidth(ITesting *t) {
    TR_ASSERT(t, CharWidth(U'a') == 1);
    TR_ASSERT(t, CharWidth(U'å') == 1);
    TR_ASSERT(t, CharWidth(U'▛') == 1);             // block elements: narrow (East Asian Ambiguous counts as 1)
    TR_ASSERT(t, CharWidth(U'─') == 1);
    TR_ASSERT(t, CharWidth(U'✳') == 1);             // text presentation by default
    TR_ASSERT(t, CharWidth(U'中') == 2);
    TR_ASSERT(t, CharWidth(U'가') == 2);             // Hangul syllable
    TR_ASSERT(t, CharWidth(U'Ａ') == 2);             // fullwidth Latin
    TR_ASSERT(t, CharWidth(U'😀') == 2);
    TR_ASSERT(t, CharWidth(U'🚀') == 2);
    TR_ASSERT(t, CharWidth(U'\U00020000') == 2);    // CJK Extension B
    TR_ASSERT(t, CharWidth(U'\U0002FFF0') == 2);    // unassigned, but in a wide plane
    TR_ASSERT(t, CharWidth(U'́') == 0);         // combining acute
    TR_ASSERT(t, CharWidth(U'‍') == 0);         // ZWJ
    TR_ASSERT(t, CharWidth(U'️') == 0);         // VS16
    TR_ASSERT(t, CharWidth(U'ᅠ') == 0);         // Hangul medial jamo
    TR_ASSERT(t, CharWidth(U'­') == 1);         // soft hyphen stays visible
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_wide(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 6, 3);
    term.Feed("a中b");
    TR_ASSERT(t, term.At(1, 0).codepoint == U'中');
    TR_ASSERT(t, term.At(1, 0).width == 2);
    TR_ASSERT(t, term.At(2, 0).width == 0);
    TR_ASSERT(t, term.At(3, 0).codepoint == U'b');
    TR_ASSERT(t, term.GetCursor().x == 4);
    TR_ASSERT(t, RowText(term, 0) == "a中b");
    // Does not fit in the last column: the column is left blank and the character wraps
    term.Feed("c文");
    TR_ASSERT(t, RowText(term, 0) == "a中bc");
    TR_ASSERT(t, term.IsRowWrapped(0));
    TR_ASSERT(t, RowText(term, 1) == "文");
    // Exactly fills the row: pending wrap on its right half
    term.Feed("\x1b[3;1Habcd字");
    TR_ASSERT(t, term.GetCursor().x == 5);
    TR_ASSERT(t, term.GetCursor().isPendingWrap);
    // Without autowrap it takes the last two columns
    term.Feed("\x1b[?7l\x1b[2;1Hxxxxx中\x1b[?7h");
    TR_ASSERT(t, RowText(term, 1) == "xxxx中");
    // REP repeats it whole
    term.Feed("\x1b[3;1H\x1b[2K中\x1b[b");
    TR_ASSERT(t, RowText(term, 2) == "中中");
    return kTR_Pass;
}

// Writing, erasing, inserting or deleting over one half of a wide character takes the whole character (xterm,
// Ghostty). libvterm leaves the other half behind - see kScreenDivergences.
DLL_EXPORT int test_terminal_widehalves(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 10, 6);
    term.Feed("中文\x1b[1Gx");
    TR_ASSERT(t, RowText(term, 0) == "x 文");
    TR_ASSERT(t, term.At(1, 0).width == 1);
    term.Feed("\x1b[2;1H中文\x1b[2;2Hx");
    TR_ASSERT(t, RowText(term, 1) == " x文");
    term.Feed("\x1b[3;1H中文字\x1b[3;4H\x1b[K");
    TR_ASSERT(t, RowText(term, 2) == "中");
    term.Feed("\x1b[4;1H中文字\x1b[4;2H\x1b[X");
    TR_ASSERT(t, RowText(term, 3) == "  文字");
    // DCH that moves a right half to the front of the cursor
    term.Feed("\x1b[5;1Ha中b\x1b[5;2H\x1b[P");
    TR_ASSERT(t, RowText(term, 4) == "a b");
    // ICH pushing a wide character over the right edge drops it whole
    term.Feed("\x1b[6;1Habcdefgh中\x1b[6;1H\x1b[@");
    TR_ASSERT(t, RowText(term, 5) == " abcdefgh");
    for (int y = 0; y < term.Rows(); y++) {
        for (int x = 0; x < term.Cols(); x++) {
            auto &cell = term.At(x, y);
            if (cell.width == 2) {
                TR_ASSERT(t, term.At(x + 1, y).width == 0);
            } else if (cell.width == 0) {
                TR_ASSERT(t, (x > 0) && (term.At(x - 1, y).width == 2));
            }
        }
    }
    // Shrinking through a wide character keeps no half
    Terminal narrow(host, 4, 1);
    narrow.Feed("ab中");
    narrow.Resize(3, 1);
    TR_ASSERT(t, RowText(narrow, 0) == "ab");
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_combining(ITesting *t) {
    RecordingHost host;
    Terminal term(host, 10, 3);
    term.Feed("éä̲x");
    TR_ASSERT(t, RowText(term, 0) == "éä̲x");
    TR_ASSERT(t, term.GetCursor().x == 3);
    TR_ASSERT(t, term.Grapheme(term.At(0, 0)) == U"é");
    TR_ASSERT(t, term.Grapheme(term.At(2, 0)) == U"x");
    // Onto a wide character, and onto the last column while a wrap is pending
    term.Feed("\r\n中́\x1b[3;10Hź");
    TR_ASSERT(t, term.Grapheme(term.At(0, 1)) == U"中́");
    TR_ASSERT(t, term.Grapheme(term.At(9, 2)) == U"ź");
    // At the start of a row there is nothing to join: dropped
    term.Feed("\x1b[2;1H\x1b[2Ḱq");
    TR_ASSERT(t, RowText(term, 1) == "q");
    // Overwriting the cell drops its marks
    term.Feed("\x1b[1;1Hy");
    TR_ASSERT(t, term.Grapheme(term.At(0, 0)) == U"y");
    // ZWJ joins, the emoji after it takes its own cells (what wcwidth-based apps expect)
    term.Feed("\x1b[3;1H\x1b[2K👨‍👩!");
    TR_ASSERT(t, term.Grapheme(term.At(0, 2)) == U"👨‍");
    TR_ASSERT(t, term.At(2, 2).codepoint == U'👩');
    TR_ASSERT(t, term.GetCursor().x == 5);
    // The pool is compacted rather than growing without end
    for (int i = 0; i < 70000; i++) {
        term.Feed("\x1b[1;1Há");
    }
    TR_ASSERT(t, term.Grapheme(term.At(0, 0)) == U"á");
    return kTR_Pass;
}
