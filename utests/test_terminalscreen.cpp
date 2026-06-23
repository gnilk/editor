//
// Created by gnilk on 04.06.26.
//
#include <testinterface.h>
#include "Core/TerminalScreen.h"
#include "Core/Config/Config.h"

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
DLL_EXPORT int test_terminalscreen_block_initial_loose_block(ITesting *t);
DLL_EXPORT int test_terminalscreen_block_open_close(ITesting *t);
DLL_EXPORT int test_terminalscreen_block_eviction_drops_whole_block(ITesting *t);
DLL_EXPORT int test_terminalscreen_block_altscreen_excluded(ITesting *t);
DLL_EXPORT int test_terminalscreen_block_single_open_block_not_evicted(ITesting *t);
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

DLL_EXPORT int test_terminalscreen_resize(ITesting *t) {
    // Put a recognisable char at the start of each of the 5 rows
    auto s = MakeScreen(10, 5);
    for (int y = 0; y < 5; y++) {
        s.PutChar(U'A' + y);            // row y, col 0 == 'A'+y
        s.CarriageReturn();
        if (y < 4) { s.NewLine(); }
    }
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
    auto s2 = MakeScreen(10, 5);
    for (int y = 0; y < 5; y++) {
        s2.PutChar(U'A' + y);
        s2.CarriageReturn();
        if (y < 4) { s2.NewLine(); }
    }
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
// shows. The old resize kept the bottom `newRows` grid rows regardless of the cursor, so when there
// was blank padding below a short prompt a shrink scrolled the REAL content into scrollback and kept
// blanks. Here: 10 lines of content with the cursor on line 9 and lots of blank rows below; a shrink
// that still has room for the content must NOT touch scrollback and must keep the content + cursor.
//
DLL_EXPORT int test_terminalscreen_resize_keeps_content(ITesting *t) {
    auto s = MakeScreen(20, 40);
    // 10 content lines 'A'..'J', cursor ends on row 9; rows 10..39 are blank padding.
    for (int y = 0; y < 10; y++) {
        s.PutChar((char32_t)(U'A' + y));
        if (y < 9) { s.NewLine(); s.CarriageReturn(); }
    }
    TR_ASSERT(t, s.GetCursorPos().y == 9);
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 0);

    // Shrink to 20 rows - the 10 content rows still fit, so nothing should scroll off.
    s.Resize(20, 20);
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 0); // (old code scrolled 20 rows here)
    TR_ASSERT(t, s.GetCursorPos().y == 9);            // cursor/content stays put
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
// Regression: resizing the terminal pane to a degenerate (zero/negative) size empties the grid.
// The pty-reader thread keeps feeding ANSI sequences that mutate the grid; if cols/rows didn't
// track the (now-empty) grid those mutators would write past it -> heap use-after-free (the crash
// seen when shrinking the window so the terminal pane is squeezed to nothing). After a degenerate
// resize cols/rows must be 0 so every mutator no-ops instead of touching freed memory.
//
DLL_EXPORT int test_terminalscreen_resize_degenerate(ITesting *t) {
    auto s = MakeScreen(20, 8);
    for (int y = 0; y < 8; y++) {
        s.PutChar(U'X');
        if (y < 7) { s.NewLine(); }
    }

    // Squeeze to nothing - both the zero and negative paths must leave a consistent empty state.
    for (int rows : {0, -3}) {
        s.Resize(0, rows);
        TR_ASSERT(t, s.Cols() == 0);
        TR_ASSERT(t, s.Rows() == 0);

        // Every grid mutator the pty thread can reach must be a safe no-op now (no crash, no UAF).
        s.PutChar(U'Z');
        s.NewLine();
        s.CarriageReturn();
        s.MoveCursor(3, 3);
        s.SetCursorPos(5, 5);
        s.EraseInLine(0);
        s.EraseInLine(1);
        s.EraseInLine(2);
        s.EraseInDisplay(0);
        s.EraseInDisplay(2);
        s.InsertLine(2);
        s.DeleteLine(2);
        s.ReverseIndex();
        s.SetScrollRegion(1, 4);
    }

    // Growing back to a real size must restore a usable, correctly-sized grid.
    s.Resize(10, 4);
    TR_ASSERT(t, s.Cols() == 10);
    TR_ASSERT(t, s.Rows() == 4);
    TR_ASSERT(t, (int)s.GetRow(0).size() == 10);
    s.PutChar(U'Q');
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'Q');

    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_scroll(ITesting *t) {
    auto s = MakeScreen(10, 3);  // 3-row screen to trigger scroll quickly

    // Use \r\n to write one char at column 0 per row
    s.PutChar(U'A'); s.CarriageReturn(); s.NewLine();
    s.PutChar(U'B'); s.CarriageReturn(); s.NewLine();
    s.PutChar(U'C'); s.CarriageReturn(); s.NewLine();  // scrolls: A pushed to scrollback
    s.PutChar(U'D');

    TR_ASSERT(t, s.GetScrollback()->NumLines() == 1);
    TR_ASSERT(t, s.GetScrollback()->LineAt(0)->Buffer()[0] == U'A');

    // Grid now holds B, C, D at column 0
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'B');
    TR_ASSERT(t, s.GetRow(1)[0].ch == U'C');
    TR_ASSERT(t, s.GetRow(2)[0].ch == U'D');

    // One more \r\n scrolls B into scrollback
    s.CarriageReturn(); s.NewLine();
    s.PutChar(U'E');
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 2);
    TR_ASSERT(t, s.GetScrollback()->LineAt(1)->Buffer()[0] == U'B');

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

DLL_EXPORT int test_terminalscreen_erase_in_line(ITesting *t) {
    auto s = MakeScreen(10, 5);

    // Fill row 0 with 'X'
    for (int i = 0; i < s.Cols(); i++) { s.PutChar(U'X'); }
    s.CarriageReturn();

    // mode 0: erase from cursor.x to end of line
    s.SetCursorPos(3, 0);
    s.EraseInLine(0);
    TR_ASSERT(t, s.GetRow(0)[2].ch == U'X');  // before cursor — untouched
    TR_ASSERT(t, s.GetRow(0)[3].ch == U' ');  // erased
    TR_ASSERT(t, s.GetRow(0)[9].ch == U' ');  // erased

    // Re-fill row 0
    s.SetCursorPos(0, 0);
    for (int i = 0; i < s.Cols(); i++) { s.PutChar(U'X'); }

    // mode 1: erase from start of line to cursor (inclusive)
    s.SetCursorPos(4, 0);
    s.EraseInLine(1);
    TR_ASSERT(t, s.GetRow(0)[0].ch == U' ');  // erased
    TR_ASSERT(t, s.GetRow(0)[4].ch == U' ');  // erased (inclusive)
    TR_ASSERT(t, s.GetRow(0)[5].ch == U'X');  // after cursor — untouched

    // mode 2: erase entire line
    s.SetCursorPos(5, 0);
    s.EraseInLine(2);
    for (int x = 0; x < s.Cols(); x++) {
        TR_ASSERT(t, s.GetRow(0)[x].ch == U' ');
    }

    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_erase_in_display(ITesting *t) {
    auto s = MakeScreen(5, 5);

    // Fill entire grid with 'X'
    for (int y = 0; y < s.Rows(); y++) {
        s.SetCursorPos(0, y);
        for (int x = 0; x < s.Cols(); x++) { s.PutChar(U'X'); }
    }

    // mode 2: clear entire screen, cursor stays
    s.SetCursorPos(2, 2);
    s.EraseInDisplay(2);
    for (int y = 0; y < s.Rows(); y++) {
        for (int x = 0; x < s.Cols(); x++) {
            TR_ASSERT(t, s.GetRow(y)[x].ch == U' ');
        }
    }
    TR_ASSERT(t, s.GetCursorPos().x == 2);  // cursor unchanged
    TR_ASSERT(t, s.GetCursorPos().y == 2);

    // Refill
    for (int y = 0; y < s.Rows(); y++) {
        s.SetCursorPos(0, y);
        for (int x = 0; x < s.Cols(); x++) { s.PutChar(U'X'); }
    }

    // mode 0: erase from cursor to end of screen
    s.SetCursorPos(2, 2);
    s.EraseInDisplay(0);
    TR_ASSERT(t, s.GetRow(1)[4].ch == U'X');  // row above cursor — untouched
    TR_ASSERT(t, s.GetRow(2)[1].ch == U'X');  // before cursor on same row — untouched
    TR_ASSERT(t, s.GetRow(2)[2].ch == U' ');  // cursor pos — erased
    TR_ASSERT(t, s.GetRow(3)[0].ch == U' ');  // row below — erased
    TR_ASSERT(t, s.GetRow(4)[4].ch == U' ');  // last row — erased

    return kTR_Pass;
}

DLL_EXPORT int test_terminalscreen_scroll_region(ITesting *t) {
    auto s = MakeScreen(10, 5);

    // Set scroll region to rows 1..3 (0-indexed), leaving rows 0 and 4 outside
    s.SetScrollRegion(1, 3);
    TR_ASSERT(t, s.ScrollRegionTop()    == 1);
    TR_ASSERT(t, s.ScrollRegionBottom() == 3);
    // SetScrollRegion homes the cursor
    TR_ASSERT(t, s.GetCursorPos().x == 0);
    TR_ASSERT(t, s.GetCursorPos().y == 0);

    // Write one char to each row inside the region
    s.SetCursorPos(0, 1); s.PutChar(U'A');
    s.SetCursorPos(0, 2); s.PutChar(U'B');
    s.SetCursorPos(0, 3); s.PutChar(U'C');
    // Write to rows outside the region
    s.SetCursorPos(0, 0); s.PutChar(U'T');
    s.SetCursorPos(0, 4); s.PutChar(U'B');

    // Force a scroll by triggering NewLine at the bottom of the region
    s.SetCursorPos(0, 3);
    s.NewLine();  // cursor is at scrollRegionBottom → ScrollRegionUp

    // Rows 0 and 4 (outside region) are preserved
    TR_ASSERT(t, s.GetRow(0)[0].ch == U'T');
    TR_ASSERT(t, s.GetRow(4)[0].ch == U'B');

    // Inside region: A scrolled off (gone, scroll region top != 0 → no scrollback),
    // B moved to row 1, C moved to row 2, row 3 blanked
    TR_ASSERT(t, s.GetRow(1)[0].ch == U'B');
    TR_ASSERT(t, s.GetRow(2)[0].ch == U'C');
    TR_ASSERT(t, s.GetRow(3)[0].ch == U' ');

    // A was scrolled off but scrollRegionTop != 0 so it doesn't go to scrollback
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 0);

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
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 2);
    TR_ASSERT(t, s.GetScrollback()->LineAt(0)->Buffer()[0] == U'H');  // "Hello"
    TR_ASSERT(t, s.GetScrollback()->LineAt(1)->Buffer()[0] == U'W');  // "World"
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

// Property: alt-screen output must never reach the persistent scrollback (docs/terminal-scrollback.md
// §10). Rows that scroll off while isAltScreen land in the alt-screen's own (fresh) scrollback, which
// RestoreScreen discards wholesale - the real scrollback is unchanged across the save/restore cycle.
DLL_EXPORT int test_terminalscreen_savestate_excludes_scrollback(ITesting *t) {
    auto s = MakeScreen(10, 3);

    // Scroll one real line into scrollback before entering the alt screen.
    s.PutChar(U'A'); s.CarriageReturn(); s.NewLine();
    s.PutChar(U'B'); s.CarriageReturn(); s.NewLine();
    s.PutChar(U'C'); s.CarriageReturn(); s.NewLine();
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 1);

    s.SaveScreen();
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 0);  // fresh, empty scrollback for the alt screen

    // Drive enough scrolling in alt-screen mode to prove it does NOT leak into the saved scrollback.
    for (int i = 0; i < 5; i++) {
        s.PutChar(U'X'); s.CarriageReturn(); s.NewLine();
    }
    TR_ASSERT(t, s.GetScrollback()->NumLines() > 0);   // alt-screen's own scrollback did accumulate...

    s.RestoreScreen();
    // ...but it is discarded - the real scrollback is exactly as it was before SaveScreen().
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 1);
    TR_ASSERT(t, s.GetScrollback()->LineAt(0)->Buffer()[0] == U'A');

    return kTR_Pass;
}

// Row -> Line conversion (docs/terminal-scrollback.md §3.0) must be lossless for color: maximal runs
// of same fg/bg/attrs map 1:1 onto LineAttrib spans, and trailing blank padding (MakeBlankRow fills
// the full row width) is trimmed exactly like every other Line in the editor.
DLL_EXPORT int test_terminalscreen_scrollback_color_roundtrip(ITesting *t) {
    auto s = MakeScreen(10, 3);

    s.SetForeground(RED);
    s.PutChar(U'R');
    s.PutChar(U'E');
    s.SetForeground(GREEN);
    s.SetAttributes(TerminalScreen::kAttrBold);
    s.PutChar(U'D');
    s.CarriageReturn(); s.NewLine();
    s.PutChar(U'B'); s.CarriageReturn(); s.NewLine();
    s.PutChar(U'C'); s.CarriageReturn(); s.NewLine();  // scrolls "RED" off into scrollback

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
    auto s = MakeScreen(10, 3);

    TR_ASSERT(t, s.ScrollbackBase() == 0);
    TR_ASSERT(t, s.AbsRowCount() == 0);   // nothing written yet, cursor at row 0

    s.PutChar(U'A'); s.CarriageReturn(); s.NewLine();   // cursor.y -> 1
    s.PutChar(U'B'); s.CarriageReturn(); s.NewLine();   // cursor.y -> 2
    s.PutChar(U'C'); s.CarriageReturn(); s.NewLine();   // at scrollRegionBottom -> scrolls A off

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

// Cap + eviction (docs/terminal-scrollback.md §7): "terminal.scrollback_lines" bounds in-memory
// scrollback. Eviction is BLOCK-granular (TS-1a) - it drops the oldest whole block. Each letter here
// opens+closes its own one-line block (mirroring how CommitLine opens a block per command, TS-1b), so
// the block boundaries line up 1:1 with the rows and this produces the exact same retained/evicted
// rows as the old line-granular behaviour - block-granular eviction must still hold the cap.
DLL_EXPORT int test_terminalscreen_scrollback_cap(ITesting *t) {
    auto oldCap = Config::Instance()["terminal"].GetInt("scrollback_lines", 10000);
    Config::Instance()["terminal"].SetInt("scrollback_lines", 3);

    auto s = MakeScreen(10, 2);
    for (char32_t ch = U'A'; ch <= U'H'; ch++) {
        s.BeginCommandBlock(std::u32string(1, ch), TerminalScreen::CommandBlock::Source::kHeuristic);
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

    // Block-granular eviction actually ran: the loose block + blocks A-D are gone (5 of 9 dropped),
    // leaving E, F, G (closed) and H (still open) - a retained block is always complete.
    TR_ASSERT(t, s.Blocks().size() == 4);
    TR_ASSERT(t, s.Blocks().front().command == std::u32string(1, U'E'));
    TR_ASSERT(t, s.Blocks().back().command == std::u32string(1, U'H'));
    TR_ASSERT(t, !s.Blocks().back().endAbsRow.has_value());   // H is still the open/live block

    Config::Instance()["terminal"].SetInt("scrollback_lines", oldCap);
    return kTR_Pass;
}

// TS-1a: the constructor opens an implicit loose block (Source::kHeuristic, empty command) covering
// row 0 up front - "every scrollback line belongs to some block" (§7) holds even before the first
// real command is ever committed.
DLL_EXPORT int test_terminalscreen_block_initial_loose_block(ITesting *t) {
    auto s = MakeScreen(10, 5);

    TR_ASSERT(t, s.Blocks().size() == 1);
    auto &loose = s.Blocks().front();
    TR_ASSERT(t, loose.command.empty());
    TR_ASSERT(t, loose.source == TerminalScreen::CommandBlock::Source::kHeuristic);
    TR_ASSERT(t, loose.startAbsRow == 0);
    TR_ASSERT(t, !loose.endAbsRow.has_value());   // still open - nothing has closed it yet

    return kTR_Pass;
}

// TS-1a: a committed command opens exactly one block; the next command closes the prior one at
// exactly the abs row the new block starts (no gap, no overlap).
DLL_EXPORT int test_terminalscreen_block_open_close(ITesting *t) {
    auto s = MakeScreen(20, 5);

    s.BeginCommandBlock(U"first", TerminalScreen::CommandBlock::Source::kCommitLine);
    TR_ASSERT(t, s.Blocks().size() == 2);   // loose (now closed) + "first" (open)
    TR_ASSERT(t, s.Blocks()[0].endAbsRow.has_value());
    TR_ASSERT(t, !s.Blocks()[1].endAbsRow.has_value());
    TR_ASSERT(t, s.Blocks()[1].command == std::u32string(U"first"));

    SimulateWriteLine(s, U"output of first");

    s.BeginCommandBlock(U"second", TerminalScreen::CommandBlock::Source::kCommitLine);
    TR_ASSERT(t, s.Blocks().size() == 3);
    TR_ASSERT(t, s.Blocks()[1].endAbsRow.has_value());          // "first" now closed
    TR_ASSERT(t, s.Blocks()[1].endAbsRow.value() == s.Blocks()[2].startAbsRow);   // abuts exactly
    TR_ASSERT(t, !s.Blocks()[2].endAbsRow.has_value());         // "second" is the new open block

    return kTR_Pass;
}

// TS-1a/§10: no block opens or closes while IsAltScreen() - alt-screen rows are never grouped, and
// the block that launched the full-screen app (e.g. "vim foo") stays open across the whole session.
DLL_EXPORT int test_terminalscreen_block_altscreen_excluded(ITesting *t) {
    auto s = MakeScreen(20, 5);

    s.BeginCommandBlock(U"vim foo", TerminalScreen::CommandBlock::Source::kCommitLine);
    auto blocksBeforeAlt = s.Blocks().size();

    s.SaveScreen();   // enters alt-screen
    TR_ASSERT(t, s.IsAltScreen());

    // Attempts to open/close blocks while alt-screen is active are no-ops.
    s.BeginCommandBlock(U"should not open", TerminalScreen::CommandBlock::Source::kCommitLine);
    s.EndOpenBlock(0);
    TR_ASSERT(t, s.Blocks().size() == blocksBeforeAlt);
    TR_ASSERT(t, !s.Blocks().back().endAbsRow.has_value());   // "vim foo" is still open

    s.RestoreScreen();
    TR_ASSERT(t, !s.IsAltScreen());
    TR_ASSERT(t, s.Blocks().size() == blocksBeforeAlt);
    TR_ASSERT(t, s.Blocks().back().command == std::u32string(U"vim foo"));
    TR_ASSERT(t, !s.Blocks().back().endAbsRow.has_value());   // closes at the NEXT real prompt, not here

    return kTR_Pass;
}

// §7 consequence (documented, not a bug): block-granular eviction can never behead the open (tail)
// block, so a single pathological block that alone exceeds the cap is NOT trimmed - "never evict the
// open block" wins over "hold the cap." (The per-block hard-ceiling safety valve for this case is a
// separate, not-yet-built guard - see docs/terminal-scrollback.md §7.)
DLL_EXPORT int test_terminalscreen_block_single_open_block_not_evicted(ITesting *t) {
    auto oldCap = Config::Instance()["terminal"].GetInt("scrollback_lines", 10000);
    Config::Instance()["terminal"].SetInt("scrollback_lines", 3);

    auto s = MakeScreen(10, 2);
    // Never call BeginCommandBlock - everything lands in the one initial loose block, which stays
    // open the whole time (size() == 1 the entire test), so TrimScrollbackIfNeeded's
    // `blocks.size() > 1` guard never lets it evict.
    for (char32_t ch = U'A'; ch <= U'H'; ch++) {
        SimulateWriteLine(s, std::u32string(1, ch));
    }

    TR_ASSERT(t, s.Blocks().size() == 1);
    TR_ASSERT(t, !s.Blocks().front().endAbsRow.has_value());
    TR_ASSERT(t, s.GetScrollback()->NumLines() == 7);   // A..G, uncapped despite cap == 3
    TR_ASSERT(t, s.ScrollbackBase() == 0);               // nothing evicted

    Config::Instance()["terminal"].SetInt("scrollback_lines", oldCap);
    return kTR_Pass;
}

// scrollback_lines == 0 means unlimited - no eviction, scrollbackBase stays put.
DLL_EXPORT int test_terminalscreen_scrollback_cap_unlimited(ITesting *t) {
    auto oldCap = Config::Instance()["terminal"].GetInt("scrollback_lines", 10000);
    Config::Instance()["terminal"].SetInt("scrollback_lines", 0);

    auto s = MakeScreen(10, 2);
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
