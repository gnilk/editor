//
// Created by gnilk on 04.06.26.
//

#include <algorithm>
#include "TerminalScreen.h"

using namespace gedit;

void TerminalScreen::Resize(int newCols, int newRows) {
    cols = newCols;
    rows = newRows;
    grid.assign(rows, MakeBlankRow());
    cursor = {};
    scrollRegionTop    = 0;
    scrollRegionBottom = rows - 1;
}

void TerminalScreen::SetDefaultColors(ColorRGBA fg, ColorRGBA bg) {
    defaultFg = fg;
    defaultBg = bg;
    penFg = fg;
    penBg = bg;
}

void TerminalScreen::PutChar(char32_t ch) {
    if (cols == 0 || rows == 0) {
        return;
    }
    grid[cursor.y][cursor.x] = {ch, penFg, penBg, penAttrs};
    cursor.x++;
    if (cursor.x >= cols) {
        cursor.x = 0;
        NewLine();
    }
}

void TerminalScreen::NewLine() {
    if (rows == 0) {
        return;
    }
    if (cursor.y == scrollRegionBottom) {
        ScrollRegionUp();
    } else {
        cursor.y++;
        if (cursor.y >= rows) {
            cursor.y = rows - 1;
        }
    }
}

void TerminalScreen::CarriageReturn() {
    cursor.x = 0;
}

void TerminalScreen::SetForeground(ColorRGBA color) {
    penFg = color;
}

void TerminalScreen::SetBackground(ColorRGBA color) {
    penBg = color;
}

void TerminalScreen::ResetAttributes() {
    penFg    = defaultFg;
    penBg    = defaultBg;
    penAttrs = 0;
}

void TerminalScreen::SetAttributes(uint8_t attrs) {
    penAttrs = attrs;
}

const TerminalScreen::Row &TerminalScreen::GetRow(int y) const {
    return grid[y];
}

const std::vector<TerminalScreen::Row> &TerminalScreen::GetScrollback() const {
    return scrollback;
}

Point TerminalScreen::GetCursorPos() const {
    return cursor;
}

int TerminalScreen::Cols() const {
    return cols;
}

int TerminalScreen::Rows() const {
    return rows;
}

void TerminalScreen::SetCursorPos(int x, int y) {
    if (cols == 0 || rows == 0) { return; }
    cursor.x = std::clamp(x, 0, cols - 1);
    cursor.y = std::clamp(y, 0, rows - 1);
}

void TerminalScreen::MoveCursor(int dx, int dy) {
    SetCursorPos(cursor.x + dx, cursor.y + dy);
}

void TerminalScreen::EraseToEndOfLine() {
    EraseInLine(0);
}

void TerminalScreen::EraseScreen() {
    if (cols == 0 || rows == 0) {
        return;
    }
    grid.assign(rows, MakeBlankRow());
    cursor = {};
}

void TerminalScreen::EraseInLine(int mode) {
    if (cols == 0 || rows == 0) { return; }
    auto blank = MakeBlankCell();
    switch (mode) {
        case 0: // cursor to end of line
            for (int x = cursor.x; x < cols; x++) {
                grid[cursor.y][x] = blank;
            }
            break;
        case 1: // start of line to cursor (inclusive)
            for (int x = 0; x <= cursor.x; x++) {
                grid[cursor.y][x] = blank;
            }
            break;
        case 2: // entire line
            grid[cursor.y] = MakeBlankRow();
            break;
    }
}

void TerminalScreen::EraseInDisplay(int mode) {
    if (cols == 0 || rows == 0) { return; }
    switch (mode) {
        case 0: // cursor to end of screen
            EraseInLine(0);
            for (int y = cursor.y + 1; y < rows; y++) {
                grid[y] = MakeBlankRow();
            }
            break;
        case 1: // start of screen to cursor
            for (int y = 0; y < cursor.y; y++) {
                grid[y] = MakeBlankRow();
            }
            EraseInLine(1);
            break;
        case 2: // entire screen — cursor stays (ANSI-correct; use EraseScreen() to also home cursor)
            grid.assign(rows, MakeBlankRow());
            break;
    }
}

void TerminalScreen::SaveCursorPos() {
    savedCursorPos = cursor;
}

void TerminalScreen::RestoreCursorPos() {
    cursor = savedCursorPos;
}

void TerminalScreen::SetScrollRegion(int top, int bottom) {
    if (rows == 0) { return; }
    scrollRegionTop    = std::clamp(top,    0, rows - 1);
    scrollRegionBottom = std::clamp(bottom, 0, rows - 1);
    if (scrollRegionTop >= scrollRegionBottom) {
        // Invalid region — reset to full screen
        scrollRegionTop    = 0;
        scrollRegionBottom = rows - 1;
    }
    // Setting the scroll region homes the cursor (standard terminal behaviour)
    cursor = {};
}

void TerminalScreen::SaveScreen() {
    savedGrid              = grid;
    savedScrollback        = scrollback;
    savedCursor            = cursor;
    savedPenFg             = penFg;
    savedPenBg             = penBg;
    savedPenAttrs          = penAttrs;
    savedScrollRegionTop    = scrollRegionTop;
    savedScrollRegionBottom = scrollRegionBottom;
    // Enter a clean alternate screen
    grid.assign(rows, MakeBlankRow());
    scrollback.clear();
    cursor            = {};
    scrollRegionTop    = 0;
    scrollRegionBottom = rows - 1;
    isAltScreen = true;
}

void TerminalScreen::RestoreScreen() {
    if (savedGrid.empty()) {
        return;
    }
    grid               = savedGrid;
    scrollback         = savedScrollback;
    cursor             = savedCursor;
    penFg              = savedPenFg;
    penBg              = savedPenBg;
    penAttrs           = savedPenAttrs;
    scrollRegionTop    = savedScrollRegionTop;
    scrollRegionBottom = savedScrollRegionBottom;
    savedGrid.clear();
    savedScrollback.clear();
    isAltScreen = false;
}

void TerminalScreen::ReverseIndex() {
    if (cols == 0 || rows == 0) { return; }
    if (cursor.y == scrollRegionTop) {
        // At top of scroll region — insert a blank line at top, bottom line of region is lost
        for (int r = scrollRegionBottom; r > scrollRegionTop; r--) {
            grid[r] = grid[r - 1];
        }
        grid[scrollRegionTop] = MakeBlankRow();
    } else {
        cursor.y = std::max(0, cursor.y - 1);
    }
}

void TerminalScreen::InsertLine(int count) {
    if (cols == 0 || rows == 0) { return; }
    count = std::clamp(count, 1, scrollRegionBottom - cursor.y + 1);
    for (int i = 0; i < count; i++) {
        for (int r = scrollRegionBottom; r > cursor.y; r--) {
            grid[r] = grid[r - 1];
        }
        grid[cursor.y] = MakeBlankRow();
    }
    cursor.x = 0;
}

void TerminalScreen::DeleteLine(int count) {
    if (cols == 0 || rows == 0) { return; }
    count = std::clamp(count, 1, scrollRegionBottom - cursor.y + 1);
    for (int i = 0; i < count; i++) {
        for (int r = cursor.y; r < scrollRegionBottom; r++) {
            grid[r] = grid[r + 1];
        }
        grid[scrollRegionBottom] = MakeBlankRow();
    }
    cursor.x = 0;
}

void TerminalScreen::InsertChar(int count) {
    if (cols == 0 || rows == 0) { return; }
    auto &row = grid[cursor.y];
    auto blank = MakeBlankCell();
    count = std::clamp(count, 1, cols - cursor.x);
    for (int i = 0; i < count; i++) {
        row.insert(row.begin() + cursor.x, blank);
        if ((int)row.size() > cols) {
            row.resize(cols);
        }
    }
}

void TerminalScreen::DeleteChar(int count) {
    if (cols == 0 || rows == 0) { return; }
    auto &row = grid[cursor.y];
    int n = std::min(count, (int)row.size() - cursor.x);
    if (n > 0) {
        row.erase(row.begin() + cursor.x, row.begin() + cursor.x + n);
        while ((int)row.size() < cols) {
            row.push_back(MakeBlankCell());
        }
    }
}

//
// Private
//

TerminalScreen::Cell TerminalScreen::MakeBlankCell() const {
    return {U' ', penFg, penBg, 0};
}

TerminalScreen::Row TerminalScreen::MakeBlankRow() const {
    return Row(cols, MakeBlankCell());
}

void TerminalScreen::ScrollRegionUp() {
    if (cols == 0 || rows == 0) {
        return;
    }
    // Only push to scrollback when the scroll region covers the top of the grid
    if (scrollRegionTop == 0) {
        scrollback.push_back(std::move(grid[scrollRegionTop]));
    }
    for (int y = scrollRegionTop; y < scrollRegionBottom; y++) {
        grid[y] = std::move(grid[y + 1]);
    }
    grid[scrollRegionBottom] = MakeBlankRow();
}
