//
// Created by gnilk on 20.10.26.
//
// WrapProjection — pure, stateless coordinate translation between text space (line + char-index, the
// canonical model coordinate) and the wrapped screen rows an editor view draws when soft-wrap is on.
// A logical line longer than the view width is split into segments, one per screen row; the model never
// sees this - cursor, selection and search stay in text coordinates, and the view translates at draw
// (same rule as CharToVisualColumn for tabs).
//
// Tab stops stay LINE-relative across a wrap: a segment's 'visualStart' is the line-relative visual column
// of its first char, which is what LineRender::ExpandTabs takes as 'startCol'. A tab that does not fit on
// a row moves whole to the next row. Every segment holds at least one char (a single char wider than the
// view - a tab with tabSize > width - still gets its own row), so wrapping always makes progress.
//
// Word mode breaks after the last whitespace that fits. A whitespace run that falls on the break hangs off
// the end of the row (it may overflow the width) so the next row starts on a word; a word longer than the
// width falls back to a hard char break.
//
// Not to be confused with the UI layout (ViewLayout / stack / split views) - this only maps text rows.
//

#ifndef EDITOR_WRAPPROJECTION_H
#define EDITOR_WRAPPROJECTION_H

#include <cstddef>
#include <vector>

#include "Core/Line.h"
#include "Core/Point.h"

namespace gedit {

    enum class WrapMode {
        kNone,      // one row per line (long lines clip at the view edge)
        kChar,      // break at the width, mid-word if need be
        kWord,      // break after whitespace; hard break only for a word wider than the view
    };

    // One screen row's slice of a logical line: chars [charStart, charEnd).
    struct WrapSegment {
        size_t charStart = 0;
        size_t charEnd = 0;
        int visualStart = 0;    // line-relative visual column of charStart
    };

    // One visible screen row: which line, and which of that line's segments it shows.
    struct WrapRow {
        size_t idxLine = 0;
        size_t idxSegment = 0;      // 0 == the line's first row (where the gutter prints the line number)
        bool isLastSegment = true;
        WrapSegment segment = {};
    };

    class WrapProjection {
    public:
        // Split one line into its row segments. Always returns at least one segment (an empty line is one
        // empty segment), and the segments tile [0, Length()) contiguously.
        static std::vector<WrapSegment> WrapLine(const Line &line, int width, int tabSize, WrapMode mode);

        // The screen rows of a view whose first row is the first segment of 'idxTopLine', up to 'nRows'
        // rows. The last line may be cut off part-way through its segments.
        static std::vector<WrapRow> ProjectRows(const std::vector<Line::Ref> &lines, size_t idxTopLine, int nRows,
                                                int width, int tabSize, WrapMode mode);

        // Text position (x=charIdx, y=lineIdx) -> screen position (x=column within the row, y=row index in
        // 'rows'). A char index on a segment boundary belongs to the NEXT row (it is that row's first char);
        // the end-of-line position (and beyond) belongs to the line's last row. Returns false when the
        // position is not on any of the given rows.
        static bool TextToScreen(const std::vector<WrapRow> &rows, const std::vector<Line::Ref> &lines,
                                 const Point &textPos, int tabSize, Point &outScreenPos);

        // As TextToScreen, but always yields a position, for overlay ends: a text position above the rows maps to
        // (0, -1), one below them (or not on any row) to (0, rows.size()). Overlays treat rows strictly between
        // their start and end as fully covered, so a selection running out of the view still paints every
        // visible row it spans.
        static Point TextToScreenClamped(const std::vector<WrapRow> &rows, const std::vector<Line::Ref> &lines,
                                         const Point &textPos, int tabSize);

        // Screen position -> text position. The row is clamped into 'rows'; a column past the end of a
        // wrapped (non-last) row lands on that row's last char, past the end of a line's last row lands on
        // end-of-line. Returns (0,0) when there are no rows.
        static Point ScreenToText(const std::vector<WrapRow> &rows, const std::vector<Line::Ref> &lines,
                                  const Point &screenPos, int tabSize);
    };
}

#endif //EDITOR_WRAPPROJECTION_H
