//
// gansi Terminal — grids + encoder + parser orchestration over an injected ITerminalIO.
//
#include "gansi/Terminal.h"

#include "gansi/AnsiEncoder.h"
#include "gansi/Caps.h"

using namespace gnilk::ansi;

namespace {
    // A front-buffer fill that can never equal a real cell -> forces a full repaint on the next diff.
    Cell SentinelCell() {
        Cell c;
        c.ch = 0;   // real content is never NUL (blank is U' ')
        return c;
    }

    void WriteStr(ITerminalIO &io, const std::string &s) {
        if (!s.empty()) {
            io.Write(reinterpret_cast<const uint8_t *>(s.data()), s.size());
        }
    }
}

Terminal::Terminal(std::unique_ptr<ITerminalIO> ioIn) : io(std::move(ioIn)) {
}

Terminal::~Terminal() {
    Close();
}

bool Terminal::Open() {
    if (isOpen) {
        return true;
    }
    if (!io->EnterRawMode()) {
        return false;
    }

    int cols = 0, rows = 0;
    if (!io->GetSize(cols, rows)) {
        cols = 80;
        rows = 24;
    }
    back.Resize(cols, rows);
    front.Resize(cols, rows);
    back.Clear();
    Invalidate();

    // Enter alt-screen, clear, hide cursor during setup, enable input capabilities.
    std::string seq;
    seq += AnsiEncoder::EnterAltScreen();
    seq += AnsiEncoder::ClearScreen();
    seq += AnsiEncoder::HideCursor();
    seq += Caps::EnableMouse();
    seq += Caps::EnablePaste();
    seq += Caps::EnableFocus();
    seq += Caps::EnableKittyKeyboard();
    WriteStr(*io, seq);
    lastCursorVisible = false;

    isOpen = true;
    return true;
}

void Terminal::Close() {
    if (!isOpen) {
        return;
    }
    std::string seq;
    seq += Caps::DisableKittyKeyboard();
    seq += Caps::DisableFocus();
    seq += Caps::DisablePaste();
    seq += Caps::DisableMouse();
    seq += AnsiEncoder::SetCursorShape(CursorShape::Default);
    seq += AnsiEncoder::ShowCursor();
    seq += AnsiEncoder::LeaveAltScreen();
    WriteStr(*io, seq);

    io->RestoreMode();
    isOpen = false;
}

void Terminal::Resize(int cols, int rows) {
    back.Resize(cols, rows);
    front.Resize(cols, rows);
    Invalidate();
}

void Terminal::Invalidate() {
    front.Fill(SentinelCell());
}

void Terminal::Flush() {
    std::string out;
    AnsiEncoder::EncodeDiff(front, back, out);

    // Position / show the hardware cursor.
    if (cursorVisible) {
        if (!lastCursorVisible) {
            out += AnsiEncoder::ShowCursor();
        }
        out += AnsiEncoder::SetCursorShape(cursorShape);
        out += AnsiEncoder::MoveCursor(cursorCol, cursorRow);
    } else if (lastCursorVisible) {
        out += AnsiEncoder::HideCursor();
    }
    lastCursorVisible = cursorVisible;

    WriteStr(*io, out);
    front.CopyFrom(back);
}

void Terminal::SetCursor(int col, int row, bool visible, CursorShape shape) {
    cursorCol = col;
    cursorRow = row;
    cursorVisible = visible;
    cursorShape = shape;
}

std::optional<Event> Terminal::PollEvent(int timeoutMs) {
    // Drain already-parsed events first.
    if (!eventQueue.empty()) {
        Event e = eventQueue.front();
        eventQueue.pop_front();
        return e;
    }

    // A pending resize takes priority over input.
    if (io->TakePendingResize()) {
        int cols = 0, rows = 0;
        if (io->GetSize(cols, rows)) {
            Resize(cols, rows);
            return ResizeEvent{cols, rows};
        }
    }

    uint8_t buf[4096];
    long n = io->Read(buf, sizeof(buf), timeoutMs);
    std::vector<Event> parsed;
    if (n > 0) {
        parser.Feed(buf, static_cast<size_t>(n), parsed);
    } else {
        // Timeout with a lone pending ESC -> resolve it as Escape.
        parser.Flush(parsed);
    }
    for (auto &e : parsed) {
        eventQueue.push_back(e);
    }

    if (!eventQueue.empty()) {
        Event e = eventQueue.front();
        eventQueue.pop_front();
        return e;
    }
    return std::nullopt;
}

void Terminal::SetClipboard(const std::string &utf8) {
    WriteStr(*io, Caps::SetClipboardOSC52(utf8));
}
