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
    cursor.y++;
    if (cursor.y >= rows) {
        ScrollUp();
        cursor.y = rows - 1;
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
    cursor.x = std::clamp(x, 0, cols - 1);
    cursor.y = std::clamp(y, 0, rows - 1);
}

void TerminalScreen::MoveCursor(int dx, int dy) {
    SetCursorPos(cursor.x + dx, cursor.y + dy);
}

void TerminalScreen::EraseToEndOfLine() {
    auto blank = MakeBlankCell();
    for (int x = cursor.x; x < cols; x++) {
        grid[cursor.y][x] = blank;
    }
}

void TerminalScreen::EraseScreen() {
    grid.assign(rows, MakeBlankRow());
    cursor = {};
}

void TerminalScreen::SaveScreen() {
    savedGrid      = grid;
    savedCursor    = cursor;
    savedPenFg     = penFg;
    savedPenBg     = penBg;
    savedPenAttrs  = penAttrs;
}

void TerminalScreen::RestoreScreen() {
    if (savedGrid.empty()) {
        return;
    }
    grid      = savedGrid;
    cursor    = savedCursor;
    penFg     = savedPenFg;
    penBg     = savedPenBg;
    penAttrs  = savedPenAttrs;
    savedGrid.clear();
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

void TerminalScreen::ScrollUp() {
    scrollback.push_back(std::move(grid[0]));
    grid.erase(grid.begin());
    grid.push_back(MakeBlankRow());
}
