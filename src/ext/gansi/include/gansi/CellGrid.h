//
// gansi cell grid — a cols×rows buffer of Cells. Pure logic, no platform deps.
//
// Invariant (the load-bearing one): Cols()/Rows() ALWAYS equal the real buffer extent. Resize
// reallocates and updates both together — a stale dimension reading against a resized buffer is the
// class of bug the editor's TerminalScreen notes warn about.
//
#ifndef GANSI_CELLGRID_H
#define GANSI_CELLGRID_H

#include <vector>
#include "gansi/Types.h"

namespace gnilk::ansi {

    class CellGrid {
    public:
        CellGrid() = default;
        CellGrid(int cols, int rows);

        // Reallocate to cols×rows. Content in the overlapping top-left region is preserved; newly
        // exposed cells are blank. Negative inputs clamp to 0 (→ an empty 0×0 grid).
        void Resize(int cols, int rows);

        int Cols() const { return cols; }
        int Rows() const { return rows; }
        bool IsEmpty() const { return cols == 0 || rows == 0; }

        // Cell access — bounds-CLAMPED: out-of-range indices clamp to the nearest valid cell, and any
        // access on an empty grid lands on a private scratch cell (never UB, never a crash).
        Cell &At(int col, int row);
        const Cell &At(int col, int row) const;

        // Fill every cell with `cell`.
        void Fill(const Cell &cell);
        // Blank every cell: space glyph, default fg, given bg, no attrs.
        void Clear(Color bg = kDefaultBg);

        // Copy the contents of `other` (including its dimensions) into this grid.
        void CopyFrom(const CellGrid &other);

        bool SameSizeAs(const CellGrid &other) const {
            return cols == other.cols && rows == other.rows;
        }

    private:
        int cols = 0;
        int rows = 0;
        std::vector<Cell> cells;          // row-major, size == cols*rows
        mutable Cell scratch;             // target for clamped access on an empty grid
    };

}

#endif // GANSI_CELLGRID_H
