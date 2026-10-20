//
// Created by gnilk on 31.03.23.
//

#include <algorithm>

#include "Core/UI/Graphics/DrawContext.h"
#include "logger.h"
#include "LineRender.h"
#include "Core/Editor.h"
using namespace gedit;

// Expand tabs to spaces relative to the starting line-relative column.
std::u32string LineRender::ExpandTabs(const std::u32string &in, int startCol, int tabSize) {
    // Fast path: nothing to expand
    if (in.find(U'\t') == std::u32string::npos) {
        return in;
    }
    int ts = (tabSize < 1) ? 1 : tabSize;
    std::u32string out;
    out.reserve(in.size());
    int col = startCol;
    for (char32_t ch : in) {
        if (ch == U'\t') {
            int n = ts - (col % ts);
            out.append(n, U' ');
            col += n;
        } else {
            out.push_back(ch);
            col++;
        }
    }
    return out;
}

void LineRender::DrawRows(const std::vector<Line::Ref> &lines, const std::vector<WrapRow> &rows) {
    for (size_t idxRow = 0; idxRow < rows.size(); ++idxRow) {
        auto &row = rows[idxRow];
        auto line = lines[row.idxLine];
        int y = static_cast<int>(idxRow);

        line->Lock();
        dc.ClearLine(y);
        DrawSegmentWithAttributesAt(y, *line, row.segment);

        // Overlays now carry their own color/alpha (set by the view from the theme) — no app-set
        // selection color here.
        dc.DrawLineOverlays(y);
        line->Release();
    }
}

void LineRender::DrawLine(int x, int y, const Line::Ref line) {
    line->Lock();
    DrawLineWithAttributesAt(x,y, line);
    line->Release();
}


// This is the more advanced drawing routine...
void LineRender::DrawLineWithAttributesAt(int x, int y, const Line::Ref line) {

    // No attributes?  Just dump the string...
    auto &attribs = line->Attributes();
    if (attribs.size() == 0) {
        dc.DrawStringAt(x, y, ExpandTabs(line->Buffer(), 0, tabSize));
        return;
    }
    // We split the line in attribute chunks and draw partial lines...
    int xp = x;

    auto callback = [&xp, x, y, this](const Line::LineAttribIterator &itAttrib, std::u32string &strOut) {
        auto expanded = ExpandTabs(strOut, xp - x, tabSize);
        dc.SetColor(itAttrib->foregroundColor, itAttrib->backgroundColor);
        dc.DrawStringWithAttributesAt(xp, y, itAttrib->textAttributes, expanded);
        xp += expanded.length();
    };

    line->IterateWithAttributes(callback);
}


// Draw one row's slice [charStart, charEnd) of a line at screen x = 0. Tab stops stay line-relative:
// each attribute chunk is expanded from its own line-relative visual column, then shifted left by the
// segment's visualStart so the row starts at the left edge.
void LineRender::DrawSegmentWithAttributesAt(int y, Line &line, const WrapSegment &segment) {

    // No attributes?  Just dump the string...
    auto &attribs = line.Attributes();
    if (attribs.size() == 0) {
        auto text = line.Buffer().substr(segment.charStart, segment.charEnd - segment.charStart);
        dc.DrawStringAt(0, y, ExpandTabs(text, segment.visualStart, tabSize));
        return;
    }

    auto callback = [&line, &segment, y, this](const Line::LineAttribIterator &itAttrib, std::u32string &strOut) {
        size_t chunkStart = itAttrib->idxOrigString;
        size_t from = std::max(chunkStart, segment.charStart);
        size_t to = std::min(chunkStart + strOut.length(), segment.charEnd);
        if (from >= to) {
            return;
        }
        int col = line.CharToVisualColumn(static_cast<int>(from), tabSize);
        auto expanded = ExpandTabs(strOut.substr(from - chunkStart, to - from), col, tabSize);
        dc.SetColor(itAttrib->foregroundColor, itAttrib->backgroundColor);
        dc.DrawStringWithAttributesAt(col - segment.visualStart, y, itAttrib->textAttributes, expanded);
    };

    line.IterateWithAttributes(callback);
}
