//
// Created by gnilk on 04.06.26.
//
// TerminalScreen: the editor's side of the embedded terminal - scrollback, last output, abs-row spine,
// persistence, resolved-colour rows - over a gnilk::vt::Terminal. Content is produced the way the shell
// produces it: bytes fed in. The emulation itself (cursor, erase, scroll regions, SGR, ...) is tested in the
// gvt library (src/ext/gvt/tests, `gvttests`); the cases here check what the editor relies on.
//
#include <testinterface.h>
#include "Core/TerminalScreen.h"
#include "Core/Config/Config.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_terminalscreen(ITesting *t);
DLL_EXPORT int test_terminalscreen_putchar(ITesting *t);
DLL_EXPORT int test_terminalscreen_newline(ITesting *t);
DLL_EXPORT int test_terminalscreen_scroll(ITesting *t);
DLL_EXPORT int test_terminalscreen_cr(ITesting *t);
DLL_EXPORT int test_terminalscreen_tab(ITesting *t);
DLL_EXPORT int test_terminalscreen_colors(ITesting *t);
DLL_EXPORT int test_terminalscreen_cursor(ITesting *t);
DLL_EXPORT int test_terminalscreen_erase(ITesting *t);
DLL_EXPORT int test_terminalscreen_erase_in_line(ITesting *t);
DLL_EXPORT int test_terminalscreen_erase_in_display(ITesting *t);
DLL_EXPORT int test_terminalscreen_scroll_region(ITesting *t);
DLL_EXPORT int test_terminalscreen_savestate(ITesting *t);
DLL_EXPORT int test_terminalscreen_savestate_excludes_scrollback(ITesting *t);
DLL_EXPORT int test_terminalscreen_writeline(ITesting *t);
DLL_EXPORT int test_terminalscreen_resize(ITesting *t);
DLL_EXPORT int test_terminalscreen_resize_degenerate(ITesting *t);
DLL_EXPORT int test_terminalscreen_resize_keeps_content(ITesting *t);
DLL_EXPORT int test_terminalscreen_scrollback_color_roundtrip(ITesting *t);
DLL_EXPORT int test_terminalscreen_absrow_spine(ITesting *t);
DLL_EXPORT int test_terminalscreen_scrollback_cap(ITesting *t);
DLL_EXPORT int test_terminalscreen_scrollback_cap_unlimited(ITesting *t);
DLL_EXPORT int test_terminalscreen_last_output(ITesting *t);
DLL_EXPORT int test_terminalscreen_snapshot_roundtrip(ITesting *t);
DLL_EXPORT int test_terminalscreen_snapshot_altscreen_empty(ITesting *t);
DLL_EXPORT int test_terminalscreen_snapshot_caps_tail(ITesting *t);
DLL_EXPORT int test_terminalscreen_snapshot_includes_screen(ITesting *t);
DLL_EXPORT int test_terminalscreen_history_follows_theme(ITesting *t);
DLL_EXPORT int test_terminalscreen_split_sequence(ITesting *t);
DLL_EXPORT int test_terminalscreen_palette(ITesting *t);
DLL_EXPORT int test_terminalscreen_inverse(ITesting *t);
DLL_EXPORT int test_terminalscreen_wide(ITesting *t);
DLL_EXPORT int test_terminalscreen_host(ITesting *t);
DLL_EXPORT int test_terminalscreen_before_resize(ITesting *t);
DLL_EXPORT int test_terminalscreen_clear_scrollback(ITesting *t);
}

static const ColorRGBA WHITE = ColorRGBA::FromRGB(1.0f, 1.0f, 1.0f);
static const ColorRGBA BLACK = ColorRGBA::FromRGB(0.0f, 0.0f, 0.0f);
static const ColorRGBA RED   = ColorRGBA::FromRGB(1.0f, 0.0f, 0.0f);
static const ColorRGBA GREEN = ColorRGBA::FromRGB(0.0f, 1.0f, 0.0f);

static const char *SGR_RED        = "\x1b[38;2;255;0;0m";
static const char *SGR_GREEN      = "\x1b[38;2;0;255;0m";
static const char *SGR_BG_GREEN   = "\x1b[48;2;0;255;0m";
static const char *SGR_DEFAULT_FG = "\x1b[39m";
static const char *SGR_BOLD       = "\x1b[1m";
static const char *SGR_RESET      = "\x1b[m";
static const char *ENTER_ALT      = "\x1b[?1049h";
static const char *LEAVE_ALT      = "\x1b[?1049l";

static void Setup(TerminalScreen &s, int cols = 10, int rows = 5) {
    s.Resize(cols, rows);
    s.SetDefaultColors(WHITE, BLACK);
}

// CUP with 0-based x, y
static std::string MoveTo(int x, int y) {
    return "\x1b[" + std::to_string(y + 1) + ";" + std::to_string(x + 1) + "H";
}

// What Console.WriteLine("text") produces: the text, then CR LF - each line starts at column 0
static void SimulateWriteLine(TerminalScreen &s, const std::u32string &str) {
    s.Feed(UnicodeHelper::utf32to8(str) + "\r\n");
}

DLL_EXPORT int test_terminalscreen(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_putchar(ITesting *t) {
    TerminalScreen s;
    Setup(s);
    s.Feed("ABC");

    TR_ASSERT(t, s.GetRow(0)[0].ch == U'A');
    TR_ASSERT(t, s.GetRow(0)[1].ch == U'B');
    TR_ASSERT(t, s.GetRow(0)[2].ch == U'C');
    TR_ASSERT(t, s.GetCursorPos().x == 3);
    TR_ASSERT(t, s.GetCursorPos().y == 0);
    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_newline(ITesting *t) {
    {
        // \n alone is a pure vertical move - x stays where it is (terminal convention)
        TerminalScreen s;
        Setup(s);
        s.Feed("A\n");
        TR_ASSERT(t, s.GetCursorPos().x == 1);
        TR_ASSERT(t, s.GetCursorPos().y == 1);
    }
    {
        // \r\n (CR+LF) is what moves to the start of the next line
        TerminalScreen s;
        Setup(s);
        s.Feed("A\r\nB");
        TR_ASSERT(t, s.GetRow(0)[0].ch == U'A');
        TR_ASSERT(t, s.GetRow(1)[0].ch == U'B');
        TR_ASSERT(t, s.GetCursorPos().y == 1);
    }
    {
        // More newlines than rows: the cursor stays on the last row
        TerminalScreen s;
        Setup(s);
        for (int i = 0; i < s.Rows(); i++) {
            s.Feed("\n");
        }
        TR_ASSERT(t, s.GetCursorPos().y == s.Rows() - 1);
    }
    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_resize(ITesting *t) {
    // A recognisable char at the start of each of the 5 rows
    TerminalScreen s;
    Setup(s, 10, 5);
    s.Feed("A\r\nB\r\nC\r\nD\r\nE\r");
    TR_ASSERT(t, s.GetCursorPos().y == 4);

    // Grow height: existing rows stay at the top, content is NOT blanked
    s.Resize(10, 8);
    TR_ASSERT(t, s.Rows() == 8);
    for (int y = 0; y < 5; y++) {
        TR_ASSERT(t, s.GetRow(y)[0].ch == (char32_t)(U'A' + y));
    }
    TR_ASSERT(t, s.GetCursorPos().y == 4);   // cursor row unchanged on grow

    // Grow width: rows widen, original cells preserved
    s.Resize(20, 8);
    TR_ASSERT(t, s.Cols() == 20);
    TR_ASSERT(t, (int)s.GetRow(0).size() == 20);
    TR_ASSERT(t, s.GetRow(2)[0].ch == U'C');

    // Shrink height below content: bottom rows kept, overflow top rows go to scrollback
    TerminalScreen s2;
    Setup(s2, 10, 5);
    s2.Feed("A\r\nB\r\nC\r\nD\r\nE\r");
    size_t sbBefore = s2.GetScrollback()->NumLines();
    s2.Resize(10, 3);                        // drop 2 rows from the top
    TR_ASSERT(t, s2.Rows() == 3);
    TR_ASSERT(t, s2.GetScrollback()->NumLines() == sbBefore + 2);
    TR_ASSERT(t, s2.GetRow(0)[0].ch == U'C'); // rows C,D,E remain (A,B scrolled off)
    TR_ASSERT(t, s2.GetRow(2)[0].ch == U'E');
    TR_ASSERT(t, s2.GetCursorPos().y == 2);   // cursor followed the content down
    return kTR_Pass;
}

//
// Regression: resizing back and forth injected blank lines / drifted the content. In shell mode the
// content occupies rows [0..cursor.y] and the rows BELOW the cursor are blank padding the view never
// shows. A shrink that still has room for the content must NOT touch scrollback and must keep the
// content + cursor.
//
DLL_EXPORT int test_terminalscreen_resize_keeps_content(ITesting *t) {
    TerminalScreen s;
    Setup(s, 20, 40);
    // 10 content lines 'A'..'J', cursor ends on row 9; rows 10..39 are blank padding.
    s.Feed("A\n\rB\n\rC\n\rD\n\rE\n\rF\n\rG\n\rH\n\rI\n\rJ");
    TR_ASSERT(t, s.GetCursorPos().y == 9);
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 0);

    // Shrink to 20 rows - the 10 content rows still fit, so nothing should scroll off.
    s.Resize(20, 20);
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 0);
    TR_ASSERT(t, s.GetCursorPos().y == 9);
    for (int y = 0; y < 10; y++) {
        TR_ASSERT(t, s.GetRow(y)[0].ch == (char32_t)(U'A' + y));
    }

    // Grow back - still no scrollback, content unchanged.
    s.Resize(20, 40);
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 0);
    TR_ASSERT(t, s.GetCursorPos().y == 9);
    for (int y = 0; y < 10; y++) {
        TR_ASSERT(t, s.GetRow(y)[0].ch == (char32_t)(U'A' + y));
    }

    // Shrink BELOW the content height: only then may the top rows spill into scrollback, and the
    // cursor must ride the bottom of the visible content.
    s.Resize(20, 6);
    TR_ASSERT(t, s.Rows() == 6);
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 4);  // 10 content rows - 6 = 4 spilled
    TR_ASSERT(t, s.GetCursorPos().y == 5);
    TR_ASSERT(t, s.GetRow(5)[0].ch == U'J');          // newest content line on the last row
    return kTR_Pass;
}

//
// Regression: squeezing the terminal pane to a zero/negative size once emptied the grid, and a pty-thread write
// into it was a heap use-after-free. Now the size is ignored (the terminal keeps its last real size) while
// Cols()/Rows() report 0 so nothing is drawn; output keeps being processed safely, and nothing is lost.
//
DLL_EXPORT int test_terminalscreen_resize_degenerate(ITesting *t) {
    TerminalScreen s;
    Setup(s, 20, 8);
    s.Feed("X\r\nX\r\nX\r\nX\r\nX\r\nX\r\nX\r\nX");

    for (int rows : {0, -3}) {
        s.Resize(0, rows);
        TR_ASSERT(t, s.Cols() == 0);
        TR_ASSERT(t, s.Rows() == 0);
        TR_ASSERT(t, s.GetRow(0).empty());
        // Whatever the shell sends meanwhile is safe
        s.Feed("Z\n\r\t\x1b[3;3H\x1b[K\x1b[1K\x1b[2K\x1b[J\x1b[2J\x1b[2L\x1b[2M\x1bM\x1b[2;4r\x1b[r");
    }

    // Growing back to a real size restores a usable, correctly-sized grid
    s.Resize(10, 4);
    TR_ASSERT(t, s.Cols() == 10);
    TR_ASSERT(t, s.Rows() == 4);
    TR_ASSERT(t, (int)s.GetRow(0).size() == 10);
    s.Feed("\x1b[HQ");
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'Q');
    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_scroll(ITesting *t) {
    TerminalScreen s;
    Setup(s, 10, 3);   // 3-row screen to trigger scroll quickly

    s.Feed("A\r\nB\r\nC\r\nD");   // the third CR LF scrolls: A pushed to scrollback
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 1);
    TR_ASSERT(t, s.GetScrollback()->LineAt(0)->Buffer()[0] == U'A');

    // Grid now holds B, C, D at column 0
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'B');
    TR_ASSERT(t, s.GetRow(1)[0].ch == U'C');
    TR_ASSERT(t, s.GetRow(2)[0].ch == U'D');

    // One more CR LF scrolls B into scrollback
    s.Feed("\r\nE");
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 2);
    TR_ASSERT(t, s.GetScrollback()->LineAt(1)->Buffer()[0] == U'B');
    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_cr(ITesting *t) {
    TerminalScreen s;
    Setup(s);
    s.Feed("AB");
    TR_ASSERT(t, s.GetCursorPos().x == 2);

    s.Feed("\r");
    TR_ASSERT(t, s.GetCursorPos().x == 0);
    TR_ASSERT(t, s.GetCursorPos().y == 0);  // CR does not advance row

    // Overwrite the first cell
    s.Feed("X");
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'X');
    TR_ASSERT(t, s.GetRow(0)[1].ch == U'B');  // B was not cleared
    return kTR_Pass;
}

// open-bugs.md #11: HT advances the cursor to the next 8-column tab stop - it must not be dropped (the old bug fused
// multi-column shell output like BSD `ls`). Like xterm it only MOVES: the cells it passes keep what they hold.
DLL_EXPORT int test_terminalscreen_tab(ITesting *t) {
    TerminalScreen s;
    Setup(s, 40, 5);

    s.Feed("a\tb");
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'a');
    TR_ASSERT(t, s.GetRow(0)[8].ch == U'b');
    for (int x = 1; x < 8; x++) {
        TR_ASSERT(t, s.GetRow(0)[x].ch == U' ');
        TR_ASSERT(t, s.GetRow(0)[x].fg == WHITE);
        TR_ASSERT(t, s.GetRow(0)[x].bg == BLACK);
    }

    // From column 9 the next stop is 16
    s.Feed("\t");
    TR_ASSERT(t, s.GetCursorPos().x == 16);

    // Passing over content leaves it
    s.Feed("\r\n0123456789\r\t");
    TR_ASSERT(t, s.GetCursorPos().x == 8);
    TR_ASSERT(t, s.GetRow(1)[3].ch == U'3');

    // At the right margin a tab can't move off the row - it clamps to the last column, no wrap.
    s.Feed(MoveTo(38, 0) + "\t");
    TR_ASSERT(t, s.GetCursorPos().x == s.Cols() - 1);
    TR_ASSERT(t, s.GetCursorPos().y == 0);
    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_colors(ITesting *t) {
    TerminalScreen s;
    Setup(s);
    s.Feed(std::string(SGR_RED) + SGR_BG_GREEN + "R");
    TR_ASSERT(t, s.GetRow(0)[0].fg == RED);
    TR_ASSERT(t, s.GetRow(0)[0].bg == GREEN);

    // Reset and write another char - should use defaults
    s.Feed(std::string(SGR_RESET) + "W");
    TR_ASSERT(t, s.GetRow(0)[1].fg == WHITE);
    TR_ASSERT(t, s.GetRow(0)[1].bg == BLACK);

    // Colors don't bleed: first cell still has original colors
    TR_ASSERT(t, s.GetRow(0)[0].fg == RED);

    // Default colours are references: changing them recolours what is already on screen
    s.SetDefaultColors(GREEN, RED);
    TR_ASSERT(t, s.GetRow(0)[1].fg == GREEN);
    TR_ASSERT(t, s.GetRow(0)[1].bg == RED);
    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_cursor(ITesting *t) {
    TerminalScreen s;
    Setup(s);
    s.Feed(MoveTo(3, 2));
    TR_ASSERT(t, s.GetCursorPos().x == 3);
    TR_ASSERT(t, s.GetCursorPos().y == 2);

    s.Feed("\x1b[2C\x1b[1B");
    TR_ASSERT(t, s.GetCursorPos().x == 5);
    TR_ASSERT(t, s.GetCursorPos().y == 3);

    // Clamp at boundaries
    s.Feed("\x1b[100;1H");
    TR_ASSERT(t, s.GetCursorPos().x == 0);
    TR_ASSERT(t, s.GetCursorPos().y == s.Rows() - 1);
    s.Feed("\x1b[1;100H");
    TR_ASSERT(t, s.GetCursorPos().x == s.Cols() - 1);
    TR_ASSERT(t, s.GetCursorPos().y == 0);

    // DECTCEM
    TR_ASSERT(t, s.IsCursorVisible());
    s.Feed("\x1b[?25l");
    TR_ASSERT(t, !s.IsCursorVisible());
    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_erase(ITesting *t) {
    TerminalScreen s;
    Setup(s);
    // Row 0 full of X; the next character wraps, so the Y's fill row 1
    s.Feed(std::string(s.Cols(), 'X') + std::string(s.Cols(), 'Y'));

    // Erase from col 4 to end of row 1
    s.Feed(MoveTo(4, 1) + "\x1b[K");
    TR_ASSERT(t, s.GetRow(1)[3].ch == U'Y');  // before cursor - untouched
    TR_ASSERT(t, s.GetRow(1)[4].ch == U' ');  // erased
    TR_ASSERT(t, s.GetRow(1)[9].ch == U' ');  // erased
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'X');  // row 0 is untouched

    // Clear screen + home
    s.Feed("\x1b[2J\x1b[H");
    for (int y = 0; y < s.Rows(); y++) {
        for (int x = 0; x < s.Cols(); x++) {
            TR_ASSERT(t, s.GetRow(y)[x].ch == U' ');
        }
    }
    TR_ASSERT(t, s.GetCursorPos().x == 0);
    TR_ASSERT(t, s.GetCursorPos().y == 0);
    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_erase_in_line(ITesting *t) {
    TerminalScreen s;
    Setup(s, 10, 5);
    std::string row(s.Cols(), 'X');

    // mode 0: erase from cursor.x to end of line
    s.Feed(row + MoveTo(3, 0) + "\x1b[0K");
    TR_ASSERT(t, s.GetRow(0)[2].ch == U'X');
    TR_ASSERT(t, s.GetRow(0)[3].ch == U' ');
    TR_ASSERT(t, s.GetRow(0)[9].ch == U' ');

    // mode 1: erase from start of line to cursor (inclusive)
    s.Feed(MoveTo(0, 0) + row + MoveTo(4, 0) + "\x1b[1K");
    TR_ASSERT(t, s.GetRow(0)[0].ch == U' ');
    TR_ASSERT(t, s.GetRow(0)[4].ch == U' ');
    TR_ASSERT(t, s.GetRow(0)[5].ch == U'X');

    // mode 2: erase entire line
    s.Feed(MoveTo(5, 0) + "\x1b[2K");
    for (int x = 0; x < s.Cols(); x++) {
        TR_ASSERT(t, s.GetRow(0)[x].ch == U' ');
    }
    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_erase_in_display(ITesting *t) {
    TerminalScreen s;
    Setup(s, 5, 5);
    auto fill = [&]() {
        for (int y = 0; y < s.Rows(); y++) {
            s.Feed(MoveTo(0, y) + "XXXXX");
        }
    };

    // mode 2: clear entire screen, cursor stays
    fill();
    s.Feed(MoveTo(2, 2) + "\x1b[2J");
    for (int y = 0; y < s.Rows(); y++) {
        for (int x = 0; x < s.Cols(); x++) {
            TR_ASSERT(t, s.GetRow(y)[x].ch == U' ');
        }
    }
    TR_ASSERT(t, s.GetCursorPos().x == 2);
    TR_ASSERT(t, s.GetCursorPos().y == 2);

    // mode 0: erase from cursor to end of screen
    fill();
    s.Feed(MoveTo(2, 2) + "\x1b[J");
    TR_ASSERT(t, s.GetRow(1)[4].ch == U'X');  // row above cursor - untouched
    TR_ASSERT(t, s.GetRow(2)[1].ch == U'X');  // before cursor on same row - untouched
    TR_ASSERT(t, s.GetRow(2)[2].ch == U' ');  // cursor pos - erased
    TR_ASSERT(t, s.GetRow(3)[0].ch == U' ');  // row below - erased
    TR_ASSERT(t, s.GetRow(4)[4].ch == U' ');  // last row - erased
    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_scroll_region(ITesting *t) {
    TerminalScreen s;
    Setup(s, 10, 5);

    // Scroll region rows 1..3 (0-indexed), leaving rows 0 and 4 outside. DECSTBM homes the cursor.
    s.Feed("\x1b[2;4r");
    TR_ASSERT(t, s.GetCursorPos().x == 0);
    TR_ASSERT(t, s.GetCursorPos().y == 0);

    s.Feed(MoveTo(0, 1) + "A" + MoveTo(0, 2) + "B" + MoveTo(0, 3) + "C");
    s.Feed(MoveTo(0, 0) + "T" + MoveTo(0, 4) + "B");

    // LF at the bottom of the region scrolls the region only
    s.Feed(MoveTo(0, 3) + "\n");
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'T');
    TR_ASSERT(t, s.GetRow(4)[0].ch == U'B');
    TR_ASSERT(t, s.GetRow(1)[0].ch == U'B');
    TR_ASSERT(t, s.GetRow(2)[0].ch == U'C');
    TR_ASSERT(t, s.GetRow(3)[0].ch == U' ');

    // A was scrolled off, but the region does not start at the top row - not scrollback
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 0);
    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_writeline(ITesting *t) {
    TerminalScreen s;
    Setup(s, 20, 5);

    SimulateWriteLine(s, U"Hello");
    SimulateWriteLine(s, U"World");
    SimulateWriteLine(s, U"Line3");

    // Each line must start at column 0
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'H');
    TR_ASSERT(t, s.GetRow(0)[4].ch == U'o');
    TR_ASSERT(t, s.GetRow(1)[0].ch == U'W');
    TR_ASSERT(t, s.GetRow(1)[4].ch == U'd');
    TR_ASSERT(t, s.GetRow(2)[0].ch == U'L');
    TR_ASSERT(t, s.GetRow(2)[4].ch == U'3');
    TR_ASSERT(t, s.GetCursorPos().x == 0);
    TR_ASSERT(t, s.GetCursorPos().y == 3);

    // After 6 writes on a 5-row grid: scrollback [Hello, World], grid Line3..Line6 + the blank cursor row
    SimulateWriteLine(s, U"Line4");
    SimulateWriteLine(s, U"Line5");
    SimulateWriteLine(s, U"Line6");
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 2);
    TR_ASSERT(t, s.GetScrollback()->LineAt(0)->Buffer()[0] == U'H');
    TR_ASSERT(t, s.GetScrollback()->LineAt(1)->Buffer()[0] == U'W');
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'L');
    TR_ASSERT(t, s.GetRow(3)[0].ch == U'L');
    TR_ASSERT(t, s.GetRow(4)[0].ch == U' ');
    TR_ASSERT(t, s.GetCursorPos().y == 4);
    TR_ASSERT(t, s.GetCursorPos().x == 0);
    return kTR_Pass;
}

// Alternate screen (?1049): entering saves the cursor and shows a clean screen, leaving brings back the primary
// screen exactly as it was
DLL_EXPORT int test_terminalscreen_savestate(ITesting *t) {
    TerminalScreen s;
    Setup(s);
    s.Feed(std::string("AB") + SGR_RED);
    s.Feed(ENTER_ALT);
    TR_ASSERT(t, s.IsAltScreen());
    TR_ASSERT(t, s.GetRow(0)[0].ch == U' ');

    s.Feed("\x1b[2J\x1b[HZ");
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'Z');
    TR_ASSERT(t, s.GetCursorPos().x == 1);

    s.Feed(LEAVE_ALT);
    TR_ASSERT(t, !s.IsAltScreen());
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'A');
    TR_ASSERT(t, s.GetRow(0)[1].ch == U'B');
    TR_ASSERT(t, s.GetCursorPos().x == 2);

    // Leaving again (not on the alternate screen) changes nothing visible
    s.Feed(LEAVE_ALT);
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'A');
    return kTR_Pass;
}

// Property: alt-screen output never reaches the persistent scrollback (docs/terminal-scrollback.md §10). Rows that
// scroll off the alternate screen go nowhere; the real scrollback is unchanged across the whole visit.
DLL_EXPORT int test_terminalscreen_savestate_excludes_scrollback(ITesting *t) {
    TerminalScreen s;
    Setup(s, 10, 3);
    s.Feed("A\r\nB\r\nC\r\n");
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 1);

    s.Feed(ENTER_ALT);
    for (int i = 0; i < 5; i++) {
        s.Feed("X\r\n");
    }
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 1);

    s.Feed(LEAVE_ALT);
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 1);
    TR_ASSERT(t, s.GetScrollback()->LineAt(0)->Buffer()[0] == U'A');
    return kTR_Pass;
}

// Row -> Line conversion (docs/terminal-scrollback.md §3.0) must be lossless for color: maximal runs
// of same fg/bg/attrs map 1:1 onto LineAttrib spans, and trailing blank padding (MakeBlankRow fills
// the full row width) is trimmed exactly like every other Line in the editor.
DLL_EXPORT int test_terminalscreen_scrollback_color_roundtrip(ITesting *t) {
    TerminalScreen s;
    Setup(s, 10, 3);

    s.Feed(SGR_RED);
    s.Feed("R");
    s.Feed("E");
    s.Feed(SGR_GREEN);
    s.Feed(SGR_BOLD);
    s.Feed("D");
    s.Feed("\r\n");
    s.Feed("B\r\n");
    s.Feed("C\r\n");  // scrolls "RED" off into scrollback

    TR_ASSERT(t, s.GetScrollback()->NumLines() == 1);
    auto line = s.GetScrollback()->LineAt(0);
    TR_ASSERT(t, line->Buffer() == U"RED");            // trailing blanks trimmed

    auto &attribs = line->Attributes();
    TR_ASSERT(t, attribs.size() == 2);                 // run "RE" (red) + run "D" (green, bold)
    TR_ASSERT(t, attribs[0].idxOrigString == 0);
    TR_ASSERT(t, attribs[0].foregroundColor == RED);
    TR_ASSERT(t, !(attribs[0].textAttributes & kTextAttributes::kBold));
    TR_ASSERT(t, attribs[1].idxOrigString == 2);
    TR_ASSERT(t, attribs[1].foregroundColor == GREEN);
    TR_ASSERT(t, (attribs[1].textAttributes & kTextAttributes::kBold));

    return kTR_Pass;
}

// Abs-line spine (docs/terminal-scrollback.md §3.1): ids are monotonic across scrollback + grid
// history, RowAtAbs resolves to the right physical row, and out-of-range/the live row resolve empty.
DLL_EXPORT int test_terminalscreen_absrow_spine(ITesting *t) {
    TerminalScreen s;
    Setup(s, 10, 3);

    TR_ASSERT(t, s.ScrollbackBase() == 0);
    TR_ASSERT(t, s.AbsRowCount() == 0);   // nothing written yet, cursor at row 0

    s.Feed("A\r\n");   // cursor.y -> 1
    s.Feed("B\r\n");   // cursor.y -> 2
    s.Feed("C\r\n");   // at scrollRegionBottom -> scrolls A off

    // A -> scrollback; grid is now [B, C, blank], cursor stays on row 2 (the live/cursor row).
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 1);
    TR_ASSERT(t, s.GetCursorPos().y == 2);
    TR_ASSERT(t, s.AbsRowCount() == s.ScrollbackBase() + s.GetScrollback()->NumLines() + s.GetCursorPos().y);
    TR_ASSERT(t, s.AbsRowCount() == 3);

    // abs 0 -> the scrollback line "A"
    auto r0 = s.RowAtAbs(0);
    TR_ASSERT(t, std::holds_alternative<Line::Ref>(r0));
    TR_ASSERT(t, std::get<Line::Ref>(r0)->Buffer()[0] == U'A');

    // abs 1 -> grid history row 0 ("B")
    auto r1 = s.RowAtAbs(1);
    TR_ASSERT(t, std::holds_alternative<const TerminalScreen::Row *>(r1));
    TR_ASSERT(t, (*std::get<const TerminalScreen::Row *>(r1))[0].ch == U'B');

    // abs == AbsRowCount() is the live cursor row - not yet history, resolves empty
    auto rLive = s.RowAtAbs(s.AbsRowCount());
    TR_ASSERT(t, std::holds_alternative<std::monostate>(rLive));

    // Out of range
    auto rOOR = s.RowAtAbs(9999);
    TR_ASSERT(t, std::holds_alternative<std::monostate>(rOOR));

    return kTR_Pass;
}

// Cap + eviction: "terminal.scrollback_lines" bounds in-memory scrollback; the oldest lines go
DLL_EXPORT int test_terminalscreen_scrollback_cap(ITesting *t) {
    auto oldCap = Config::Instance()["terminal"].GetInt("scrollback_lines", 10000);
    Config::Instance()["terminal"].SetInt("scrollback_lines", 3);

    TerminalScreen s;
    Setup(s, 10, 2);
    for (char32_t ch = U'A'; ch <= U'H'; ch++) {
        SimulateWriteLine(s, std::u32string(1, ch));
    }
    // 8 single-char lines on a 2-row grid: A doesn't scroll, B..H each push one row -> 7 pushed
    // (A..G), capped to the 3 most recent (E, F, G); H is still live in the grid.
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 3);
    TR_ASSERT(t, s.GetScrollback()->LineAt(0)->Buffer()[0] == U'E');
    TR_ASSERT(t, s.GetScrollback()->LineAt(1)->Buffer()[0] == U'F');
    TR_ASSERT(t, s.GetScrollback()->LineAt(2)->Buffer()[0] == U'G');

    // 7 pushed - 3 retained = 4 evicted.
    TR_ASSERT(t, s.ScrollbackBase() == 4);

    // Evicted abs ids resolve empty.
    TR_ASSERT(t, std::holds_alternative<std::monostate>(s.RowAtAbs(0)));
    TR_ASSERT(t, std::holds_alternative<std::monostate>(s.RowAtAbs(3)));

    // The oldest RETAINED scrollback line ("E") is still addressable at its abs id.
    auto r4 = s.RowAtAbs(4);
    TR_ASSERT(t, std::holds_alternative<Line::Ref>(r4));
    TR_ASSERT(t, std::get<Line::Ref>(r4)->Buffer()[0] == U'E');

    // Grid history row ("H", one above the live cursor row) resolves too.
    auto r7 = s.RowAtAbs(7);
    TR_ASSERT(t, std::holds_alternative<const TerminalScreen::Row *>(r7));
    TR_ASSERT(t, (*std::get<const TerminalScreen::Row *>(r7))[0].ch == U'H');

    Config::Instance()["terminal"].SetInt("scrollback_lines", oldCap);
    return kTR_Pass;
}

// scrollback_lines == 0 means unlimited - no eviction, scrollbackBase stays put.
DLL_EXPORT int test_terminalscreen_scrollback_cap_unlimited(ITesting *t) {
    auto oldCap = Config::Instance()["terminal"].GetInt("scrollback_lines", 10000);
    Config::Instance()["terminal"].SetInt("scrollback_lines", 0);

    TerminalScreen s;
    Setup(s, 10, 2);
    for (char32_t ch = U'A'; ch <= U'J'; ch++) {   // 10 lines
        SimulateWriteLine(s, std::u32string(1, ch));
    }
    // 9 of the 10 lines (A..I) scrolled off; none evicted.
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 9);
    TR_ASSERT(t, s.ScrollbackBase() == 0);
    TR_ASSERT(t, s.GetScrollback()->LineAt(0)->Buffer()[0] == U'A');

    Config::Instance()["terminal"].SetInt("scrollback_lines", oldCap);
    return kTR_Pass;
}

// The last command's output: from the row below the command line (or the cursor row, OSC 133;C) up to the
// next prompt, across scrollback and the grid; gone once its start is trimmed away; nothing on the alt screen
DLL_EXPORT int test_terminalscreen_last_output(ITesting *t) {
    auto oldCap = Config::Instance()["terminal"].GetInt("scrollback_lines", 10000);
    Config::Instance()["terminal"].SetInt("scrollback_lines", 0);

    TerminalScreen s;
    Setup(s, 20, 3);
    TR_ASSERT(t, s.GetLastOutputText().empty());

    s.Feed("$ make");
    s.MarkOutputStart(false);
    s.Feed("\r\nerror: one\r\n  at two\r\n  at three\r\n$ ");
    auto text = s.GetLastOutputText();
    TR_ASSERT(t, text.size() == 3);                         // two of them in scrollback by now
    TR_ASSERT(t, text[0] == std::u32string(U"error: one"));
    TR_ASSERT(t, text[2] == std::u32string(U"  at three"));

    // OSC 133;C marks the cursor row itself
    s.Feed("ls\r\n");
    s.MarkOutputStart(true);
    s.Feed("a.txt\r\n$ ");
    text = s.GetLastOutputText();
    TR_ASSERT(t, text.size() == 1);
    TR_ASSERT(t, text[0] == std::u32string(U"a.txt"));

    // Not on the alternate screen
    s.Feed("\x1b[?1049h");
    s.MarkOutputStart(false);
    s.Feed("\x1b[?1049l");
    TR_ASSERT(t, s.GetLastOutputText().size() == 1);

    // Trimmed away from under it: no partial output
    Config::Instance()["terminal"].SetInt("scrollback_lines", 1);
    s.Feed("\r\n\r\n\r\n\r\n");
    TR_ASSERT(t, s.GetLastOutputText().empty());

    Config::Instance()["terminal"].SetInt("scrollback_lines", oldCap);
    return kTR_Pass;
}

// SnapshotScrollbackTail + SeedScrollback round-trip: the history lines come back at abs 0, with no last output
DLL_EXPORT int test_terminalscreen_snapshot_roundtrip(ITesting *t) {
    auto oldCap = Config::Instance()["terminal"].GetInt("scrollback_lines", 10000);
    Config::Instance()["terminal"].SetInt("scrollback_lines", 0);   // unlimited - no eviction, base stays 0

    TerminalScreen s;
    Setup(s, 20, 2);   // 2-row grid forces early scroll-off into scrollback
    s.Feed(SGR_RED);
    SimulateWriteLine(s, U"alpha");
    SimulateWriteLine(s, U"beta");
    s.Feed(SGR_GREEN);
    s.MarkOutputStart(false);
    SimulateWriteLine(s, U"gamma");
    SimulateWriteLine(s, U"delta");

    auto sb = s.GetScrollback();
    size_t numLines = sb->NumLines();
    TR_ASSERT(t, numLines > 0);

    auto lines = s.SnapshotScrollbackTail(0);   // all of it: the scrollback, then "delta" still on screen
    TR_ASSERT(t, lines.size() == numLines + 1);
    for (size_t i = 0; i < numLines; i++) {
        TR_ASSERT(t, lines[i]->Buffer() == sb->LineAt(i)->Buffer());
    }
    TR_ASSERT(t, lines.back()->Buffer() == std::u32string(U"delta"));

    TerminalScreen seeded;
    seeded.SeedScrollback(lines);
    TR_ASSERT(t, seeded.GetScrollback()->NumLines() == lines.size());
    for (size_t i = 0; i < lines.size(); i++) {
        TR_ASSERT(t, seeded.GetScrollback()->LineAt(i)->Buffer() == lines[i]->Buffer());
    }
    TR_ASSERT(t, seeded.ScrollbackBase() == 0);
    TR_ASSERT(t, seeded.GetLastOutputText().empty());

    Config::Instance()["terminal"].SetInt("scrollback_lines", oldCap);
    return kTR_Pass;
}

// §10: the alternate screen is NEVER persisted - while IsAltScreen() SnapshotScrollbackTail is the scrollback only,
// not the app's grid.
DLL_EXPORT int test_terminalscreen_snapshot_altscreen_empty(ITesting *t) {
    TerminalScreen s;
    Setup(s, 20, 2);
    SimulateWriteLine(s, U"one");
    SimulateWriteLine(s, U"two");
    SimulateWriteLine(s, U"three");
    TR_ASSERT(t, s.GetScrollback()->NumLines() > 0);   // there IS history

    s.Feed(ENTER_ALT);   // enter alt-screen
    TR_ASSERT(t, s.IsAltScreen());
    s.Feed("app screen");
    auto lines = s.SnapshotScrollbackTail(0);
    TR_ASSERT(t, lines.size() == s.GetScrollback()->NumLines());
    for (auto &line : lines) {
        TR_ASSERT(t, line->Buffer() != std::u32string(U"app screen"));
    }
    return kTR_Pass;
}

// §8.1: the on-disk cap keeps only the last N lines of history
DLL_EXPORT int test_terminalscreen_snapshot_caps_tail(ITesting *t) {
    auto oldCap = Config::Instance()["terminal"].GetInt("scrollback_lines", 10000);
    Config::Instance()["terminal"].SetInt("scrollback_lines", 0);   // unlimited in-memory; the cap is on save

    TerminalScreen s;
    Setup(s, 20, 2);
    for (int i = 0; i < 10; i++) {
        std::u32string line = U"line";
        line.push_back((char32_t)(U'0' + i));
        SimulateWriteLine(s, line);
    }
    size_t numLines = s.GetScrollback()->NumLines();
    TR_ASSERT(t, numLines > 3);

    auto lines = s.SnapshotScrollbackTail(3);   // keep only the last 3 lines: two from scrollback, "line9" on screen
    TR_ASSERT(t, lines.size() == 3);
    for (size_t i = 0; i < 2; i++) {
        TR_ASSERT(t, lines[i]->Buffer() == s.GetScrollback()->LineAt(numLines - 2 + i)->Buffer());
    }
    TR_ASSERT(t, lines[2]->Buffer() == std::u32string(U"line9"));

    Config::Instance()["terminal"].SetInt("scrollback_lines", oldCap);
    return kTR_Pass;
}

// What is still on screen at exit is history too: the grid's rows up to the cursor follow the scrollback (trailing
// blank rows dropped), and the cap counts them. On the alternate screen only the scrollback is saved - the grid is
// the app's. (The Mac repro: `ls -laF`, quit, start again - the listing was gone.)
DLL_EXPORT int test_terminalscreen_snapshot_includes_screen(ITesting *t) {
    auto oldCap = Config::Instance()["terminal"].GetInt("scrollback_lines", 10000);
    Config::Instance()["terminal"].SetInt("scrollback_lines", 0);

    TerminalScreen s;
    Setup(s, 20, 5);
    s.Feed("$ ls\r\na.txt\r\nb.txt\r\n$ ");      // fits on screen: nothing has scrolled out
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 0);
    auto lines = s.SnapshotScrollbackTail(0);
    TR_ASSERT(t, lines.size() == 4);
    TR_ASSERT(t, lines[0]->Buffer() == std::u32string(U"$ ls"));
    TR_ASSERT(t, lines[2]->Buffer() == std::u32string(U"b.txt"));
    TR_ASSERT(t, lines[3]->Buffer() == std::u32string(U"$"));

    // Scrollback first, then the screen; the cap keeps the newest
    s.Feed("\r\nc\r\nd\r\ne\r\n$ ");
    TR_ASSERT(t, s.GetScrollback()->NumLines() > 0);
    lines = s.SnapshotScrollbackTail(0);
    TR_ASSERT(t, lines.front()->Buffer() == std::u32string(U"$ ls"));
    TR_ASSERT(t, lines.back()->Buffer() == std::u32string(U"$"));
    auto tail = s.SnapshotScrollbackTail(2);
    TR_ASSERT(t, tail.size() == 2);
    TR_ASSERT(t, tail[0]->Buffer() == std::u32string(U"e"));

    // An empty screen adds nothing
    TerminalScreen empty;
    Setup(empty, 20, 5);
    TR_ASSERT(t, empty.SnapshotScrollbackTail(0).empty());

    // Alternate screen: the scrollback only
    s.Feed(ENTER_ALT);
    s.Feed("vim screen");
    lines = s.SnapshotScrollbackTail(0);
    TR_ASSERT(t, lines.size() == s.GetScrollback()->NumLines());

    Config::Instance()["terminal"].SetInt("scrollback_lines", oldCap);
    return kTR_Pass;
}

// History keeps the default colours as defaults: saved under one theme, restored and drawn under another, a line
// in the default colours takes the new theme's; a colour an app set stays. (The Mac: history saved with a black
// background showed as black boxes once the terminal took the editor's blue.)
DLL_EXPORT int test_terminalscreen_history_follows_theme(ITesting *t) {
    auto oldCap = Config::Instance()["terminal"].GetInt("scrollback_lines", 10000);
    Config::Instance()["terminal"].SetInt("scrollback_lines", 0);
    auto black = ColorRGBA::FromRGB(0, 0, 0);
    auto grey = ColorRGBA::FromRGB(127, 127, 127);
    auto blue = ColorRGBA::FromRGB(20, 30, 60);
    auto white = ColorRGBA::FromRGB(230, 230, 230);

    TerminalScreen s;
    Setup(s, 20, 2);
    s.SetDefaultColors(grey, black);
    s.Feed(std::string("plain\r\n") + SGR_RED + "red\x1b[0m\r\n\x1b[7minverse\x1b[0m\r\nlast");
    auto lines = s.SnapshotScrollbackTail(0);
    TR_ASSERT(t, lines.size() == 4);          // three scrolled out, "last" still on screen

    TerminalScreen restored;
    Setup(restored, 20, 2);
    restored.SetDefaultColors(white, blue);   // another theme
    restored.SeedScrollback(lines);
    auto plain = restored.ResolveHistoryLine(restored.GetScrollback()->LineAt(0));
    TR_ASSERT(t, plain->Attributes()[0].foregroundColor == white);
    TR_ASSERT(t, plain->Attributes()[0].backgroundColor == blue);
    auto red = restored.ResolveHistoryLine(restored.GetScrollback()->LineAt(1));
    TR_ASSERT(t, red->Attributes()[0].foregroundColor == RED);
    TR_ASSERT(t, red->Attributes()[0].backgroundColor == blue);
    auto inverse = restored.ResolveHistoryLine(restored.GetScrollback()->LineAt(2));
    TR_ASSERT(t, inverse->Attributes()[0].foregroundColor == blue);
    TR_ASSERT(t, inverse->Attributes()[0].backgroundColor == white);
    auto last = restored.ResolveHistoryLine(restored.GetScrollback()->LineAt(3));
    TR_ASSERT(t, last->Attributes()[0].backgroundColor == blue);

    // The live scrollback follows a theme change too
    s.SetDefaultColors(white, blue);
    auto live = s.ResolveHistoryLine(s.GetScrollback()->LineAt(0));
    TR_ASSERT(t, live->Attributes()[0].backgroundColor == blue);

    Config::Instance()["terminal"].SetInt("scrollback_lines", oldCap);
    return kTR_Pass;
}

// The TE-1 bug class: a sequence split across two pty reads takes effect as if it arrived whole
DLL_EXPORT int test_terminalscreen_split_sequence(ITesting *t) {
    TerminalScreen s;
    Setup(s);
    s.Feed("\x1b[38;2;25");
    s.Feed("5;0;0mR\xE4\xB8");
    s.Feed("\xAD");
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'R');
    TR_ASSERT(t, s.GetRow(0)[0].fg == RED);
    TR_ASSERT(t, s.GetRow(0)[1].ch == U'中');
    return kTR_Pass;
}

// Indexed colours resolve through the palette; changing it recolours cells already on screen
DLL_EXPORT int test_terminalscreen_palette(ITesting *t) {
    TerminalScreen s;
    Setup(s);
    auto palette = TerminalScreen::DefaultPalette();
    s.Feed("\x1b[31mA\x1b[38;5;196mB\x1b[94mC");
    TR_ASSERT(t, s.GetRow(0)[0].fg == palette[1]);
    TR_ASSERT(t, s.GetRow(0)[1].fg == palette[196]);
    TR_ASSERT(t, s.GetRow(0)[2].fg == palette[12]);
    palette[1] = GREEN;
    s.SetPalette(palette);
    TR_ASSERT(t, s.GetRow(0)[0].fg == GREEN);
    return kTR_Pass;
}

// Inverse and hidden are applied to the resolved colours (the view draws fg/bg as given)
DLL_EXPORT int test_terminalscreen_inverse(ITesting *t) {
    TerminalScreen s;
    Setup(s);
    s.Feed(std::string(SGR_RED) + "\x1b[7mI\x1b[27;8mH\x1b[m\x1b[1;4mU");
    TR_ASSERT(t, s.GetRow(0)[0].fg == BLACK);
    TR_ASSERT(t, s.GetRow(0)[0].bg == RED);
    TR_ASSERT(t, s.GetRow(0)[1].fg == BLACK);     // hidden: drawn in the background colour
    TR_ASSERT(t, s.GetRow(0)[2].attrs == (TerminalScreen::kAttrBold | TerminalScreen::kAttrUnderline));
    // Dim: the foreground half way to the background; with inverse, the dimmed colour becomes the background
    s.Feed("\x1b[m\x1b[2mD\x1b[7mE");
    auto grey = ColorRGBA::FromRGB(0.5f, 0.5f, 0.5f);
    TR_ASSERT(t, s.GetRow(0)[3].fg == grey);
    TR_ASSERT(t, s.GetRow(0)[3].bg == BLACK);
    TR_ASSERT(t, s.GetRow(0)[4].fg == BLACK);
    TR_ASSERT(t, s.GetRow(0)[4].bg == grey);
    return kTR_Pass;
}

// A wide character takes two cells; its right half is width 0 and is not text
DLL_EXPORT int test_terminalscreen_wide(ITesting *t) {
    TerminalScreen s;
    Setup(s, 10, 2);
    s.Feed("a中b\r\n\r\n");
    auto line = s.GetScrollback()->LineAt(0);
    TR_ASSERT(t, line->Buffer() == std::u32string(U"a中b"));

    s.Feed("x中y");
    auto &row = s.GetRow(1);
    TR_ASSERT(t, row[1].ch == U'中');
    TR_ASSERT(t, row[1].width == 2);
    TR_ASSERT(t, row[2].width == 0);
    TR_ASSERT(t, row[2].ch == 0);
    TR_ASSERT(t, row[3].ch == U'y');
    TR_ASSERT(t, s.GetCursorPos().x == 4);
    return kTR_Pass;
}

// Replies and OSC events go to the host; scrollback stays with the screen
namespace {
    class RecordingHost : public gnilk::vt::ITerminalHost {
    public:
        void WriteToPty(std::string_view bytes) override {
            replies += bytes;
        }
        void OnSemanticPrompt(char kind, std::string_view params) override {
            prompts += kind;
        }
        void OnTitle(std::string_view title) override {
            this->title = title;
        }

    public:
        std::string replies;
        std::string prompts;
        std::string title;
    };
}

DLL_EXPORT int test_terminalscreen_host(ITesting *t) {
    TerminalScreen s;
    Setup(s, 10, 3);
    s.Feed("\x1b[c");                                   // no host: dropped, no crash
    RecordingHost host;
    s.SetHost(&host);
    s.Feed("\x1b[2;3H\x1b[6n\x1b]133;A\x07\x1b]0;goat\x07\x1b]11;?\x07");
    TR_ASSERT(t, host.replies.substr(0, 6) == "\x1b[2;3R");
    TR_ASSERT(t, host.replies.find("\x1b]11;rgb:0000/0000/0000") != std::string::npos);
    TR_ASSERT(t, host.prompts == "A");
    TR_ASSERT(t, host.title == "goat");
    s.SetHost(nullptr);
    return kTR_Pass;
}

// Output that arrives before the view's first Resize is kept (the shell banner) and lands in place
DLL_EXPORT int test_terminalscreen_before_resize(ITesting *t) {
    TerminalScreen s;
    TR_ASSERT(t, s.Cols() == 0);
    TR_ASSERT(t, s.Rows() == 0);
    s.Feed("banner\r\nline2\r\n$ ");
    s.Resize(20, 2);
    TR_ASSERT(t, s.Cols() == 20);
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 1);   // the cursor's row and the one above fit
    TR_ASSERT(t, s.GetScrollback()->LineAt(0)->Buffer() == std::u32string(U"banner"));
    TR_ASSERT(t, s.GetRow(1)[0].ch == U'$');
    return kTR_Pass;
}

// `clear` sends ED 3: the scrollback goes, abs ids stay monotonic, a last output in it goes too
DLL_EXPORT int test_terminalscreen_clear_scrollback(ITesting *t) {
    TerminalScreen s;
    s.Resize(20, 3);
    s.MarkOutputStart(false);
    s.Feed("old\r\na\r\nb\r\nc\r\nd\r\n");
    s.Feed("e\r\nf\r\n");
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 5);
    TR_ASSERT(t, !s.GetLastOutputText().empty());
    auto absBefore = s.AbsRowCount();

    s.Feed("\x1b[H\x1b[2J\x1b[3J$ ");
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 0);
    TR_ASSERT(t, s.ScrollbackBase() == 5);
    TR_ASSERT(t, s.AbsRowCount() == 5);                 // the cursor's row: base + grid row 0
    TR_ASSERT(t, s.AbsRowCount() <= absBefore);
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'$');
    TR_ASSERT(t, s.GetLastOutputText().empty());
    // New history lands after the cleared range
    s.Feed("1\r\n2\r\n3\r\n");
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 1);
    TR_ASSERT(t, s.GetScrollback()->LineAt(0)->Buffer() == std::u32string(U"$ 1"));
    return kTR_Pass;
}
