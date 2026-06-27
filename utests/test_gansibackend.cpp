//
// gedit::Gansi backend tests — drive the backend headlessly with an injected MockTerminalIO. Verify
// the ScreenBase contract (dimensions, window-offset draw + clipping into the shared grid, color +
// box-rule + overlay-invert), the keyboard translation, and the modal snapshot/restore.
//
#include <testinterface.h>

#include <memory>

#include "Core/ColorRGBA.h"
#include "Core/Keyboard.h"
#include "Core/Editor.h"
#include "Core/ClipBoard.h"
#include "Core/Graphics/Gansi/GansiScreen.h"
#include "Core/Graphics/Gansi/GansiWindow.h"
#include "Core/Graphics/Gansi/GansiKeyboardDriver.h"
#include "gansi/MockTerminalIO.h"
#include "gansi/Terminal.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_gansibackend(ITesting *t);
DLL_EXPORT int test_gansibackend_dimensions(ITesting *t);
DLL_EXPORT int test_gansibackend_window_offset(ITesting *t);
DLL_EXPORT int test_gansibackend_clip(ITesting *t);
DLL_EXPORT int test_gansibackend_color(ITesting *t);
DLL_EXPORT int test_gansibackend_hrule(ITesting *t);
DLL_EXPORT int test_gansibackend_overlay(ITesting *t);
DLL_EXPORT int test_gansibackend_keyboard(ITesting *t);
DLL_EXPORT int test_gansibackend_snapshot(ITesting *t);
DLL_EXPORT int test_gansibackend_clipboard_out(ITesting *t);
DLL_EXPORT int test_gansibackend_paste_in(ITesting *t);
}

// Build an opened GansiScreen + hand back the raw mock for input/output inspection.
static std::shared_ptr<Gansi::GansiScreen> MakeScreenRaw(int cols, int rows, gnilk::ansi::MockTerminalIO **outMock) {
    auto io = std::make_unique<gnilk::ansi::MockTerminalIO>();
    io->SetSize(cols, rows);
    *outMock = io.get();
    auto term = std::make_unique<gnilk::ansi::Terminal>(std::move(io));
    auto screen = std::make_shared<Gansi::GansiScreen>(std::move(term));
    screen->Open();
    return screen;
}

// Build an opened GansiScreen over a mock terminal of the given size.
static std::shared_ptr<Gansi::GansiScreen> MakeScreen(int cols, int rows) {
    auto io = std::make_unique<gnilk::ansi::MockTerminalIO>();
    io->SetSize(cols, rows);
    auto term = std::make_unique<gnilk::ansi::Terminal>(std::move(io));
    auto screen = std::make_shared<Gansi::GansiScreen>(std::move(term));
    screen->Open();
    screen->Clear();
    return screen;
}

DLL_EXPORT int test_gansibackend(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_gansibackend_dimensions(ITesting *t) {
    auto screen = MakeScreen(20, 10);
    auto dim = screen->Dimensions();
    TR_ASSERT(t, dim.Width() == 20);
    TR_ASSERT(t, dim.Height() == 10);
    return kTR_Pass;
}

DLL_EXPORT int test_gansibackend_window_offset(ITesting *t) {
    auto screen = MakeScreen(20, 10);
    auto *grid = screen->GetTerminal();
    // Window at (5,2), size 10x4. Local (0,0) -> absolute (5,2).
    auto *win = screen->CreateWindow(gedit::Rect(gedit::Point(5, 2), 10, 4), WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    win->GetContentDC().DrawStringAt(0, 0, std::u32string(U"Hi"));
    TR_ASSERT(t, grid->At(5, 2).ch == U'H');
    TR_ASSERT(t, grid->At(6, 2).ch == U'i');
    // A cell outside the window is untouched.
    TR_ASSERT(t, grid->At(0, 0).ch == U' ');
    delete win;
    return kTR_Pass;
}

DLL_EXPORT int test_gansibackend_clip(ITesting *t) {
    auto screen = MakeScreen(20, 10);
    auto *grid = screen->GetTerminal();
    auto *win = screen->CreateWindow(gedit::Rect(gedit::Point(5, 2), 10, 4), WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    auto &dc = win->GetContentDC();
    // Horizontal clip: local x 8,9 are inside (width 10), x 10 is clipped.
    dc.DrawStringAt(8, 0, std::u32string(U"XYZ"));
    TR_ASSERT(t, grid->At(13, 2).ch == U'X');   // 5+8
    TR_ASSERT(t, grid->At(14, 2).ch == U'Y');   // 5+9
    TR_ASSERT(t, grid->At(15, 2).ch == U' ');   // 5+10 clipped (outside window)
    // Vertical clip: local y 4 == height -> nothing drawn.
    dc.DrawStringAt(0, 4, std::u32string(U"Q"));
    TR_ASSERT(t, grid->At(5, 6).ch == U' ');
    delete win;
    return kTR_Pass;
}

DLL_EXPORT int test_gansibackend_color(ITesting *t) {
    auto screen = MakeScreen(20, 10);
    auto *grid = screen->GetTerminal();
    auto *win = screen->CreateWindow(gedit::Rect(gedit::Point(5, 2), 10, 4), WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    auto &dc = win->GetContentDC();
    dc.SetColor(ColorRGBA::FromRGB(255, 0, 0), ColorRGBA::FromRGB(0, 0, 255));
    dc.DrawStringAt(0, 1, std::u32string(U"A"));
    auto &cell = grid->At(5, 3);
    TR_ASSERT(t, cell.ch == U'A');
    TR_ASSERT(t, (cell.fg == gnilk::ansi::Color{255, 0, 0}));
    TR_ASSERT(t, (cell.bg == gnilk::ansi::Color{0, 0, 255}));
    delete win;
    return kTR_Pass;
}

DLL_EXPORT int test_gansibackend_hrule(ITesting *t) {
    auto screen = MakeScreen(20, 10);
    auto *grid = screen->GetTerminal();
    auto *win = screen->CreateWindow(gedit::Rect(gedit::Point(5, 2), 10, 4), WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    auto &dc = win->GetContentDC();
    dc.SetColor(ColorRGBA::FromRGB(255, 255, 255), ColorRGBA::FromRGB(0, 0, 0));
    dc.DrawHRule(0);
    TR_ASSERT(t, grid->At(5, 2).ch == U'─');     // box-drawing horizontal
    TR_ASSERT(t, grid->At(14, 2).ch == U'─');    // last cell of the window row
    delete win;
    return kTR_Pass;
}

DLL_EXPORT int test_gansibackend_overlay(ITesting *t) {
    auto screen = MakeScreen(20, 10);
    auto *grid = screen->GetTerminal();
    auto *win = screen->CreateWindow(gedit::Rect(gedit::Point(5, 2), 10, 4), WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    auto &dc = win->GetContentDC();
    dc.SetColor(ColorRGBA::FromRGB(200, 200, 200), ColorRGBA::FromRGB(0, 0, 0));
    dc.DrawStringAt(0, 0, std::u32string(U"AB"));

    DrawContext::Overlay ov;
    ov.isActive = true;
    ov.Set(gedit::Point(0, 0), gedit::Point(2, 0));   // covers local cols 0..1 on row 0
    dc.AddOverlay(ov);
    dc.DrawLineOverlays(0);

    // Covered cells: fg/bg swapped (inverse highlight), glyph preserved.
    auto &cell = grid->At(5, 2);
    TR_ASSERT(t, cell.ch == U'A');
    TR_ASSERT(t, (cell.fg == gnilk::ansi::Color{0, 0, 0}));
    TR_ASSERT(t, (cell.bg == gnilk::ansi::Color{200, 200, 200}));
    delete win;
    return kTR_Pass;
}

DLL_EXPORT int test_gansibackend_keyboard(ITesting *t) {
    Gansi::GansiKeyboardDriver kb;

    gnilk::ansi::KeyEvent charEv;
    charEv.isChar = true;
    charEv.ch = U'a';
    KeyPress kp = kb.TranslateKeyEvent(charEv);
    TR_ASSERT(t, kp.isKeyValid && !kp.isSpecialKey && kp.key == U'a');

    gnilk::ansi::KeyEvent upEv;
    upEv.isChar = false;
    upEv.key = gnilk::ansi::Key::Up;
    upEv.mods = gnilk::ansi::Mod::Shift;
    KeyPress kp2 = kb.TranslateKeyEvent(upEv);
    TR_ASSERT(t, kp2.isSpecialKey);
    TR_ASSERT(t, kp2.specialKey == Keyboard::kKeyCode_UpArrow);
    TR_ASSERT(t, kp2.IsShiftPressed());
    return kTR_Pass;
}

DLL_EXPORT int test_gansibackend_snapshot(ITesting *t) {
    auto screen = MakeScreen(20, 10);
    auto *grid = screen->GetTerminal();
    auto *win = screen->CreateWindow(gedit::Rect(gedit::Point(0, 0), 20, 10), WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    win->GetContentDC().DrawStringAt(0, 0, std::u32string(U"Z"));
    screen->CopyToTexture();        // snapshot

    grid->At(0, 0).ch = U'!';       // scribble over (as a modal would)
    screen->ClearWithTexture();     // restore
    TR_ASSERT(t, grid->At(0, 0).ch == U'Z');
    delete win;
    return kTR_Pass;
}

// Outbound clipboard: an editor clipboard change fires the OnUpdate delegate that GansiScreen::Open
// registered, which emits OSC 52 to the terminal. (Open registers it on the global Editor clipboard;
// the screen destructor clears it.)
DLL_EXPORT int test_gansibackend_clipboard_out(ITesting *t) {
    gnilk::ansi::MockTerminalIO *mock = nullptr;
    auto screen = MakeScreenRaw(20, 10, &mock);
    mock->ClearWritten();

    Editor::Instance().GetClipBoard().CopyText(U"hi");
    // OSC 52 ; c ; base64("hi"="aGk=")
    TR_ASSERT(t, mock->Written().find("\x1b]52;c;aGk=") != std::string::npos);
    return kTR_Pass;
}

// Inbound paste: a bracketed-paste sequence on stdin surfaces as a PasteEvent. The text is loaded onto
// the clipboard via CopyFromExternal (the E.18 external item, reusing AsText()) DIRECTLY in PollEvents;
// the follow-up PasteFromClipboard action is posted to the runloop queue (verified in test_runloop) and
// is not processed here, so we assert only the synchronous clipboard load.
DLL_EXPORT int test_gansibackend_paste_in(ITesting *t) {
    gnilk::ansi::MockTerminalIO *mock = nullptr;
    auto screen = MakeScreenRaw(20, 10, &mock);
    mock->QueueInput("\x1b[200~world\x1b[201~");

    screen->PollEvents();   // drains the paste event -> CopyFromExternal (action dispatch is posted)

    auto top = Editor::Instance().GetClipBoard().Top();
    TR_ASSERT(t, top != nullptr);
    TR_ASSERT(t, top->AsText() == std::u32string(U"world"));
    return kTR_Pass;
}
