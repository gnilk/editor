//
// OverviewMap (docs/usability-round.md §7) - the whole file mapped onto the overview's rows, on plain numbers
//
#include <testinterface.h>

#include <string>
#include <vector>

#include "Core/LineDiff.h"
#include "Core/OverviewMap.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_overviewmap(ITesting *t);
    DLL_EXPORT int test_overviewmap_short(ITesting *t);
    DLL_EXPORT int test_overviewmap_long(ITesting *t);
    DLL_EXPORT int test_overviewmap_roundtrip(ITesting *t);
    DLL_EXPORT int test_overviewmap_empty(ITesting *t);
    DLL_EXPORT int test_overviewmap_box(ITesting *t);
    DLL_EXPORT int test_overviewmap_click(ITesting *t);
    DLL_EXPORT int test_overviewmap_drag(ITesting *t);
    DLL_EXPORT int test_overviewmap_marks(ITesting *t);
    DLL_EXPORT int test_overviewmap_marksshared(ITesting *t);
    DLL_EXPORT int test_overviewmap_thicken(ITesting *t);
    DLL_EXPORT int test_overviewmap_nevergone(ITesting *t);
}

namespace {
    // The marks as a string, a character per row: '.' none, 'A' added, 'M' modified, 'R' removed, 'S' search, '*' active
    std::string Marks(const std::vector<uint8_t> &marks) {
        static const char kChars[] = {'.', 'A', 'M', 'R', 'S', '*'};
        std::string out;
        for (auto mark : marks) {
            out += kChars[mark];
        }
        return out;
    }

    // The (lines, rows) pairs the properties are checked over: short, exact, several lines a row, not evenly divisible
    struct Shape {
        size_t numLines;
        int numRows;
    };
    const std::vector<Shape> kShapes = {{1, 10}, {7, 100}, {50, 100}, {100, 100}, {150, 100}, {1500, 900},
                                        {1501, 899}, {100000, 37}, {3, 1}};
}

DLL_EXPORT int test_overviewmap(ITesting *t) {
    return kTR_Pass;
}

// A short file doesn't stretch: two rows a line, from the top, the rest of the rows unused
DLL_EXPORT int test_overviewmap_short(ITesting *t) {
    OverviewMap map(10, 100);
    TR_ASSERT(t, map.RowsPerLine() == 2.0);
    TR_ASSERT(t, map.UsedRows() == 20);
    TR_ASSERT(t, map.RowOfLine(0) == 0);
    TR_ASSERT(t, map.RowOfLine(5) == 10);
    TR_ASSERT(t, map.LineOfRow(10) == 5);
    TR_ASSERT(t, map.LineOfRow(11) == 5);
    // Below the file: the last line
    TR_ASSERT(t, map.LineOfRow(50) == 9);
    return kTR_Pass;
}

// A long file fits the height: every row used, several lines a row
DLL_EXPORT int test_overviewmap_long(ITesting *t) {
    OverviewMap map(1500, 900);
    TR_ASSERT(t, map.RowsPerLine() < 1.0);
    TR_ASSERT(t, map.UsedRows() == 900);
    TR_ASSERT(t, map.RowOfLine(1499) == 899);
    TR_ASSERT(t, map.LineOfRow(899) <= 1499);
    TR_ASSERT(t, map.LineOfRow(-5) == 0);
    return kTR_Pass;
}

// Line -> row -> line agree on every boundary, whatever the shape
DLL_EXPORT int test_overviewmap_roundtrip(ITesting *t) {
    for (auto &shape : kShapes) {
        OverviewMap map(shape.numLines, shape.numRows);
        TR_ASSERT(t, map.UsedRows() <= shape.numRows);
        // The row's line is the first of the lines drawn from its row, and those lines reach the row
        for (int row = 0; row < map.UsedRows(); row++) {
            auto idxLine = map.LineOfRow(row);
            auto rowStart = map.RowOfLine(idxLine);
            TR_ASSERT(t, rowStart <= row);
            TR_ASSERT(t, (idxLine == 0) || (map.RowOfLine(idxLine - 1) < rowStart));
            auto idxNext = idxLine;
            while ((idxNext < shape.numLines) && (map.RowOfLine(idxNext) == rowStart)) {
                idxNext++;
            }
            TR_ASSERT(t, map.RowOfLine(idxNext) > row);
        }
        // A line's row leads back to that line, or to an earlier one sharing the row
        for (size_t idxLine = 0; idxLine < shape.numLines; idxLine += 1 + shape.numLines / 1000) {
            auto row = map.RowOfLine(idxLine);
            TR_ASSERT(t, map.LineOfRow(row) <= idxLine);
            TR_ASSERT(t, map.RowOfLine(map.LineOfRow(row)) == row);
        }
        // Rows only go down the file
        for (size_t idxLine = 1; idxLine < shape.numLines; idxLine += 1 + shape.numLines / 1000) {
            TR_ASSERT(t, map.RowOfLine(idxLine) >= map.RowOfLine(idxLine - 1));
        }
    }
    return kTR_Pass;
}

// No lines, or no rows: nothing to map, and nothing breaks
DLL_EXPORT int test_overviewmap_empty(ITesting *t) {
    OverviewMap noLines(0, 100);
    TR_ASSERT(t, noLines.UsedRows() == 0);
    TR_ASSERT(t, noLines.LineOfRow(10) == 0);
    TR_ASSERT(t, noLines.BoxOf(0, 40).Height() == 0);
    TR_ASSERT(t, noLines.TopLineFor(10, 0, 40) == 0);
    TR_ASSERT(t, Marks(noLines.MarksPerRow({}, {}, 0)) == std::string(100, '.'));

    OverviewMap noRows(100, 0);
    TR_ASSERT(t, noRows.UsedRows() == 0);
    TR_ASSERT(t, noRows.BoxOf(0, 40).Height() == 0);
    TR_ASSERT(t, noRows.MarksPerRow({LineDiff::kAdded}, {}, 0).empty());
    return kTR_Pass;
}

// The box is always at least a row, always inside the overview, and covers the view's lines
DLL_EXPORT int test_overviewmap_box(ITesting *t) {
    for (auto &shape : kShapes) {
        OverviewMap map(shape.numLines, shape.numRows);
        for (size_t numVisible : {(size_t)1, (size_t)40, shape.numLines, shape.numLines * 2}) {
            for (size_t idxTop : {(size_t)0, shape.numLines / 2, shape.numLines - 1}) {
                auto box = map.BoxOf(idxTop, numVisible);
                TR_ASSERT(t, box.Height() >= 1);
                TR_ASSERT(t, box.top >= 0);
                TR_ASSERT(t, box.bottom <= shape.numRows);
                TR_ASSERT(t, box.Contains(map.RowOfLine(idxTop)));
            }
        }
    }
    // A huge file: the box is a single row, and still there
    OverviewMap huge(100000, 37);
    TR_ASSERT(t, huge.BoxOf(50000, 40).Height() == 1);
    // The whole of a short file on screen: the box is the file
    OverviewMap small(10, 100);
    auto box = small.BoxOf(0, 40);
    TR_ASSERT(t, (box.top == 0) && (box.bottom == small.UsedRows()));
    return kTR_Pass;
}

// A click outside the box centres the view on the line under the pointer: the new box holds the pointer's row
DLL_EXPORT int test_overviewmap_click(ITesting *t) {
    for (auto &shape : kShapes) {
        OverviewMap map(shape.numLines, shape.numRows);
        size_t numVisible = 40;
        auto box = map.BoxOf(0, numVisible);
        for (int pointer = 0; pointer < map.UsedRows(); pointer += 1 + map.UsedRows() / 50) {
            if (box.Contains(pointer)) {
                continue;
            }
            auto grab = OverviewMap::GrabOffset(box, pointer);
            auto idxTop = map.TopLineFor(pointer, grab, numVisible);
            auto maxTop = (shape.numLines > numVisible) ? shape.numLines - numVisible : 0;
            TR_ASSERT(t, idxTop <= maxTop);
            // Unless the view had to stop at an end of the file, it now shows the pointer's row
            if ((idxTop > 0) && (idxTop < maxTop)) {
                TR_ASSERT(t, map.BoxOf(idxTop, numVisible).Contains(pointer));
            }
        }
    }
    return kTR_Pass;
}

// A drag keeps the box where it was taken: the box follows the pointer row for row, and stops at the ends of the file
DLL_EXPORT int test_overviewmap_drag(ITesting *t) {
    // A row a line, so a row moved is a line scrolled
    OverviewMap map(200, 200);
    size_t numVisible = 40;
    auto box = map.BoxOf(50, numVisible);
    TR_ASSERT(t, box.top == 50);
    auto press = box.top + 7;
    auto grab = OverviewMap::GrabOffset(box, press);
    TR_ASSERT(t, grab == 7);
    // Pressing without moving doesn't move the view
    TR_ASSERT(t, map.TopLineFor(press, grab, numVisible) == 50);
    TR_ASSERT(t, map.TopLineFor(press + 10, grab, numVisible) == 60);
    TR_ASSERT(t, map.TopLineFor(press - 10, grab, numVisible) == 40);
    // Past the ends: the view stops at the first line, and at the last screenful
    TR_ASSERT(t, map.TopLineFor(-500, grab, numVisible) == 0);
    TR_ASSERT(t, map.TopLineFor(5000, grab, numVisible) == 200 - numVisible);

    // Several lines a row: a drag still only goes one way, and never past the ends
    OverviewMap dense(1500, 900);
    auto denseBox = dense.BoxOf(300, numVisible);
    auto denseGrab = OverviewMap::GrabOffset(denseBox, denseBox.top);
    size_t idxPrev = 0;
    for (int pointer = -10; pointer < 950; pointer++) {
        auto idxTop = dense.TopLineFor(pointer, denseGrab, numVisible);
        TR_ASSERT(t, idxTop >= idxPrev);
        TR_ASSERT(t, idxTop <= 1500 - numVisible);
        idxPrev = idxTop;
    }
    return kTR_Pass;
}

// One line a row (well, two rows a line here): each kind of mark where its line is, removals on the edges
DLL_EXPORT int test_overviewmap_marks(ITesting *t) {
    OverviewMap map(5, 10);
    std::vector<uint8_t> changes = {LineDiff::kNone, LineDiff::kAdded, LineDiff::kModified, LineDiff::kRemovedAbove,
                                    LineDiff::kRemovedBelow};
    TR_ASSERT(t, Marks(map.MarksPerRow(changes, {}, OverviewMap::kNoLine)) == "..AAMMR..R");
    TR_ASSERT(t, Marks(map.MarksPerRow({}, {0}, 4)) == "SS......**");
    // The active line wins over its own change, a search hit over a change
    TR_ASSERT(t, Marks(map.MarksPerRow(changes, {2}, 1)) == "..**SSR..R");
    // A change list longer than the file (the marks of a moment ago) is cut at the file's end
    TR_ASSERT(t, Marks(map.MarksPerRow(std::vector<uint8_t>(20, LineDiff::kAdded), {}, OverviewMap::kNoLine)) == "AAAAAAAAAA");
    return kTR_Pass;
}

// Several lines a row: the row shows the heaviest of them - removed > modified > added
DLL_EXPORT int test_overviewmap_marksshared(ITesting *t) {
    // Four lines a row
    OverviewMap map(12, 3);
    std::vector<uint8_t> changes(12, LineDiff::kNone);
    changes[0] = LineDiff::kAdded;
    changes[1] = LineDiff::kModified;
    changes[5] = LineDiff::kAdded;
    changes[6] = LineDiff::kRemovedAbove;
    changes[7] = LineDiff::kModified;
    changes[11] = LineDiff::kAdded;
    TR_ASSERT(t, Marks(map.MarksPerRow(changes, {}, OverviewMap::kNoLine)) == "MRA");
    TR_ASSERT(t, Marks(map.MarksPerRow(changes, {10}, OverviewMap::kNoLine)) == "MRS");
    return kTR_Pass;
}

// A mark a pixel high grows to the least height, centred, inside the image; where two meet the heavier wins
DLL_EXPORT int test_overviewmap_thicken(ITesting *t) {
    auto marks = [](const std::string &text) {
        std::vector<uint8_t> out;
        for (auto ch : text) {
            out.push_back((ch == 'A') ? OverviewMap::kAdded : (ch == 'R') ? OverviewMap::kRemoved : OverviewMap::kNone);
        }
        return out;
    };
    TR_ASSERT(t, Marks(OverviewMap::Thicken(marks("....A....."), 4)) == "...AAAA...");
    TR_ASSERT(t, Marks(OverviewMap::Thicken(marks("A........."), 4)) == "AAAA......");
    TR_ASSERT(t, Marks(OverviewMap::Thicken(marks(".........A"), 4)) == "......AAAA");
    TR_ASSERT(t, Marks(OverviewMap::Thicken(marks("...AR....."), 4)) == "..ARRRR...");
    TR_ASSERT(t, Marks(OverviewMap::Thicken(marks("..A..."), 1)) == "..A...");
    TR_ASSERT(t, Marks(OverviewMap::Thicken(marks("A."), 4)) == "AA");
    return kTR_Pass;
}

// However many lines share a row, a changed line is never scaled away: its row carries a mark at least as heavy as the
// change - for every line of every shape, the first and the last line included
DLL_EXPORT int test_overviewmap_nevergone(ITesting *t) {
    for (auto &shape : kShapes) {
        OverviewMap map(shape.numLines, shape.numRows);
        auto step = 1 + shape.numLines / 500;
        for (size_t idxLine = 0; idxLine < shape.numLines; idxLine += step) {
            for (uint8_t change : {(uint8_t)LineDiff::kAdded, (uint8_t)LineDiff::kModified, (uint8_t)LineDiff::kRemovedAbove,
                                   (uint8_t)LineDiff::kRemovedBelow}) {
                std::vector<uint8_t> changes(shape.numLines, LineDiff::kNone);
                changes[idxLine] = change;
                auto marks = map.MarksPerRow(changes, {}, OverviewMap::kNoLine);
                TR_ASSERT(t, marks.size() == (size_t)shape.numRows);
                int numMarked = 0;
                for (auto mark : marks) {
                    numMarked += (mark != OverviewMap::kNone) ? 1 : 0;
                }
                TR_ASSERT(t, numMarked >= 1);
                TR_ASSERT(t, marks[(size_t)map.RowOfLine(idxLine)] != OverviewMap::kNone ||
                             change == LineDiff::kRemovedBelow);
                // Thickened, it is still there, and at least the least height (when the overview has the rows)
                auto thick = OverviewMap::Thicken(marks, 4);
                int numThick = 0;
                for (auto mark : thick) {
                    numThick += (mark != OverviewMap::kNone) ? 1 : 0;
                }
                TR_ASSERT(t, numThick >= std::min(4, shape.numRows));
            }
        }
    }
    return kTR_Pass;
}
