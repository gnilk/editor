//
// gansi cell grid implementation.
//
#include "gansi/CellGrid.h"

#include <algorithm>

using namespace gnilk::ansi;

CellGrid::CellGrid(int cols, int rows) {
    Resize(cols, rows);
}

void CellGrid::Resize(int newCols, int newRows) {
    newCols = std::max(0, newCols);
    newRows = std::max(0, newRows);

    // Build a fresh blank buffer, then copy the overlapping top-left region from the old one so
    // existing content survives a resize and newly exposed cells start blank.
    std::vector<Cell> next(static_cast<size_t>(newCols) * static_cast<size_t>(newRows));

    const int copyCols = std::min(cols, newCols);
    const int copyRows = std::min(rows, newRows);
    for (int y = 0; y < copyRows; ++y) {
        for (int x = 0; x < copyCols; ++x) {
            next[static_cast<size_t>(y) * newCols + x] = cells[static_cast<size_t>(y) * cols + x];
        }
    }

    cells = std::move(next);
    cols = newCols;
    rows = newRows;
}

Cell &CellGrid::At(int col, int row) {
    if (IsEmpty()) {
        scratch = Cell{};
        return scratch;
    }
    col = std::clamp(col, 0, cols - 1);
    row = std::clamp(row, 0, rows - 1);
    return cells[static_cast<size_t>(row) * cols + col];
}

const Cell &CellGrid::At(int col, int row) const {
    if (IsEmpty()) {
        scratch = Cell{};
        return scratch;
    }
    col = std::clamp(col, 0, cols - 1);
    row = std::clamp(row, 0, rows - 1);
    return cells[static_cast<size_t>(row) * cols + col];
}

void CellGrid::Fill(const Cell &cell) {
    std::fill(cells.begin(), cells.end(), cell);
}

void CellGrid::Clear(Color bg) {
    Cell blank;
    blank.ch = U' ';
    blank.fg = kDefaultFg;
    blank.bg = bg;
    blank.attr = Attr::None;
    Fill(blank);
}

void CellGrid::CopyFrom(const CellGrid &other) {
    cols = other.cols;
    rows = other.rows;
    cells = other.cells;
}
