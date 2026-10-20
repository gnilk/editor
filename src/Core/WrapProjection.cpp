//
// Created by gnilk on 20.10.26.
//

#include "Core/WrapProjection.h"

using namespace gedit;

// Visual column after 'ch' when it starts at line-relative column 'col' (tabs advance to the next stop).
static int AdvanceColumn(char32_t ch, int col, int tabSize) {
    if (ch == U'\t') {
        return col + (tabSize - (col % tabSize));
    }
    return col + 1;
}

static bool IsBreakSpace(char32_t ch) {
    return (ch == U' ') || (ch == U'\t');
}

std::vector<WrapSegment> WrapProjection::WrapLine(const Line &line, int width, int tabSize, WrapMode mode) {
    const auto &text = line.Buffer();
    size_t len = text.size();
    std::vector<WrapSegment> segments;

    if ((mode == WrapMode::kNone) || (len == 0)) {
        segments.push_back({0, len, 0});
        return segments;
    }
    if (tabSize < 1) {
        tabSize = 1;
    }
    if (width < 1) {
        width = 1;
    }

    size_t start = 0;
    int startCol = 0;
    while (true) {
        // Take as many chars as fit in 'width' cells - but always at least one, so we make progress.
        size_t end = start;
        int endCol = startCol;
        while (end < len) {
            int next = AdvanceColumn(text[end], endCol, tabSize);
            if (((next - startCol) > width) && (end > start)) {
                break;
            }
            endCol = next;
            ++end;
        }

        if ((end < len) && (mode == WrapMode::kWord)) {
            if (IsBreakSpace(text[end])) {
                // The break falls on whitespace - let the run hang off this row so the next starts on a word.
                while ((end < len) && IsBreakSpace(text[end])) {
                    endCol = AdvanceColumn(text[end], endCol, tabSize);
                    ++end;
                }
            } else {
                // Back up to just after the last whitespace on this row; none -> keep the hard break.
                size_t brk = end;
                while ((brk > start) && !IsBreakSpace(text[brk - 1])) {
                    --brk;
                }
                if (brk > start) {
                    end = brk;
                    endCol = line.CharToVisualColumn(static_cast<int>(end), tabSize);
                }
            }
        }

        segments.push_back({start, end, startCol});
        if (end >= len) {
            break;
        }
        start = end;
        startCol = endCol;
    }
    return segments;
}

std::vector<WrapRow> WrapProjection::ProjectRows(const std::vector<Line::Ref> &lines, size_t idxTopLine, int nRows,
                                                 int width, int tabSize, WrapMode mode) {
    std::vector<WrapRow> rows;
    if (nRows <= 0) {
        return rows;
    }
    for (size_t idxLine = idxTopLine; idxLine < lines.size(); ++idxLine) {
        auto segments = WrapLine(*lines[idxLine], width, tabSize, mode);
        for (size_t idxSegment = 0; idxSegment < segments.size(); ++idxSegment) {
            if (static_cast<int>(rows.size()) >= nRows) {
                return rows;
            }
            rows.push_back({idxLine, idxSegment, (idxSegment + 1) == segments.size(), segments[idxSegment]});
        }
    }
    return rows;
}

bool WrapProjection::TextToScreen(const std::vector<WrapRow> &rows, const std::vector<Line::Ref> &lines,
                                  const Point &textPos, int tabSize, Point &outScreenPos) {
    if ((textPos.y < 0) || (static_cast<size_t>(textPos.y) >= lines.size())) {
        return false;
    }
    size_t charIdx = (textPos.x < 0) ? 0 : static_cast<size_t>(textPos.x);

    for (size_t idxRow = 0; idxRow < rows.size(); ++idxRow) {
        auto &row = rows[idxRow];
        if (row.idxLine != static_cast<size_t>(textPos.y)) {
            continue;
        }
        if (charIdx < row.segment.charStart) {
            continue;
        }
        if ((charIdx >= row.segment.charEnd) && !row.isLastSegment) {
            continue;
        }
        int visualCol = lines[row.idxLine]->CharToVisualColumn(static_cast<int>(charIdx), tabSize);
        outScreenPos = Point(visualCol - row.segment.visualStart, static_cast<int>(idxRow));
        return true;
    }
    return false;
}

Point WrapProjection::TextToScreenClamped(const std::vector<WrapRow> &rows, const std::vector<Line::Ref> &lines,
                                          const Point &textPos, int tabSize) {
    Point screenPos;
    if (TextToScreen(rows, lines, textPos, tabSize, screenPos)) {
        return screenPos;
    }
    // Not on a row - the rows are one contiguous run of text, so it is either before the first or after the last.
    bool isBefore = rows.empty() || (textPos.y < static_cast<int>(rows.front().idxLine)) ||
                    ((textPos.y == static_cast<int>(rows.front().idxLine)) &&
                     (textPos.x < static_cast<int>(rows.front().segment.charStart)));
    if (isBefore) {
        return {0, -1};
    }
    return {0, static_cast<int>(rows.size())};
}

Point WrapProjection::ScreenToText(const std::vector<WrapRow> &rows, const std::vector<Line::Ref> &lines,
                                   const Point &screenPos, int tabSize) {
    if (rows.empty()) {
        return {0, 0};
    }
    int idxRow = screenPos.y;
    if (idxRow < 0) {
        idxRow = 0;
    }
    if (idxRow >= static_cast<int>(rows.size())) {
        idxRow = static_cast<int>(rows.size()) - 1;
    }
    auto &row = rows[idxRow];
    int col = (screenPos.x < 0) ? 0 : screenPos.x;

    auto charIdx = static_cast<size_t>(lines[row.idxLine]->VisualToCharIndex(row.segment.visualStart + col, tabSize));
    if (!row.isLastSegment && (charIdx >= row.segment.charEnd)) {
        // Past the end of a wrapped row - stay on this row (its first char past the end is the next row's).
        charIdx = row.segment.charEnd - 1;
    }
    return {static_cast<int>(charIdx), static_cast<int>(row.idxLine)};
}
