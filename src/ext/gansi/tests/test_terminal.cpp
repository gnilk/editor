//
// gansi Terminal tests — driven through MockTerminalIO: open/close capability sequences, flush diff,
// event polling (key + resize + timeout), full-repaint-on-resize, OSC 52 clipboard.
//
#include <testinterface.h>

#include <memory>
#include <string>

#include "gansi/Terminal.h"
#include "gansi/MockTerminalIO.h"

using namespace gnilk::ansi;

extern "C" {
DLL_EXPORT int test_terminal(ITesting *t);
DLL_EXPORT int test_terminal_open(ITesting *t);
DLL_EXPORT int test_terminal_close(ITesting *t);
DLL_EXPORT int test_terminal_flush_minimal(ITesting *t);
DLL_EXPORT int test_terminal_poll_key(ITesting *t);
DLL_EXPORT int test_terminal_poll_resize(ITesting *t);
DLL_EXPORT int test_terminal_poll_timeout(ITesting *t);
DLL_EXPORT int test_terminal_resize_repaint(ITesting *t);
DLL_EXPORT int test_terminal_clipboard(ITesting *t);
DLL_EXPORT int test_terminal_snapshot(ITesting *t);
DLL_EXPORT int test_terminal_cursor_shape_coalesce(ITesting *t);
DLL_EXPORT int test_terminal_open_caps_gated(ITesting *t);
DLL_EXPORT int test_terminal_clipboard_gated(ITesting *t);
}

static bool Contains(const std::string &hay, const std::string &needle) {
    return hay.find(needle) != std::string::npos;
}

DLL_EXPORT int test_terminal(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_open(ITesting *t) {
    auto mock = std::make_unique<MockTerminalIO>();
    MockTerminalIO *raw = mock.get();
    raw->SetSize(100, 30);
    Terminal term(std::move(mock));

    TR_ASSERT(t, term.Open());
    TR_ASSERT(t, term.IsOpen());
    TR_ASSERT(t, raw->IsRawMode());
    // Grids sized to the terminal.
    TR_ASSERT(t, term.Cols() == 100);
    TR_ASSERT(t, term.Rows() == 30);
    // Enable sequences were written.
    const std::string &w = raw->Written();
    TR_ASSERT(t, Contains(w, "\x1b[?1049h"));   // alt screen
    TR_ASSERT(t, Contains(w, "\x1b[?1006h"));   // SGR mouse
    TR_ASSERT(t, Contains(w, "\x1b[>1u"));       // kitty keyboard
    TR_ASSERT(t, Contains(w, "\x1b[?2004h"));   // bracketed paste
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_close(ITesting *t) {
    auto mock = std::make_unique<MockTerminalIO>();
    MockTerminalIO *raw = mock.get();
    Terminal term(std::move(mock));
    term.Open();
    raw->ClearWritten();

    term.Close();
    TR_ASSERT(t, !term.IsOpen());
    TR_ASSERT(t, raw->RestoreCalls() == 1);
    const std::string &w = raw->Written();
    TR_ASSERT(t, Contains(w, "\x1b[?1049l"));   // leave alt screen
    TR_ASSERT(t, Contains(w, "\x1b[?1006l"));   // disable mouse
    TR_ASSERT(t, Contains(w, "\x1b[<u"));        // pop kitty keyboard

    // Idempotent: a second Close is a no-op (no extra RestoreMode).
    term.Close();
    TR_ASSERT(t, raw->RestoreCalls() == 1);
    return kTR_Pass;
}

static Cell RedX() {
    Cell c;
    c.ch = U'X';
    c.fg = Color{255, 0, 0};
    c.bg = Color{0, 0, 0};
    return c;
}

DLL_EXPORT int test_terminal_flush_minimal(ITesting *t) {
    auto mock = std::make_unique<MockTerminalIO>();
    MockTerminalIO *raw = mock.get();
    raw->SetSize(10, 3);
    Terminal term(std::move(mock));
    term.Open();
    term.Clear({0, 0, 0});
    term.At(2, 1) = RedX();
    term.Flush();
    TR_ASSERT(t, Contains(raw->Written(), "X"));

    // No change -> the next Flush emits no content (no SGR color run).
    raw->ClearWritten();
    term.Flush();
    TR_ASSERT(t, raw->Written().find("38;2") == std::string::npos);
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_poll_key(ITesting *t) {
    auto mock = std::make_unique<MockTerminalIO>();
    MockTerminalIO *raw = mock.get();
    Terminal term(std::move(mock));
    term.Open();
    raw->QueueInput("a");

    auto ev = term.PollEvent(0);
    TR_ASSERT(t, ev.has_value());
    TR_ASSERT(t, std::holds_alternative<KeyEvent>(*ev));
    TR_ASSERT(t, std::get<KeyEvent>(*ev).ch == U'a');
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_poll_resize(ITesting *t) {
    auto mock = std::make_unique<MockTerminalIO>();
    MockTerminalIO *raw = mock.get();
    raw->SetSize(80, 24);
    Terminal term(std::move(mock));
    term.Open();

    raw->SetSize(120, 40);
    raw->QueueResize();
    auto ev = term.PollEvent(0);
    TR_ASSERT(t, ev.has_value());
    TR_ASSERT(t, std::holds_alternative<ResizeEvent>(*ev));
    TR_ASSERT(t, std::get<ResizeEvent>(*ev).cols == 120);
    TR_ASSERT(t, std::get<ResizeEvent>(*ev).rows == 40);
    TR_ASSERT(t, term.Cols() == 120 && term.Rows() == 40);
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_poll_timeout(ITesting *t) {
    auto mock = std::make_unique<MockTerminalIO>();
    Terminal term(std::move(mock));
    term.Open();
    auto ev = term.PollEvent(0);
    TR_ASSERT(t, !ev.has_value());
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_resize_repaint(ITesting *t) {
    auto mock = std::make_unique<MockTerminalIO>();
    MockTerminalIO *raw = mock.get();
    raw->SetSize(10, 3);
    Terminal term(std::move(mock));
    term.Open();
    term.Clear({0, 0, 0});
    term.At(1, 1) = RedX();
    term.Flush();
    TR_ASSERT(t, Contains(raw->Written(), "X"));

    // Same-size resize still invalidates the front buffer -> next Flush repaints 'X'.
    raw->ClearWritten();
    term.Resize(10, 3);
    term.Flush();
    TR_ASSERT(t, Contains(raw->Written(), "X"));
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_clipboard(ITesting *t) {
    auto mock = std::make_unique<MockTerminalIO>();
    MockTerminalIO *raw = mock.get();
    Terminal term(std::move(mock));
    term.Open();
    raw->ClearWritten();
    term.SetClipboard("hi");
    TR_ASSERT(t, raw->Written() == std::string("\x1b]52;c;aGk=\x1b\\"));
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_snapshot(ITesting *t) {
    auto mock = std::make_unique<MockTerminalIO>();
    mock->SetSize(5, 2);
    Terminal term(std::move(mock));
    term.Open();
    term.Clear({0, 0, 0});
    term.At(0, 0) = RedX();
    term.SaveSnapshot();

    // Scribble over the back buffer (as a modal would), then restore.
    term.At(0, 0).ch = U'M';
    term.At(3, 1).ch = U'M';
    TR_ASSERT(t, term.At(0, 0).ch == U'M');
    term.RestoreSnapshot();
    TR_ASSERT(t, term.At(0, 0).ch == U'X');
    TR_ASSERT(t, term.At(3, 1).ch == U' ');
    return kTR_Pass;
}

// A reduced-capability profile (Apple_Terminal: bracketed paste only) must NOT emit Kitty keyboard,
// focus or mouse enables on Open — those are exactly the sequences a terminal mis-handles as a stray
// glyph. Alt-screen and bracketed paste still go out.
DLL_EXPORT int test_terminal_open_caps_gated(ITesting *t) {
    Capabilities caps;
    caps.kittyKeyboard = false;
    caps.focusReporting = false;
    caps.sgrMouse = false;
    caps.osc52Clipboard = false;
    caps.truecolor = false;
    // bracketedPaste stays true.

    auto mock = std::make_unique<MockTerminalIO>();
    MockTerminalIO *raw = mock.get();
    raw->SetSize(80, 24);
    Terminal term(std::move(mock), caps);
    TR_ASSERT(t, term.Open());

    const std::string &w = raw->Written();
    TR_ASSERT(t, Contains(w, "\x1b[?1049h"));    // alt screen — always
    TR_ASSERT(t, Contains(w, "\x1b[?2004h"));    // bracketed paste — supported
    TR_ASSERT(t, !Contains(w, "\x1b[>1u"));       // NO kitty keyboard
    TR_ASSERT(t, !Contains(w, "\x1b[?1004h"));   // NO focus reporting
    TR_ASSERT(t, !Contains(w, "\x1b[?1006h"));   // NO SGR mouse
    TR_ASSERT(t, !Contains(w, "\x1b[?1000h"));   // NO mouse tracking at all

    // Close mirrors it: only the disables for what was enabled.
    raw->ClearWritten();
    term.Close();
    const std::string &c = raw->Written();
    TR_ASSERT(t, Contains(c, "\x1b[?1049l"));    // leave alt screen
    TR_ASSERT(t, Contains(c, "\x1b[?2004l"));    // disable bracketed paste
    TR_ASSERT(t, !Contains(c, "\x1b[<u"));        // NO kitty pop
    TR_ASSERT(t, !Contains(c, "\x1b[?1006l"));   // NO mouse disable
    return kTR_Pass;
}

// With OSC 52 unsupported, SetClipboard is a silent no-op (nothing reaches the wire).
DLL_EXPORT int test_terminal_clipboard_gated(ITesting *t) {
    Capabilities caps;
    caps.osc52Clipboard = false;

    auto mock = std::make_unique<MockTerminalIO>();
    MockTerminalIO *raw = mock.get();
    Terminal term(std::move(mock), caps);
    term.Open();
    raw->ClearWritten();
    term.SetClipboard("hi");
    TR_ASSERT(t, raw->Written().empty());
    return kTR_Pass;
}

DLL_EXPORT int test_terminal_cursor_shape_coalesce(ITesting *t) {
    auto mock = std::make_unique<MockTerminalIO>();
    MockTerminalIO *raw = mock.get();
    mock->SetSize(10, 3);
    Terminal term(std::move(mock));
    term.Open();

    // First visible flush sets the shape (DECSCUSR bar-blink == CSI 5 SP q).
    term.SetCursor(0, 0, true, CursorShape::BarBlink);
    raw->ClearWritten();
    term.Flush();
    TR_ASSERT(t, Contains(raw->Written(), "\x1b[5 q"));

    // Same shape, cursor moved -> shape NOT re-sent, but the cursor still moves.
    term.SetCursor(1, 0, true, CursorShape::BarBlink);
    raw->ClearWritten();
    term.Flush();
    TR_ASSERT(t, !Contains(raw->Written(), " q"));
    TR_ASSERT(t, Contains(raw->Written(), "\x1b[1;2H"));

    // Changed shape -> re-sent.
    term.SetCursor(1, 0, true, CursorShape::BlockSteady);
    raw->ClearWritten();
    term.Flush();
    TR_ASSERT(t, Contains(raw->Written(), "\x1b[2 q"));
    return kTR_Pass;
}
