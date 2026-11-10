//
// gvt Terminal — screen model unit tests (VT-3). Expected results are xterm's (ctlseqs); the oracle module checks the
// same ground against libvterm.
//
#include <testinterface.h>

#include "gvt/Terminal.h"
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
