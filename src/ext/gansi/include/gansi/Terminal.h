//
// gansi Terminal — the "screen": owns the double-buffered cell grid, the input parser and the
// output encoder, and drives an injected ITerminalIO. This is the surface the editor backend
// consumes (mirroring how the SDL backend consumes SDL).
//
#ifndef GANSI_TERMINAL_H
#define GANSI_TERMINAL_H

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <vector>

#include "gansi/CellGrid.h"
#include "gansi/Event.h"
#include "gansi/ITerminalIO.h"
#include "gansi/InputParser.h"
#include "gansi/Types.h"

namespace gnilk::ansi {

    class Terminal {
    public:
        explicit Terminal(std::unique_ptr<ITerminalIO> io);
        ~Terminal();

        // Enter raw mode + alt-screen, enable capabilities, size the grids to the current terminal.
        // Returns false if raw mode could not be entered.
        bool Open();
        // Disable capabilities, leave alt-screen, restore the terminal. Idempotent / RAII-safe.
        void Close();

        int Cols() const { return back.Cols(); }
        int Rows() const { return back.Rows(); }

        // Resize the grids. Forces a full repaint on the next Flush (the displayed content is unknown
        // after a resize).
        void Resize(int cols, int rows);

        // Back-buffer cell write (bounds-clamped via CellGrid).
        Cell &At(int col, int row) { return back.At(col, row); }
        void Clear(Color bg = kDefaultBg) { back.Clear(bg); }

        // Diff back vs front -> minimal ANSI -> Write; position the hardware cursor; front <- back.
        void Flush();

        // Hardware cursor state, applied on the next Flush.
        void SetCursor(int col, int row, bool visible, CursorShape shape = CursorShape::Default);

        // Read + parse one event (waiting up to timeoutMs). Returns nullopt on timeout. A pending
        // resize surfaces as a ResizeEvent before any input is read.
        std::optional<Event> PollEvent(int timeoutMs);

        // Outbound clipboard via OSC 52.
        void SetClipboard(const std::string &utf8);

        bool IsOpen() const { return isOpen; }

    private:
        void Invalidate();      // force a full repaint (front != back everywhere)

    private:
        std::unique_ptr<ITerminalIO> io;
        CellGrid front;
        CellGrid back;
        InputParser parser;
        std::deque<Event> eventQueue;

        int cursorCol = 0;
        int cursorRow = 0;
        bool cursorVisible = true;
        CursorShape cursorShape = CursorShape::Default;
        bool lastCursorVisible = false;     // what the terminal was last told (drives show/hide emit)

        bool isOpen = false;
    };

}

#endif // GANSI_TERMINAL_H
