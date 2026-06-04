//
// Created by gnilk on 04.06.26.
//

#ifndef GOATEDIT_TERMINALSCREEN_H
#define GOATEDIT_TERMINALSCREEN_H

#include <vector>
#include <memory>
#include <cstdint>

#include "Core/ColorRGBA.h"
#include "Core/Point.h"

namespace gedit {
    class TerminalScreen {
    public:
        using Ref = std::shared_ptr<TerminalScreen>;

        static constexpr uint8_t kAttrBold      = 0x01;
        static constexpr uint8_t kAttrItalic    = 0x02;
        static constexpr uint8_t kAttrUnderline = 0x04;
        static constexpr uint8_t kAttrInvert    = 0x08;

        struct Cell {
            char32_t  ch    = U' ';
            ColorRGBA fg    = {};
            ColorRGBA bg    = {};
            uint8_t   attrs = 0;
        };

        using Row = std::vector<Cell>;

    public:
        void Resize(int newCols, int newRows);
        void SetDefaultColors(ColorRGBA fg, ColorRGBA bg);

        // Step 1 — stream input (replaces historyBuffer + lastLine)
        void PutChar(char32_t ch);
        void NewLine();
        void CarriageReturn();

        // Pen state
        void SetForeground(ColorRGBA color);
        void SetBackground(ColorRGBA color);
        void ResetAttributes();

        // Rendering accessors
        const Row &GetRow(int y) const;
        const std::vector<Row> &GetScrollback() const;
        Point GetCursorPos() const;
        int Cols() const;
        int Rows() const;

        // Step 2 — cursor movement and erase (full-screen apps)
        void SetCursorPos(int x, int y);
        void MoveCursor(int dx, int dy);
        void EraseToEndOfLine();
        void EraseScreen();
        void SetAttributes(uint8_t attrs);

        // Step 3 — alternate screen buffer
        void SaveScreen();
        void RestoreScreen();

    private:
        Cell MakeBlankCell() const;
        Row  MakeBlankRow() const;
        void ScrollUp();

    private:
        int cols = 0;
        int rows = 0;

        std::vector<Row> grid;
        std::vector<Row> scrollback;

        Point     cursor     = {};
        ColorRGBA penFg      = {};
        ColorRGBA penBg      = {};
        uint8_t   penAttrs   = 0;

        ColorRGBA defaultFg  = {};
        ColorRGBA defaultBg  = {};

        // Saved state for alternate screen (Step 3)
        std::vector<Row> savedGrid;
        Point     savedCursor   = {};
        ColorRGBA savedPenFg    = {};
        ColorRGBA savedPenBg    = {};
        uint8_t   savedPenAttrs = 0;
    };
}

#endif //GOATEDIT_TERMINALSCREEN_H
