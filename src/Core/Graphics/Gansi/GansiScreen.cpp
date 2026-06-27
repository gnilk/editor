//
// gedit::Gansi screen implementation.
//
#include "Core/Graphics/Gansi/GansiScreen.h"
#include "Core/Graphics/Gansi/GansiWindow.h"
#include "Core/Graphics/Gansi/GansiKeyboardDriver.h"

#include "Core/Editor.h"
#include "Core/MouseEvent.h"
#include "Core/Runloop.h"
#include "Core/RuntimeConfig.h"
#include "Core/UnicodeHelper.h"
#include "gansi/Capabilities.h"
#include "gansi/PosixTerminalIO.h"

#include <variant>

using namespace gedit;
using namespace gedit::Gansi;

GansiScreen::GansiScreen(std::unique_ptr<gnilk::ansi::Terminal> terminalIn)
    : terminal(std::move(terminalIn)) {
    logger = gnilk::Logger::GetLogger("GansiScreen");
}

GansiScreen::~GansiScreen() {
    Close();
}

ScreenBase::Ref GansiScreen::Create() {
    auto io = std::make_unique<gnilk::ansi::PosixTerminalIO>();
    // Detect what this terminal actually supports (TERM / TERM_PROGRAM / COLORTERM) so we never emit a
    // sequence it mis-handles — e.g. Apple_Terminal gets no Kitty keyboard / focus / SGR mouse / OSC 52
    // and falls back to 256-colour, which renders correctly there.
    auto caps = gnilk::ansi::Capabilities::Detect();
    auto term = std::make_unique<gnilk::ansi::Terminal>(std::move(io), caps);
    return std::make_shared<GansiScreen>(std::move(term));
}

bool GansiScreen::Open() {
    if (isOpen) {
        return true;
    }
    if (!terminal->Open()) {
        if (logger) logger->Error("gansi Terminal::Open failed (raw mode unavailable?)");
        return false;
    }

    // Outbound clipboard: when the editor's clipboard changes, push it to the terminal via OSC 52
    // (works over SSH where the terminal forwards it). Serialization stays in AsText() (E.18). Inbound
    // paste arrives as a bracketed-paste event -> ClipBoard::CopyFromExternal (see DispatchEvent).
    gnilk::ansi::Terminal *term = terminal.get();
    Editor::Instance().GetClipBoard().SetOnUpdateCallback([term](ClipBoard::ClipBoardItem::Ref item) {
        term->SetClipboard(UnicodeHelper::utf32to8(item->AsText()));
    });

    isOpen = true;
    return true;
}

void GansiScreen::Close() {
    if (!isOpen) {
        return;
    }
    // Drop the clipboard delegate before tearing down the terminal it captures.
    Editor::Instance().GetClipBoard().SetOnUpdateCallback(nullptr);
    terminal->Close();
    isOpen = false;
}

void GansiScreen::Clear() {
    terminal->Clear(gnilk::ansi::kDefaultBg);
}

void GansiScreen::Update() {
    terminal->SetCursor(cursorCol, cursorRow, cursorVisible);
    terminal->Flush();
}

void GansiScreen::CopyToTexture() {
    terminal->SaveSnapshot();
}

void GansiScreen::ClearWithTexture() {
    terminal->RestoreSnapshot();
}

gedit::Rect GansiScreen::Dimensions() {
    // Qualify: macOS Carbon headers (pulled in transitively via Editor.h) also define a ::Rect.
    return gedit::Rect(terminal->Cols(), terminal->Rows());
}

WindowBase *GansiScreen::CreateWindow(const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) {
    auto win = new GansiWindow(rect, terminal.get(), this);
    win->Initialize(flags, decoFlags);
    return win;
}

WindowBase *GansiScreen::UpdateWindow(WindowBase *window, const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) {
    auto win = static_cast<GansiWindow *>(window);
    if (win != nullptr) {
        win->UpdateRect(rect);
        win->Initialize(flags, decoFlags);
    }
    return window;
}

void GansiScreen::SetActiveCursor(int col, int row, bool visible) {
    cursorCol = col;
    cursorRow = row;
    cursorVisible = visible;
}

void GansiScreen::OnSizeChanged() {
    // The Terminal already re-sized its grid when the ResizeEvent surfaced; relayout the UI.
    auto &rootView = RuntimeConfig::Instance().GetRootView();
    rootView.Resize();
    rootView.InvalidateAll();
    rootView.PostMessage([]() {});
}

void GansiScreen::DispatchEvent(const gnilk::ansi::Event &ev) {
    if (std::holds_alternative<gnilk::ansi::KeyEvent>(ev)) {
        auto keyDriver = std::static_pointer_cast<GansiKeyboardDriver>(RuntimeConfig::Instance().GetKeyboard());
        if (keyDriver == nullptr) {
            return;
        }
        KeyPress kp = keyDriver->TranslateKeyEvent(std::get<gnilk::ansi::KeyEvent>(ev));
        if (kp.IsAnyValid()) {
            Runloop::PostMessage(0, [kp](uint32_t) {
                Runloop::ProcessKeyPress(kp);
            });
        }
        return;
    }

    if (std::holds_alternative<gnilk::ansi::MouseEvent>(ev)) {
        const auto &m = std::get<gnilk::ansi::MouseEvent>(ev);
        auto keyDriver = std::static_pointer_cast<GansiKeyboardDriver>(RuntimeConfig::Instance().GetKeyboard());
        MouseEvent me;
        me.x = m.col;
        me.y = m.row;
        me.button = m.button + 1;       // gansi 0/1/2 -> editor 1/2/3 (SDL-style)
        me.wheelDelta = m.wheel;
        me.modifiers = keyDriver ? keyDriver->TranslateModifiers(m.mods) : 0;
        switch (m.kind) {
            case gnilk::ansi::MouseEvent::Kind::Press:   me.kind = MouseEvent::kMouseEventKind_Press; break;
            case gnilk::ansi::MouseEvent::Kind::Release: me.kind = MouseEvent::kMouseEventKind_Release; break;
            case gnilk::ansi::MouseEvent::Kind::Move:    me.kind = MouseEvent::kMouseEventKind_Move; break;
            case gnilk::ansi::MouseEvent::Kind::Wheel:   me.kind = MouseEvent::kMouseEventKind_Wheel; break;
        }
        Runloop::PostMessage(0, [me](uint32_t) {
            Runloop::ProcessMouseEvent(me);
        });
        return;
    }

    if (std::holds_alternative<gnilk::ansi::ResizeEvent>(ev)) {
        OnSizeChanged();
        return;
    }

    if (std::holds_alternative<gnilk::ansi::PasteEvent>(ev)) {
        // Bracketed paste (e.g. the terminal handled Cmd+V itself). Load the text as the top clipboard
        // item now — clipboard events are handled directly in PollEvents — then POST a normal
        // PasteFromClipboard action so the insert + redraw happen in the message-processing phase like a
        // keypress-driven paste: the editor view inserts at the caret with undo, the terminal controller
        // writes it to the pty.
        const auto &paste = std::get<gnilk::ansi::PasteEvent>(ev);
        Editor::Instance().GetClipBoard().CopyFromExternal(UnicodeHelper::utf32to8(paste.text).c_str());
        Runloop::PostMessage(0, [](uint32_t) {
            EditorAction action;
            action.action = kAction::kActionPasteFromClipboard;
            Runloop::DispatchAction(action);
        });
        return;
    }
    // FocusEvent: nothing to do for now.
}

void GansiScreen::PollEvents() {
    // Block up to ~250 ms for the first event, then drain anything already parsed.
    auto ev = terminal->PollEvent(250);
    while (ev.has_value()) {
        DispatchEvent(*ev);
        ev = terminal->PollEvent(0);
    }
}
