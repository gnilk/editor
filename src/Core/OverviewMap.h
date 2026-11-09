//
// OverviewMap - the whole file mapped onto the height of the overview at the editor's right edge (docs/usability-round.md
// §7): the ruler's slots or the minimap's pixel rows, "rows" here either way. Plain: line counts in, rows out; knows
// nothing of documents, views or drawing.
//
// The whole file fits the height (VS Code's 'fit'): a line is numRows / numLines rows, at most 'maxRowsPerLine' - a
// short file doesn't stretch, it stays at the top. A long one puts several lines in a row; the row shows the heaviest
// mark of its lines. Soft-wrapped lines count as one: the overview is of the file, not the screen.
//
#ifndef GEDIT_OVERVIEWMAP_H
#define GEDIT_OVERVIEWMAP_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gedit {

    class OverviewMap {
    public:
        // What a row shows, by weight - a row with several lines shows the heaviest
        enum kMark : uint8_t {
            kNone = 0,
            kAdded,
            kModified,
            kRemoved,
            kSearch,
            kActiveLine,
        };

        // The rows the view shows, [top, bottom)
        struct Box {
            int top = 0;
            int bottom = 0;

            int Height() const {
                return bottom - top;
            }
            bool Contains(int row) const {
                return (row >= top) && (row < bottom);
            }
        };

        static constexpr double kDefaultMaxRowsPerLine = 2.0;
        static constexpr size_t kNoLine = SIZE_MAX;

    public:
        OverviewMap() = default;
        OverviewMap(size_t numLines, int numRows, double maxRowsPerLine = kDefaultMaxRowsPerLine);
        virtual ~OverviewMap() = default;

        size_t NumLines() const {
            return numLines;
        }
        int NumRows() const {
            return numRows;
        }
        double RowsPerLine() const {
            return rowsPerLine;
        }
        // The rows the file takes - all of them for a long file, fewer for a short one
        int UsedRows() const {
            return RowOfLine(numLines);
        }

        // The first row of a line; a line past the end is the row after the file
        int RowOfLine(size_t idxLine) const;
        // The line under a row: the first line drawn in it. A row above the file is line 0, below it the last line
        size_t LineOfRow(int row) const;

        // The rows of a view showing 'numVisibleLines' lines from 'idxTopLine' - at least one row, so a huge file still
        // shows where the view is
        Box BoxOf(size_t idxTopLine, size_t numVisibleLines) const;

        // A press on the overview: where in the box it took hold. Inside the box the box keeps that hold; outside it
        // the box is taken by its middle, so a click centres the view on the line under the pointer.
        static int GrabOffset(const Box &box, int pointerRow);
        // The view's top line with the box held at 'grabOffset' under the pointer - a click and every move of a drag
        size_t TopLineFor(int pointerRow, int grabOffset, size_t numVisibleLines) const;

        // A mark per row (NumRows() of them): the change marks (LineDiff::kMark per line), the lines with a search hit
        // and the active line (kNoLine: none) - the heaviest of a row's lines wins
        std::vector<uint8_t> MarksPerRow(const std::vector<uint8_t> &lineChanges, const std::vector<size_t> &searchLines,
                                         size_t idxActiveLine) const;

    private:
        size_t FirstLineFromRow(int row) const;
        void MarkLine(std::vector<uint8_t> &marks, size_t idxLine, kMark mark) const;
        static void MarkRow(std::vector<uint8_t> &marks, int row, kMark mark);

    private:
        size_t numLines = 0;
        int numRows = 0;
        double rowsPerLine = 0;
    };

}

#endif //GEDIT_OVERVIEWMAP_H
