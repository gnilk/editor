//
// Created by gnilk on 04.06.26.
//
#include <testinterface.h>
#include "Core/TerminalScreen.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_terminalscreen(ITesting *t);
DLL_EXPORT int test_terminalscreen_putchar(ITesting *t);
DLL_EXPORT int test_terminalscreen_newline(ITesting *t);
DLL_EXPORT int test_terminalscreen_scroll(ITesting *t);
DLL_EXPORT int test_terminalscreen_cr(ITesting *t);
DLL_EXPORT int test_terminalscreen_colors(ITesting *t);
DLL_EXPORT int test_terminalscreen_cursor(ITesting *t);
DLL_EXPORT int test_terminalscreen_erase(ITesting *t);
DLL_EXPORT int test_terminalscreen_savestate(ITesting *t);
DLL_EXPORT int test_terminalscreen_writeline(ITesting *t);
}

static const ColorRGBA WHITE = ColorRGBA::FromRGB(1.0f, 1.0f, 1.0f);
static const ColorRGBA BLACK = ColorRGBA::FromRGB(0.0f, 0.0f, 0.0f);
static const ColorRGBA RED   = ColorRGBA::FromRGB(1.0f, 0.0f, 0.0f);
static const ColorRGBA GREEN = ColorRGBA::FromRGB(0.0f, 1.0f, 0.0f);

static TerminalScreen MakeScreen(int cols = 10, int rows = 5) {
    TerminalScreen s;
    s.Resize(cols, rows);
    s.SetDefaultColors(WHITE, BLACK);
    return s;
}

DLL_EXPORT int test_terminalscreen(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_putchar(ITesting *t) {
    auto s = MakeScreen();

    s.PutChar(U'A');
    s.PutChar(U'B');
    s.PutChar(U'C');

    TR_ASSERT(t, s.GetRow(0)[0].ch == U'A');
    TR_ASSERT(t, s.GetRow(0)[1].ch == U'B');
    TR_ASSERT(t, s.GetRow(0)[2].ch == U'C');
    TR_ASSERT(t, s.GetCursorPos().x == 3);
    TR_ASSERT(t, s.GetCursorPos().y == 0);

    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_newline(ITesting *t) {
    auto s = MakeScreen();

    // \n alone is a pure vertical move — x stays where it is (terminal convention)
    s.PutChar(U'A');                    // cursor (1,0)
    s.NewLine();                        // cursor (1,1) — x unchanged
    TR_ASSERT(t, s.GetCursorPos().x == 1);
    TR_ASSERT(t, s.GetCursorPos().y == 1);

    // \r\n (CR+LF) is what moves to the start of the next line
    s = MakeScreen();
    s.PutChar(U'A');                    // cursor (1,0)
    s.CarriageReturn();                 // cursor (0,0)
    s.NewLine();                        // cursor (0,1)
    s.PutChar(U'B');                    // cursor (1,1)

    TR_ASSERT(t, s.GetRow(0)[0].ch == U'A');
    TR_ASSERT(t, s.GetRow(1)[0].ch == U'B');
    TR_ASSERT(t, s.GetCursorPos().y == 1);

    // Fill all rows and verify cursor stays on last row
    s = MakeScreen();
    for (int i = 0; i < s.Rows(); i++) {
        s.NewLine();
    }
    TR_ASSERT(t, s.GetCursorPos().y == s.Rows() - 1);

    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_scroll(ITesting *t) {
    auto s = MakeScreen(10, 3);  // 3-row screen to trigger scroll quickly

    // Use \r\n to write one char at column 0 per row
    s.PutChar(U'A'); s.CarriageReturn(); s.NewLine();
    s.PutChar(U'B'); s.CarriageReturn(); s.NewLine();
    s.PutChar(U'C'); s.CarriageReturn(); s.NewLine();  // scrolls: A pushed to scrollback
    s.PutChar(U'D');

    TR_ASSERT(t, s.GetScrollback().size() == 1);
    TR_ASSERT(t, s.GetScrollback()[0][0].ch == U'A');

    // Grid now holds B, C, D at column 0
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'B');
    TR_ASSERT(t, s.GetRow(1)[0].ch == U'C');
    TR_ASSERT(t, s.GetRow(2)[0].ch == U'D');

    // One more \r\n scrolls B into scrollback
    s.CarriageReturn(); s.NewLine();
    s.PutChar(U'E');
    TR_ASSERT(t, s.GetScrollback().size() == 2);
    TR_ASSERT(t, s.GetScrollback()[1][0].ch == U'B');

    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_cr(ITesting *t) {
    auto s = MakeScreen();

    s.PutChar(U'A');
    s.PutChar(U'B');
    TR_ASSERT(t, s.GetCursorPos().x == 2);

    s.CarriageReturn();
    TR_ASSERT(t, s.GetCursorPos().x == 0);
    TR_ASSERT(t, s.GetCursorPos().y == 0);  // CR does not advance row

    // Overwrite the first cell
    s.PutChar(U'X');
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'X');
    TR_ASSERT(t, s.GetRow(0)[1].ch == U'B');  // B was not cleared

    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_colors(ITesting *t) {
    auto s = MakeScreen();

    s.SetForeground(RED);
    s.SetBackground(GREEN);
    s.PutChar(U'R');

    TR_ASSERT(t, s.GetRow(0)[0].fg == RED);
    TR_ASSERT(t, s.GetRow(0)[0].bg == GREEN);

    // Reset and write another char — should use defaults
    s.ResetAttributes();
    s.PutChar(U'W');

    TR_ASSERT(t, s.GetRow(0)[1].fg == WHITE);
    TR_ASSERT(t, s.GetRow(0)[1].bg == BLACK);

    // Colors don't bleed: first cell still has original colors
    TR_ASSERT(t, s.GetRow(0)[0].fg == RED);

    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_cursor(ITesting *t) {
    auto s = MakeScreen();

    s.SetCursorPos(3, 2);
    TR_ASSERT(t, s.GetCursorPos().x == 3);
    TR_ASSERT(t, s.GetCursorPos().y == 2);

    s.MoveCursor(2, 1);
    TR_ASSERT(t, s.GetCursorPos().x == 5);
    TR_ASSERT(t, s.GetCursorPos().y == 3);

    // Clamp at boundaries
    s.SetCursorPos(-5, 100);
    TR_ASSERT(t, s.GetCursorPos().x == 0);
    TR_ASSERT(t, s.GetCursorPos().y == s.Rows() - 1);

    s.SetCursorPos(100, -5);
    TR_ASSERT(t, s.GetCursorPos().x == s.Cols() - 1);
    TR_ASSERT(t, s.GetCursorPos().y == 0);

    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_erase(ITesting *t) {
    auto s = MakeScreen();

    // Fill row 0 with X's — auto-wrap moves cursor to (0,1) after last X
    for (int i = 0; i < s.Cols(); i++) {
        s.PutChar(U'X');
    }
    // Cursor is now at (0,1) via auto-wrap — no extra NewLine needed
    for (int i = 0; i < s.Cols(); i++) {
        s.PutChar(U'Y');
    }

    // Erase from col 4 to end of row 1
    s.SetCursorPos(4, 1);
    s.EraseToEndOfLine();

    TR_ASSERT(t, s.GetRow(1)[3].ch == U'Y');  // before cursor — untouched
    TR_ASSERT(t, s.GetRow(1)[4].ch == U' ');  // erased
    TR_ASSERT(t, s.GetRow(1)[9].ch == U' ');  // erased

    // Row 0 is untouched
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'X');

    // EraseScreen clears everything and resets cursor
    s.EraseScreen();
    for (int y = 0; y < s.Rows(); y++) {
        for (int x = 0; x < s.Cols(); x++) {
            TR_ASSERT(t, s.GetRow(y)[x].ch == U' ');
        }
    }
    TR_ASSERT(t, s.GetCursorPos().x == 0);
    TR_ASSERT(t, s.GetCursorPos().y == 0);

    return kTR_Pass;
}

// Simulate TerminalController::WriteLine: write chars + CR + NL per line.
// This is what Console.WriteLine("text") produces. The key invariant: each
// line must start at column 0 (CR before NL), otherwise successive lines
// get progressively indented by the length of the previous one.
static void SimulateWriteLine(TerminalScreen &s, const std::u32string &str) {
    for (auto ch : str) {
        s.PutChar(ch);
    }
    s.CarriageReturn();
    s.NewLine();
}

DLL_EXPORT int test_terminalscreen_writeline(ITesting *t) {
    auto s = MakeScreen(20, 5);

    SimulateWriteLine(s, U"Hello");
    SimulateWriteLine(s, U"World");
    SimulateWriteLine(s, U"Line3");

    // Each line must start at column 0
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'H');
    TR_ASSERT(t, s.GetRow(0)[1].ch == U'e');
    TR_ASSERT(t, s.GetRow(0)[4].ch == U'o');

    TR_ASSERT(t, s.GetRow(1)[0].ch == U'W');
    TR_ASSERT(t, s.GetRow(1)[4].ch == U'd');

    TR_ASSERT(t, s.GetRow(2)[0].ch == U'L');
    TR_ASSERT(t, s.GetRow(2)[4].ch == U'3');

    // Cursor is at (0, 3) — start of the next line, ready for more output
    TR_ASSERT(t, s.GetCursorPos().x == 0);
    TR_ASSERT(t, s.GetCursorPos().y == 3);

    // Overflow: 5th write fills row 4 then NL scrolls Hello into scrollback.
    // 6th write scrolls World in. After 6 writes on a 5-row grid:
    //   scrollback: [Hello, World]
    //   grid[0..3]:  Line3, Line4, Line5, Line6
    //   grid[4]:     blank (cursor row after scroll)
    SimulateWriteLine(s, U"Line4");
    SimulateWriteLine(s, U"Line5");
    SimulateWriteLine(s, U"Line6");
    TR_ASSERT(t, s.GetScrollback().size() == 2);
    TR_ASSERT(t, s.GetScrollback()[0][0].ch == U'H');  // "Hello"
    TR_ASSERT(t, s.GetScrollback()[1][0].ch == U'W');  // "World"
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'L');  // "Line3"
    TR_ASSERT(t, s.GetRow(1)[0].ch == U'L');  // "Line4"
    TR_ASSERT(t, s.GetRow(2)[0].ch == U'L');  // "Line5"
    TR_ASSERT(t, s.GetRow(3)[0].ch == U'L');  // "Line6"
    TR_ASSERT(t, s.GetRow(4)[0].ch == U' ');  // blank cursor row
    TR_ASSERT(t, s.GetCursorPos().y == 4);
    TR_ASSERT(t, s.GetCursorPos().x == 0);

    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_savestate(ITesting *t) {
    auto s = MakeScreen();

    s.PutChar(U'A');
    s.PutChar(U'B');
    s.SetForeground(RED);

    s.SaveScreen();

    // Modify on top of saved state
    s.EraseScreen();
    s.PutChar(U'Z');
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'Z');
    TR_ASSERT(t, s.GetCursorPos().x == 1);

    // Restore — back to A B with cursor at (2,0)
    s.RestoreScreen();
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'A');
    TR_ASSERT(t, s.GetRow(0)[1].ch == U'B');
    TR_ASSERT(t, s.GetCursorPos().x == 2);

    // RestoreScreen with no saved state is a no-op
    s.RestoreScreen();
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'A');

    return kTR_Pass;
}
