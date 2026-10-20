//
// Created by gnilk on 20.10.26.
//
// Soft-wrap step 1: WrapProjection is PURE - lines + width + tabSize + mode in, row segments and
// text<->screen mappings out. No view, no Document. The cases assert the tiling property (every char lands
// on exactly one row, every row makes progress) as well as the concrete break points for char/word mode,
// tab handling, the visible-row projection the gutter and editor share, and the coordinate round-trip.
//
#include <testinterface.h>

#include <string>
#include <vector>

#include "Core/Line.h"
#include "Core/WrapProjection.h"
#include "Core/DocumentViewState.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_wrapprojection(ITesting *t);
DLL_EXPORT int test_wrapprojection_none(ITesting *t);
DLL_EXPORT int test_wrapprojection_empty(ITesting *t);
DLL_EXPORT int test_wrapprojection_char(ITesting *t);
DLL_EXPORT int test_wrapprojection_word(ITesting *t);
DLL_EXPORT int test_wrapprojection_wordhang(ITesting *t);
DLL_EXPORT int test_wrapprojection_wordlong(ITesting *t);
DLL_EXPORT int test_wrapprojection_tabs(ITesting *t);
DLL_EXPORT int test_wrapprojection_tiling(ITesting *t);
DLL_EXPORT int test_wrapprojection_rows(ITesting *t);
DLL_EXPORT int test_wrapprojection_text2screen(ITesting *t);
DLL_EXPORT int test_wrapprojection_screen2text(ITesting *t);
DLL_EXPORT int test_wrapprojection_roundtrip(ITesting *t);
DLL_EXPORT int test_wrapprojection_viewstate(ITesting *t);
DLL_EXPORT int test_wrapprojection_text2screenclamped(ITesting *t);
DLL_EXPORT int test_wrapprojection_scrollintoview(ITesting *t);
DLL_EXPORT int test_wrapprojection_scrollintoview_tallline(ITesting *t);
}

// Note: Line's ctor rtrims, so test strings must not end in whitespace.
static std::vector<WrapSegment> Wrap(const std::u32string &s, int width, WrapMode mode, int tabSize = 4) {
    Line line(s);
    return WrapProjection::WrapLine(line, width, tabSize, mode);
}

static bool IsSegment(const WrapSegment &segment, size_t charStart, size_t charEnd, int visualStart) {
    return (segment.charStart == charStart) && (segment.charEnd == charEnd) && (segment.visualStart == visualStart);
}

// The safety property: segments are non-empty (or the single segment of an empty line) and tile
// [0, len) contiguously - every char is on exactly one row.
static bool IsTiled(const std::vector<WrapSegment> &segments, size_t len) {
    if (segments.empty()) {
        return false;
    }
    if (len == 0) {
        return (segments.size() == 1) && (segments[0].charStart == 0) && (segments[0].charEnd == 0);
    }
    size_t expectedStart = 0;
    for (auto &segment : segments) {
        if ((segment.charStart != expectedStart) || (segment.charEnd <= segment.charStart)) {
            return false;
        }
        expectedStart = segment.charEnd;
    }
    return expectedStart == len;
}

static std::vector<Line::Ref> Lines(const std::vector<std::u32string> &strings) {
    std::vector<Line::Ref> lines;
    for (auto &s : strings) {
        lines.push_back(Line::Create(s));
    }
    return lines;
}

DLL_EXPORT int test_wrapprojection(ITesting *t) {
    return kTR_Pass;
}

// kNone never wraps, whatever the width
DLL_EXPORT int test_wrapprojection_none(ITesting *t) {
    auto segments = Wrap(U"abcdefghij", 4, WrapMode::kNone);
    TR_ASSERT(t, segments.size() == 1);
    TR_ASSERT(t, IsSegment(segments[0], 0, 10, 0));
    return kTR_Pass;
}

// An empty line is one empty row in every mode
DLL_EXPORT int test_wrapprojection_empty(ITesting *t) {
    for (auto mode : {WrapMode::kNone, WrapMode::kChar, WrapMode::kWord}) {
        auto segments = Wrap(U"", 4, mode);
        TR_ASSERT(t, segments.size() == 1);
        TR_ASSERT(t, IsSegment(segments[0], 0, 0, 0));
    }
    return kTR_Pass;
}

DLL_EXPORT int test_wrapprojection_char(ITesting *t) {
    auto segments = Wrap(U"abcdefghij", 4, WrapMode::kChar);
    TR_ASSERT(t, segments.size() == 3);
    TR_ASSERT(t, IsSegment(segments[0], 0, 4, 0));
    TR_ASSERT(t, IsSegment(segments[1], 4, 8, 4));
    TR_ASSERT(t, IsSegment(segments[2], 8, 10, 8));

    // Exact fit is one row, not one full row + an empty one
    segments = Wrap(U"abcd", 4, WrapMode::kChar);
    TR_ASSERT(t, segments.size() == 1);
    TR_ASSERT(t, IsSegment(segments[0], 0, 4, 0));
    return kTR_Pass;
}

// Word mode breaks after the last whitespace that fits; the space stays on the earlier row
DLL_EXPORT int test_wrapprojection_word(ITesting *t) {
    auto segments = Wrap(U"hello world foo", 8, WrapMode::kWord);
    TR_ASSERT(t, segments.size() == 3);
    TR_ASSERT(t, IsSegment(segments[0], 0, 6, 0));      // "hello "
    TR_ASSERT(t, IsSegment(segments[1], 6, 12, 6));     // "world "
    TR_ASSERT(t, IsSegment(segments[2], 12, 15, 12));   // "foo"
    return kTR_Pass;
}

// Whitespace landing exactly on the break hangs off the row, so the next row starts on the word
DLL_EXPORT int test_wrapprojection_wordhang(ITesting *t) {
    auto segments = Wrap(U"hello   world", 5, WrapMode::kWord);
    TR_ASSERT(t, segments.size() == 2);
    TR_ASSERT(t, IsSegment(segments[0], 0, 8, 0));      // "hello" + 3 hanging spaces
    TR_ASSERT(t, IsSegment(segments[1], 8, 13, 8));     // "world"
    return kTR_Pass;
}

// A word wider than the view falls back to a hard break
DLL_EXPORT int test_wrapprojection_wordlong(ITesting *t) {
    auto segments = Wrap(U"abcdefghij k", 4, WrapMode::kWord);
    TR_ASSERT(t, segments.size() == 3);
    TR_ASSERT(t, IsSegment(segments[0], 0, 4, 0));
    TR_ASSERT(t, IsSegment(segments[1], 4, 8, 4));
    TR_ASSERT(t, IsSegment(segments[2], 8, 12, 8));     // "ij k" - fits exactly
    return kTR_Pass;
}

// Tabs keep line-relative stops; a tab that does not fit moves whole to the next row
DLL_EXPORT int test_wrapprojection_tabs(ITesting *t) {
    // "\tab", tabSize 4, width 4: the tab fills row 0 (0->4), "ab" starts at visual col 4
    auto segments = Wrap(U"\tab", 4, WrapMode::kChar);
    TR_ASSERT(t, segments.size() == 2);
    TR_ASSERT(t, IsSegment(segments[0], 0, 1, 0));
    TR_ASSERT(t, IsSegment(segments[1], 1, 3, 4));

    // "ab\tc", width 3: the tab (2->4) does not fit after "ab", so it starts row 1 at visual col 2
    segments = Wrap(U"ab\tc", 3, WrapMode::kChar);
    TR_ASSERT(t, segments.size() == 2);
    TR_ASSERT(t, IsSegment(segments[0], 0, 2, 0));
    TR_ASSERT(t, IsSegment(segments[1], 2, 4, 2));

    // A tab wider than the whole view still gets a row of its own (progress guarantee)
    segments = Wrap(U"\t\tx", 2, WrapMode::kChar);
    TR_ASSERT(t, segments.size() == 3);
    TR_ASSERT(t, IsSegment(segments[0], 0, 1, 0));
    TR_ASSERT(t, IsSegment(segments[1], 1, 2, 4));
    TR_ASSERT(t, IsSegment(segments[2], 2, 3, 8));
    return kTR_Pass;
}

// Every char on exactly one row, every row non-empty - across modes, widths and tab-heavy input
DLL_EXPORT int test_wrapprojection_tiling(ITesting *t) {
    std::vector<std::u32string> samples = {
        U"x",
        U"hello world, this is a fairly long line of text",
        U"\tint main(int argc, char **argv) {\treturn 0; }",
        U"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        U"a  b  c  d  e  f  g  h  i  j  k  l  m  n  o  p",
        U"\t\t\t\tx",
    };
    for (auto &sample : samples) {
        for (auto mode : {WrapMode::kNone, WrapMode::kChar, WrapMode::kWord}) {
            for (int width = 1; width <= 20; ++width) {
                for (int tabSize : {1, 4, 8}) {
                    auto segments = Wrap(sample, width, mode, tabSize);
                    TR_ASSERT(t, IsTiled(segments, Line(sample).Length()));
                }
            }
        }
    }
    return kTR_Pass;
}

// The visible-row projection: segment 0 marks where a line (and its gutter number) starts, and the
// projection stops at nRows even part-way through a line
DLL_EXPORT int test_wrapprojection_rows(ITesting *t) {
    auto lines = Lines({U"abcdefghij", U"x", U"abcde"});
    auto rows = WrapProjection::ProjectRows(lines, 0, 5, 4, 4, WrapMode::kChar);
    TR_ASSERT(t, rows.size() == 5);
    TR_ASSERT(t, (rows[0].idxLine == 0) && (rows[0].idxSegment == 0) && !rows[0].isLastSegment);
    TR_ASSERT(t, (rows[1].idxLine == 0) && (rows[1].idxSegment == 1) && !rows[1].isLastSegment);
    TR_ASSERT(t, (rows[2].idxLine == 0) && (rows[2].idxSegment == 2) && rows[2].isLastSegment);
    TR_ASSERT(t, (rows[3].idxLine == 1) && (rows[3].idxSegment == 0) && rows[3].isLastSegment);
    TR_ASSERT(t, (rows[4].idxLine == 2) && (rows[4].idxSegment == 0) && !rows[4].isLastSegment);

    // Starting further down, and running out of lines before nRows
    rows = WrapProjection::ProjectRows(lines, 1, 10, 4, 4, WrapMode::kChar);
    TR_ASSERT(t, rows.size() == 3);
    TR_ASSERT(t, rows[0].idxLine == 1);

    // kNone is the classic one-row-per-line view
    rows = WrapProjection::ProjectRows(lines, 0, 10, 4, 4, WrapMode::kNone);
    TR_ASSERT(t, rows.size() == 3);
    return kTR_Pass;
}

DLL_EXPORT int test_wrapprojection_text2screen(ITesting *t) {
    auto lines = Lines({U"abcdefghij", U"x", U"abcde"});
    auto rows = WrapProjection::ProjectRows(lines, 0, 5, 4, 4, WrapMode::kChar);
    Point screen;

    TR_ASSERT(t, WrapProjection::TextToScreen(rows, lines, Point(5, 0), 4, screen));
    TR_ASSERT(t, (screen.x == 1) && (screen.y == 1));

    // A segment boundary belongs to the next row
    TR_ASSERT(t, WrapProjection::TextToScreen(rows, lines, Point(4, 0), 4, screen));
    TR_ASSERT(t, (screen.x == 0) && (screen.y == 1));

    // End-of-line sits after the last char on the line's last row
    TR_ASSERT(t, WrapProjection::TextToScreen(rows, lines, Point(10, 0), 4, screen));
    TR_ASSERT(t, (screen.x == 2) && (screen.y == 2));

    TR_ASSERT(t, WrapProjection::TextToScreen(rows, lines, Point(0, 1), 4, screen));
    TR_ASSERT(t, (screen.x == 0) && (screen.y == 3));

    // Line 2's second segment (chars 4..) is cut off by nRows - not visible
    TR_ASSERT(t, !WrapProjection::TextToScreen(rows, lines, Point(4, 2), 4, screen));
    // Out of range line
    TR_ASSERT(t, !WrapProjection::TextToScreen(rows, lines, Point(0, 3), 4, screen));
    return kTR_Pass;
}

DLL_EXPORT int test_wrapprojection_screen2text(ITesting *t) {
    auto lines = Lines({U"abcdefghij", U"x", U"abcde"});
    auto rows = WrapProjection::ProjectRows(lines, 0, 5, 4, 4, WrapMode::kChar);

    auto text = WrapProjection::ScreenToText(rows, lines, Point(1, 1), 4);
    TR_ASSERT(t, (text.x == 5) && (text.y == 0));

    // Past the end of a wrapped row -> that row's last char, not the next row's first
    text = WrapProjection::ScreenToText(rows, lines, Point(9, 0), 4);
    TR_ASSERT(t, (text.x == 3) && (text.y == 0));

    // Past the end of a line's last row -> end-of-line
    text = WrapProjection::ScreenToText(rows, lines, Point(9, 2), 4);
    TR_ASSERT(t, (text.x == 10) && (text.y == 0));

    // Rows clamp
    text = WrapProjection::ScreenToText(rows, lines, Point(0, 99), 4);
    TR_ASSERT(t, (text.x == 0) && (text.y == 2));

    // No rows at all
    text = WrapProjection::ScreenToText({}, lines, Point(3, 3), 4);
    TR_ASSERT(t, (text.x == 0) && (text.y == 0));
    return kTR_Pass;
}

// Every visible text position maps to the screen and back to itself - including tabs and word mode
DLL_EXPORT int test_wrapprojection_roundtrip(ITesting *t) {
    auto lines = Lines({U"\tint main(int argc, char **argv) {", U"", U"hello\tworld foo bar baz", U"x"});
    for (auto mode : {WrapMode::kNone, WrapMode::kChar, WrapMode::kWord}) {
        auto rows = WrapProjection::ProjectRows(lines, 0, 100, 7, 4, mode);
        for (size_t idxLine = 0; idxLine < lines.size(); ++idxLine) {
            for (size_t idxChar = 0; idxChar <= lines[idxLine]->Length(); ++idxChar) {
                Point textPos(static_cast<int>(idxChar), static_cast<int>(idxLine));
                Point screen;
                TR_ASSERT(t, WrapProjection::TextToScreen(rows, lines, textPos, 4, screen));
                auto back = WrapProjection::ScreenToText(rows, lines, screen, 4);
                TR_ASSERT(t, (back.x == textPos.x) && (back.y == textPos.y));
            }
        }
    }
    return kTR_Pass;
}

// The per-view row list the editor and gutter share: starts at viewTopLine, holds the view's height in
// rows, and follows the view state's wrap mode/width. The default (no wrap) is one row per line.
DLL_EXPORT int test_wrapprojection_viewstate(ITesting *t) {
    auto lines = Lines({U"zero", U"abcdefghij", U"x", U"abcde", U"four"});
    DocumentViewState viewState;
    viewState.lineCursor.viewTopLine = 1;
    viewState.lineCursor.viewBottomLine = 4;    // 3 rows high
    viewState.wrapWidth = 4;

    auto rows = viewState.ProjectVisibleRows(lines, 4);
    TR_ASSERT(t, rows.size() == 3);
    TR_ASSERT(t, (rows[0].idxLine == 1) && (rows[1].idxLine == 2) && (rows[2].idxLine == 3));

    // Wrapped: line 1 takes all three rows, so lines 2+ are pushed out of the view
    viewState.wrapMode = WrapMode::kChar;
    rows = viewState.ProjectVisibleRows(lines, 4);
    TR_ASSERT(t, rows.size() == 3);
    for (size_t i = 0; i < rows.size(); ++i) {
        TR_ASSERT(t, (rows[i].idxLine == 1) && (rows[i].idxSegment == i));
    }
    return kTR_Pass;
}

// Overlay ends: on-row positions map as TextToScreen; above the view -> row -1, below -> row rows.size(),
// so the overlay's "rows strictly between start and end are fully covered" rule paints the visible part
DLL_EXPORT int test_wrapprojection_text2screenclamped(ITesting *t) {
    auto lines = Lines({U"zero", U"abcdefghij", U"x", U"abcde"});
    // View starts at line 1 and is 3 rows high: line 1 fills it ("abcd", "efgh", "ij")
    auto rows = WrapProjection::ProjectRows(lines, 1, 3, 4, 4, WrapMode::kChar);
    TR_ASSERT(t, rows.size() == 3);

    auto pos = WrapProjection::TextToScreenClamped(rows, lines, Point(5, 1), 4);
    TR_ASSERT(t, (pos.x == 1) && (pos.y == 1));

    // Above the view
    pos = WrapProjection::TextToScreenClamped(rows, lines, Point(2, 0), 4);
    TR_ASSERT(t, (pos.x == 0) && (pos.y == -1));

    // Below the view - a later line, and a line past the end of the buffer
    pos = WrapProjection::TextToScreenClamped(rows, lines, Point(0, 2), 4);
    TR_ASSERT(t, (pos.x == 0) && (pos.y == 3));
    pos = WrapProjection::TextToScreenClamped(rows, lines, Point(0, 99), 4);
    TR_ASSERT(t, (pos.x == 0) && (pos.y == 3));

    // A view that starts part-way into nothing: no rows at all is "before"
    pos = WrapProjection::TextToScreenClamped({}, lines, Point(0, 1), 4);
    TR_ASSERT(t, pos.y == -1);
    return kTR_Pass;
}

// Is the caret (idxActiveLine, position.x) on one of the view's rows?
static bool IsCaretVisible(const DocumentViewState &viewState, const std::vector<Line::Ref> &lines, int tabSize) {
    auto rows = viewState.ProjectVisibleRows(lines, tabSize);
    Point caret(viewState.lineCursor.cursor.position.x, static_cast<int>(viewState.lineCursor.idxActiveLine));
    Point screen;
    return WrapProjection::TextToScreen(rows, lines, caret, tabSize, screen);
}

// The caret row below the view pulls the view down - by as little as possible - and the line-based invariants
// (bottom = top + height, position.y = active - top) hold afterwards. Without wrap it is left alone.
DLL_EXPORT int test_wrapprojection_scrollintoview(ITesting *t) {
    // 10-char lines at width 4 -> 3 rows each
    auto lines = Lines({U"0123456789", U"0123456789", U"0123456789", U"0123456789", U"0123456789"});
    DocumentViewState viewState;
    viewState.wrapWidth = 4;
    viewState.lineCursor.viewTopLine = 0;
    viewState.lineCursor.viewBottomLine = 5;    // 5 rows high
    viewState.lineCursor.idxActiveLine = 2;
    viewState.lineCursor.cursor.position = Point(0, 2);

    // No wrap: line-based scroll is already right, nothing moves
    viewState.ScrollActiveRowIntoView(lines, 4);
    TR_ASSERT(t, viewState.lineCursor.viewTopLine == 0);

    // Wrapped: line 2 starts at row 6 - must scroll; top=1 puts it at row 3
    viewState.wrapMode = WrapMode::kChar;
    TR_ASSERT(t, !IsCaretVisible(viewState, lines, 4));
    viewState.ScrollActiveRowIntoView(lines, 4);
    TR_ASSERT(t, IsCaretVisible(viewState, lines, 4));
    TR_ASSERT(t, viewState.lineCursor.viewTopLine == 1);           // minimal: top=0 would not fit
    TR_ASSERT(t, viewState.lineCursor.viewBottomLine == 6);
    TR_ASSERT(t, viewState.lineCursor.cursor.position.y == 1);

    // Caret further along the line (segment 2, row 5 from top=1) - needs one more line off the top
    viewState.lineCursor.cursor.position.x = 9;
    viewState.ScrollActiveRowIntoView(lines, 4);
    TR_ASSERT(t, IsCaretVisible(viewState, lines, 4));
    TR_ASSERT(t, viewState.lineCursor.viewTopLine == 2);

    // Already visible: idempotent
    viewState.ScrollActiveRowIntoView(lines, 4);
    TR_ASSERT(t, viewState.lineCursor.viewTopLine == 2);

    // Active line above the view: the view comes up to it
    viewState.lineCursor.idxActiveLine = 0;
    viewState.lineCursor.cursor.position = Point(0, 0);
    viewState.ScrollActiveRowIntoView(lines, 4);
    TR_ASSERT(t, viewState.lineCursor.viewTopLine == 0);
    TR_ASSERT(t, IsCaretVisible(viewState, lines, 4));
    return kTR_Pass;
}

// A line with more rows than the view: the best we can do is put it at the top (the deferred item) - and the
// scroll must terminate rather than walk past the active line
DLL_EXPORT int test_wrapprojection_scrollintoview_tallline(ITesting *t) {
    auto lines = Lines({U"x", U"0123456789abcdefghij"});      // line 1 is 5 rows at width 4
    DocumentViewState viewState;
    viewState.wrapMode = WrapMode::kChar;
    viewState.wrapWidth = 4;
    viewState.lineCursor.viewTopLine = 0;
    viewState.lineCursor.viewBottomLine = 2;    // 2 rows high
    viewState.lineCursor.idxActiveLine = 1;
    viewState.lineCursor.cursor.position = Point(18, 1);

    viewState.ScrollActiveRowIntoView(lines, 4);
    TR_ASSERT(t, viewState.lineCursor.viewTopLine == 1);
    TR_ASSERT(t, viewState.lineCursor.viewBottomLine == 3);
    return kTR_Pass;
}
