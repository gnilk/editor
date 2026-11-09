//
// OverviewMap - see OverviewMap.h
//
#include "OverviewMap.h"

#include <algorithm>
#include <cmath>

#include "Core/LineDiff.h"

using namespace gedit;

OverviewMap::OverviewMap(size_t numLines, int numRows, double maxRowsPerLine) : numLines(numLines), numRows(numRows) {
    if ((numLines == 0) || (numRows <= 0)) {
        this->numRows = std::max(numRows, 0);
        return;
    }
    rowsPerLine = std::min(maxRowsPerLine, (double)numRows / (double)numLines);
}

int OverviewMap::RowOfLine(size_t idxLine) const {
    if (rowsPerLine <= 0) {
        return 0;
    }
    idxLine = std::min(idxLine, numLines);
    // The epsilon keeps the last line's end on numRows when numRows / numLines doesn't come out exact
    auto row = (int)std::floor((double)idxLine * rowsPerLine + 1e-9);
    return std::min(row, numRows);
}

// The line covering 'row' (the last line starting at or above it), then back to the first line sharing its row - with
// several lines a row, the lines before the last have no rows of their own. Searched with RowOfLine itself, so the two
// never disagree on a boundary.
size_t OverviewMap::LineOfRow(int row) const {
    if (numLines == 0) {
        return 0;
    }
    auto idxNextLine = FirstLineFromRow(row + 1);
    auto idxCovering = (idxNextLine > 0) ? std::min(idxNextLine - 1, numLines - 1) : 0;
    return FirstLineFromRow(RowOfLine(idxCovering));
}

OverviewMap::Box OverviewMap::BoxOf(size_t idxTopLine, size_t numVisibleLines) const {
    Box box;
    if ((numLines == 0) || (numRows <= 0)) {
        return box;
    }
    box.top = std::min(RowOfLine(idxTopLine), numRows - 1);
    box.bottom = std::max(RowOfLine(idxTopLine + numVisibleLines), box.top + 1);
    return box;
}

int OverviewMap::GrabOffset(const Box &box, int pointerRow) {
    if (box.Contains(pointerRow)) {
        return pointerRow - box.top;
    }
    return box.Height() / 2;
}

size_t OverviewMap::TopLineFor(int pointerRow, int grabOffset, size_t numVisibleLines) const {
    if (numLines == 0) {
        return 0;
    }
    auto boxTop = pointerRow - grabOffset;
    size_t idxTopLine = (boxTop <= 0) ? 0 : LineOfRow(boxTop);
    size_t maxTopLine = (numLines > numVisibleLines) ? (numLines - numVisibleLines) : 0;
    return std::min(idxTopLine, maxTopLine);
}

std::vector<uint8_t> OverviewMap::MarksPerRow(const std::vector<uint8_t> &lineChanges, const std::vector<size_t> &searchLines,
                                              size_t idxActiveLine) const {
    std::vector<uint8_t> marks((size_t)numRows, kNone);
    if (numLines == 0) {
        return marks;
    }
    auto numChanged = std::min(lineChanges.size(), numLines);
    for (size_t idxLine = 0; idxLine < numChanged; idxLine++) {
        auto change = lineChanges[idxLine];
        if (change == LineDiff::kNone) {
            continue;
        }
        if (change & LineDiff::kAdded) {
            MarkLine(marks, idxLine, kAdded);
        }
        if (change & LineDiff::kModified) {
            MarkLine(marks, idxLine, kModified);
        }
        // A removal is an edge, not a line: the line's first row for one above it, its last row for one below
        if (change & LineDiff::kRemovedAbove) {
            MarkRow(marks, RowOfLine(idxLine), kRemoved);
        }
        if (change & LineDiff::kRemovedBelow) {
            MarkRow(marks, std::max(RowOfLine(idxLine + 1) - 1, RowOfLine(idxLine)), kRemoved);
        }
    }
    for (auto idxLine : searchLines) {
        MarkLine(marks, idxLine, kSearch);
    }
    MarkLine(marks, idxActiveLine, kActiveLine);
    return marks;
}

// The first line starting at or below 'row'; numLines when none does
size_t OverviewMap::FirstLineFromRow(int row) const {
    size_t first = 0;
    size_t count = numLines + 1;
    while (count > 0) {
        size_t step = count / 2;
        size_t idxLine = first + step;
        if (RowOfLine(idxLine) < row) {
            first = idxLine + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first;
}

// Every row of the line - a line two rows high is marked on both - and at least its first
void OverviewMap::MarkLine(std::vector<uint8_t> &marks, size_t idxLine, kMark mark) const {
    if (idxLine >= numLines) {
        return;
    }
    auto first = RowOfLine(idxLine);
    auto end = std::max(RowOfLine(idxLine + 1), first + 1);
    for (int row = first; row < end; row++) {
        MarkRow(marks, row, mark);
    }
}

void OverviewMap::MarkRow(std::vector<uint8_t> &marks, int row, kMark mark) {
    if ((row < 0) || ((size_t)row >= marks.size())) {
        return;
    }
    marks[(size_t)row] = std::max(marks[(size_t)row], (uint8_t)mark);
}
