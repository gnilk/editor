//
// Created by gnilk on 22.02.2024.
//

#include <algorithm>
#include <mutex>
#include <unordered_set>

#include "TerminalView.h"
#include "Core/Editor.h"
#include "Core/Editor/LineRender.h"
#include "Core/TextBuffer.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

static const std::string cfgSectionName = "terminal";

void TerminalView::InitView() {
    logger = gnilk::Logger::GetLogger("TerminalView");
    auto screen = RuntimeConfig::Instance().GetScreen();
    logger->Debug("InitView!");
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    window->SetCaption("Terminal");

    controller.Begin();

    linesPerScrollWheelNotch = Config::Instance()[cfgSectionName].GetInt("lines_per_scroll_wheel_notch", 3);
    showBlockMarkers = Config::Instance()[cfgSectionName].GetBool("show_block_markers", false);

    // Size the terminal screen to the full content area.
    // The grid's last row is the cursor row (prompt); it is rendered as the input
    // composition line in DrawViewContents rather than in the history loop.
    auto &dc = window->GetContentDC();
    controller.Resize(dc.GetRect().Width(), dc.GetRect().Height());
}

void TerminalView::ReInitView() {
    logger->Debug("ReInitialize View!");
    auto screen = RuntimeConfig::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);

    linesPerScrollWheelNotch = Config::Instance()[cfgSectionName].GetInt("lines_per_scroll_wheel_notch", 3);
    showBlockMarkers = Config::Instance()[cfgSectionName].GetBool("show_block_markers", false);

    auto &dc = window->GetContentDC();
    controller.Resize(dc.GetRect().Width(), dc.GetRect().Height());
}

void TerminalView::OnActivate(bool isActive) {
    logger->Debug("OnActive, isActive: %s", isActive ? "yes" : "no");
    controller.SetFocus(isActive);
    if (!isActive) {
        parentView->RestoreContentHeight();
    } else {
        parentView->RestoreContentHeight();
        Editor::Instance().SetActiveKeyMapping(Config::Instance()[cfgSectionName].GetStr("keymap", "default_keymap"));
    }
}

void TerminalView::OnKeyPress(const KeyPress &keyPress) {
    logger->Debug("OnKeyPress");
    auto strCursor = cursor;
    size_t dummyLineIndex = 0;
    if (controller.HandleKeyPress(strCursor, dummyLineIndex, keyPress)) {
        cursor = strCursor;
        return;
    }
    ViewBase::OnKeyPress(keyPress);
}

bool TerminalView::OnAction(const EditorAction &kpAction) {
    if (kpAction.action == kAction::kActionPasteFromClipboard) {
        PasteFromClipboard();
        return true;
    }
    // An app owns the terminal (alt screen, foreground job) or readline owns the line: the action's key goes to
    // the pty. Escape-hatch actions (view/buffer switching, resizing, Cmd) come back unhandled - the view takes
    // its own (resize) and the rest falls through to the editor.
    if (controller.IsAppActive() || controller.DoesShellOwnLineEditing()) {
        if (controller.ForwardActionToShell(kpAction)) {
            return true;
        }
        return ViewBase::OnAction(kpAction);
    }
    if (kpAction.uiAction == kUIAction::kUIActionCommitLine) {
        return OnActionCommitLine();
    }
    if (controller.OnAction(kpAction)) {
        cursor.position.x = controller.GetCursorXPos();
        return true;
    }
    return ViewBase::OnAction(kpAction);
}

// What another application put on the OS pasteboard first (E.18: paste round-trips through the external item)
void TerminalView::PasteFromClipboard() {
    auto screen = RuntimeConfig::Instance().GetScreen();
    if (screen != nullptr) {
        screen->UpdateClipboardData();
    }
    auto item = Editor::Instance().GetClipBoard().Top();
    if (item == nullptr) {
        return;
    }
    controller.Paste(item->AsText());
    cursor.position.x = controller.GetCursorXPos();
    InvalidateView();
}

bool TerminalView::OnMouseEvent(const MouseEvent &mouseEvent) {
    if (controller.IsAppActive()) {
        // Shift keeps the mouse for the editor even when the app reports it (xterm's override)
        bool isShift = (mouseEvent.modifiers & (Keyboard::kMod_LeftShift | Keyboard::kMod_RightShift)) != 0;
        if (!isShift && ForwardMouseToApp(mouseEvent)) {
            return true;
        }
        // Alt-screen apps (vi/less) own scrolling themselves (§5.2: never scrollable here)
        if (controller.GetScreen().IsAltScreen() && (mouseEvent.kind == MouseEvent::kMouseEventKind_Wheel)) {
            return false;
        }
    }
    if (mouseEvent.kind != MouseEvent::kMouseEventKind_Wheel) {
        return ViewBase::OnMouseEvent(mouseEvent);
    }
    controller.ScrollViewport(-(int64_t)mouseEvent.wheelDelta * linesPerScrollWheelNotch);
    InvalidateView();
    return true;
}

// Mouse reporting (?1000/1002/1003, SGR or X10 bytes): the event at its grid cell, if the app's mode reports it
bool TerminalView::ForwardMouseToApp(const MouseEvent &mouseEvent) {
    using gnilk::vt::MouseAction;
    using gnilk::vt::MouseButton;
    auto origin = GetContentRect().TopLeft();
    int x = mouseEvent.x - origin.x;
    int y = mouseEvent.y - origin.y + gridRowAtViewTop;
    if ((x < 0) || (y < 0)) {
        return false;
    }
    uint8_t modifiers = gnilk::vt::kModNone;
    if (mouseEvent.modifiers & (Keyboard::kMod_LeftAlt | Keyboard::kMod_RightAlt)) {
        modifiers |= gnilk::vt::kModAlt;
    }
    if (mouseEvent.modifiers & (Keyboard::kMod_LeftCtrl | Keyboard::kMod_RightCtrl)) {
        modifiers |= gnilk::vt::kModCtrl;
    }
    // SDL numbers the buttons 1 left, 2 middle, 3 right
    auto button = (mouseEvent.button == 2) ? MouseButton::kMiddle :
                  (mouseEvent.button == 3) ? MouseButton::kRight : MouseButton::kLeft;
    switch (mouseEvent.kind) {
        case MouseEvent::kMouseEventKind_Press:
            return controller.ForwardMouse(button, MouseAction::kPress, x, y, modifiers);
        case MouseEvent::kMouseEventKind_Release:
            return controller.ForwardMouse(button, MouseAction::kRelease, x, y, modifiers);
        case MouseEvent::kMouseEventKind_Move:
            return controller.ForwardMouse(button, MouseAction::kMotion, x, y, modifiers);
        case MouseEvent::kMouseEventKind_Hover:
            return controller.ForwardMouse(MouseButton::kNone, MouseAction::kMotion, x, y, modifiers);
        case MouseEvent::kMouseEventKind_Wheel: {
            auto wheel = (mouseEvent.wheelDelta > 0) ? MouseButton::kWheelUp : MouseButton::kWheelDown;
            bool isSent = false;
            for (int i = 0; i < std::abs(mouseEvent.wheelDelta); i++) {
                isSent = controller.ForwardMouse(wheel, MouseAction::kPress, x, y, modifiers) || isSent;
            }
            return isSent;
        }
    }
    return false;
}

bool TerminalView::OnActionCommitLine() {
    controller.CommitLine();
    return true;
}

void TerminalView::DrawViewContents() {
    auto &dc = window->GetContentDC();
    dc.ResetDrawColors();
    auto termColors = Editor::Instance().GetTheme()->GetTerminalColor();
    dc.SetColor(termColors.GetColor("foreground"), termColors.GetColor("background"));
    dc.Clear();

    // Before taking the lock - IsAppActive takes it too
    bool isAppActive = controller.IsAppActive();

    std::lock_guard<std::mutex> guard(controller.GetScreenLock());
    auto &screen = controller.GetScreen();
    int viewHeight = dc.GetRect().Height();
    // The editor's bar at the local prompt; an app's own caret (DECSCUSR, DECTCEM) while it owns the terminal
    cursor.shape = Cursor::Shape::kBar;
    cursor.isVisible = true;

    // An app's screen: during a synchronized update (?2026) the frame from before it - whatever else asks for a
    // redraw meanwhile (other views, the caret) - so a half-drawn frame is never shown
    bool isAppScreen = screen.IsAltScreen() || (isAppActive && controller.IsFollowingBottom());
    auto heldFrame = controller.IsHoldingRedraw() ? screen.GetHeldFrame() : nullptr;
    if ((heldFrame != nullptr) && (isAppScreen || heldFrame->isAltScreen)) {
        DrawAppFrame(dc, screen, *heldFrame, viewHeight);
        return;
    }
    if (isAppScreen) {
        DrawAppFrame(dc, screen, screen.CaptureFrame(), viewHeight);
        return;
    }

    // Shell mode: scrollback + grid history above. The history window is H = scrollback ++
    // grid[0..cursorGridRow), addressed by abs id (§3.1/§5.2 of docs/terminal-scrollback.md).
    LineRender lineRender(dc);
    uint64_t historyTop    = screen.ScrollbackBase();
    uint64_t historyBottom = screen.AbsRowCount();   // exclusive — the live/cursor row is NOT in H

    // Block-end boundaries for the separator rule (§5.5): the abs id one past each CLOSED block's
    // last row. A rule is drawn under the row at abs when (abs+1) is a boundary. The open (running)
    // block has no endAbsRow → no rule at the live tail. Built once per frame, O(blocks).
    std::unordered_set<uint64_t> blockEnds;
    if (showBlockMarkers) {
        for (auto &block : screen.Blocks()) {
            if (block.endAbsRow.has_value()) {
                blockEnds.insert(*block.endAbsRow);
            }
        }
    }
    ColorRGBA markerColor = termColors.GetColor("foreground") * 0.5f;   // muted text color
    markerColor.SetAlpha(1.0f);

    // Selected-block highlight (§5.5.2): a translucent overlay over the block the viewport has
    // selected (only ever set while scrolled). drawHighlight is flipped on in the scrolled branch
    // once the overlay rectangle is known; the overlay is drawn ON TOP of the row text (after it),
    // mirroring how EditorView paints search/selection highlights. Stale overlays from the prior
    // frame are dropped here.
    dc.ClearOverlays();
    ColorRGBA highlightColor = termColors.HasColor("selection")
                                 ? termColors.GetColor("selection")
                                 : termColors.GetColor("foreground");
    if (!termColors.HasColor("selection")) {
        highlightColor.SetAlpha(0.25f);   // keep the row text readable under the tint
    }
    bool drawHighlight = false;

    auto drawAbsRow = [&](uint64_t abs, int viewY) {
        auto resolved = screen.RowAtAbs(abs);
        if (std::holds_alternative<Line::Ref>(resolved)) {
            lineRender.DrawLine(0, viewY, std::get<Line::Ref>(resolved));
        } else if (std::holds_alternative<const TerminalScreen::Row *>(resolved)) {
            DrawScreenRow(dc, *std::get<const TerminalScreen::Row *>(resolved), viewY);
        }
        // monostate (evicted/out of range) — line was already cleared, leave it blank.
        if (showBlockMarkers && blockEnds.count(abs + 1)) {
            dc.SetFGColor(markerColor);
            dc.DrawHRule(viewY);
        }
        if (drawHighlight) {
            // The overlay carries highlightColor itself now (set at AddOverlay) — no app-set fg here.
            dc.DrawLineOverlays(viewY);
        }
    };

    if (!controller.IsFollowingBottom()) {
        // Scrolled: the top-visible row is pinned to anchorAbsRow (stays stationary as new
        // output appends below) — no input composite, you're not at the prompt.
        uint64_t windowTop = std::clamp(controller.GetAnchorAbsRow(), historyTop, historyBottom);

        // Highlight the selected block (§5.5.2) — only while scrolled, only with markers enabled.
        // Map the block's [startAbsRow, endAbsRow) to viewY and add one full-width overlay; the
        // per-row DrawLineOverlays in drawAbsRow paints it over the text.
        if (showBlockMarkers) {
            auto selIdx = controller.SelectedBlockIndex();
            if (selIdx.has_value()) {
                const auto &block = screen.Blocks()[*selIdx];
                uint64_t selEnd = block.endAbsRow.value_or(historyBottom);   // exclusive
                int64_t firstViewY = (int64_t)block.startAbsRow - (int64_t)windowTop;
                int64_t lastViewY  = (int64_t)selEnd - 1 - (int64_t)windowTop;
                firstViewY = std::max<int64_t>(firstViewY, 0);
                lastViewY  = std::min<int64_t>(lastViewY, viewHeight - 2);   // history rows: [0, viewHeight-1)
                if (lastViewY >= firstViewY) {
                    DrawContext::Overlay overlay;
                    overlay.Set(Point(0, (int)firstViewY), Point(dc.GetRect().Width(), (int)lastViewY));
                    overlay.color = highlightColor;
                    overlay.isActive = true;
                    dc.AddOverlay(overlay);
                    drawHighlight = true;
                }
            }
        }

        for (int viewY = 0; viewY < viewHeight - 1; viewY++) {
            dc.ClearLine(viewY);
            uint64_t abs = windowTop + (uint64_t)viewY;
            if (abs < historyBottom) {
                drawAbsRow(abs, viewY);
            }
        }

        // Bottom row: a "scrolled" affordance instead of the prompt/input composite.
        int affordanceRow = viewHeight - 1;
        dc.ClearLine(affordanceRow);
        uint64_t shown    = std::min((uint64_t)(viewHeight - 1), historyBottom - windowTop);
        uint64_t below    = historyBottom - (windowTop + shown);   // rows + live prompt not yet shown
        std::u32string affordance = U"-- scrolled, " + UnicodeHelper::utf8to32(std::to_string(below))
                                   + U" more line" + (below == 1 ? U"" : U"s") + U" below (End to return) --";
        dc.SetColor(termColors.GetColor("background"), termColors.GetColor("foreground"));
        dc.DrawStringAt(0, affordanceRow, affordance);

        cursor.position.y = affordanceRow;
        cursor.position.x = 0;
        return;
    }

    int cursorGridRow = screen.GetCursorPos().y;
    int promptLen     = screen.GetCursorPos().x;

    // Following the bottom (default): anchor content to the bottom with blank space at the top
    // when it is shorter than the view (standard terminal behaviour — content flows up from prompt).
    int historyRows = (int)(historyBottom - historyTop);
    uint64_t windowTop = historyBottom - (uint64_t)std::min(historyRows, viewHeight - 1);
    int blankRows = std::max(0, (viewHeight - 1) - historyRows);

    for (int viewY = 0; viewY < viewHeight - 1; viewY++) {
        dc.ClearLine(viewY);
        if (viewY < blankRows) {
            continue;
        }
        drawAbsRow(windowTop + (uint64_t)(viewY - blankRows), viewY);
    }

    // Bottom row: render the prompt portion using DrawScreenRow so per-cell
    // colors are preserved, then draw the user's inputLine text after it.
    int inputViewRow = viewHeight - 1;
    dc.ClearLine(inputViewRow);

    auto &cursorRow = screen.GetRow(cursorGridRow);

    if (controller.DoesShellOwnLineEditing()) {
        // Shell completion in progress: readline owns the line and has echoed the whole
        // prompt+input into the grid row, so render it directly and place the caret at
        // the grid cursor (no inputLine composite — that would double the text).
        DrawScreenRow(dc, cursorRow, inputViewRow);
        cursor.position.y = inputViewRow;
        cursor.position.x = screen.GetCursorPos().x;
        return;
    }

    if (promptLen > 0) {
        TerminalScreen::Row promptPart(cursorRow.begin(),
                                      cursorRow.begin() + std::min(promptLen, (int)cursorRow.size()));
        DrawScreenRow(dc, promptPart, inputViewRow);
    }

    dc.SetColor(termColors.GetColor("foreground"), termColors.GetColor("background"));
    auto inputLine = controller.GetInputLine();
    if (inputLine->Length() > 0) {
        dc.DrawStringAt(promptLen, inputViewRow, inputLine->Buffer());
    }

    cursor.position.y = inputViewRow;
    cursor.position.x = controller.GetCursorXPos();
}

// A foreground job on the primary screen (dialog, python, make, ...): like the shell view - bottom-anchored, history
// above - but down to the last row the app has drawn on, which may lie below the cursor, and without the local
// input line (keys go to the app). Output that just streams (cursor on the last content row) looks exactly as it
// does at the prompt. Caller holds screenLock.
void TerminalView::DrawAppFrame(DrawContext &dc, const TerminalScreen &screen, const TerminalScreen::Frame &frame,
                                int viewHeight) {
    cursor.shape = ToCursorShape(frame.cursorShape);
    cursor.isVisible = frame.isCursorVisible;
    if (frame.isAltScreen) {
        // Full-screen mode (vi, less, ...): render the entire grid directly.
        // No inputLine overlay — keystrokes go straight to the pty.
        for (int y = 0; y < viewHeight; y++) {
            dc.ClearLine(y);
            DrawScreenRow(dc, frame.GetRow(y), y);
        }
        gridRowAtViewTop = 0;
        cursor.position.x = frame.cursor.x;
        cursor.position.y = frame.cursor.y;
        return;
    }
    DrawAppScreen(dc, screen, frame, viewHeight);
}

void TerminalView::DrawAppScreen(DrawContext &dc, const TerminalScreen &screen, const TerminalScreen::Frame &frame,
                                 int viewHeight) {
    auto background = Editor::Instance().GetTheme()->GetTerminalColor().GetColor("background");
    auto isRowEmpty = [&](const TerminalScreen::Row &row) {
        for (auto &cell : row) {
            if (((cell.ch != U' ') && (cell.ch != 0)) || (cell.bg != background)) {
                return false;
            }
        }
        return true;
    };
    int cursorY = frame.cursor.y;
    int lastRow = cursorY;
    for (int y = (int)frame.rows.size() - 1; y > cursorY; y--) {
        if (!isRowEmpty(frame.GetRow(y))) {
            lastRow = y;
            break;
        }
    }

    LineRender lineRender(dc);
    uint64_t historyTop = screen.ScrollbackBase();
    uint64_t gridTop = std::max(frame.gridTopAbs, historyTop);              // abs id of grid row 0
    uint64_t contentEnd = gridTop + (uint64_t)lastRow + 1;                  // exclusive
    int shown = (int)std::min<uint64_t>(contentEnd - historyTop, (uint64_t)viewHeight);
    uint64_t windowTop = contentEnd - (uint64_t)shown;
    int blankRows = viewHeight - shown;

    for (int viewY = 0; viewY < viewHeight; viewY++) {
        dc.ClearLine(viewY);
        if (viewY < blankRows) {
            continue;
        }
        uint64_t abs = windowTop + (uint64_t)(viewY - blankRows);
        if (abs < gridTop) {
            auto resolved = screen.RowAtAbs(abs);
            if (std::holds_alternative<Line::Ref>(resolved)) {
                lineRender.DrawLine(0, viewY, std::get<Line::Ref>(resolved));
            }
        } else {
            DrawScreenRow(dc, frame.GetRow((int)(abs - gridTop)), viewY);
        }
    }
    gridRowAtViewTop = (int)((int64_t)windowTop - (int64_t)gridTop) - blankRows;
    cursor.position.x = frame.cursor.x;
    cursor.position.y = cursorY - gridRowAtViewTop;
}

// Colour runs: consecutive cells with the same fg, bg and attributes are one draw call, which also paints the
// run's background (cell backgrounds, inverse) and its underline / strike-through.
void TerminalView::DrawScreenRow(DrawContext &dc, const TerminalScreen::Row &row, int y) {
    if (row.empty()) {
        return;
    }
    auto toTextAttributes = [](uint8_t attrs) {
        auto result = kTextAttributes::kNormal;
        if (attrs & TerminalScreen::kAttrBold)      { result = result | kTextAttributes::kBold; }
        if (attrs & TerminalScreen::kAttrItalic)    { result = result | kTextAttributes::kItalic; }
        if (attrs & TerminalScreen::kAttrUnderline) { result = result | kTextAttributes::kUnderline; }
        if (attrs & TerminalScreen::kAttrStrike)    { result = result | kTextAttributes::kStrikethrough; }
        return result;
    };

    int xStart = 0;
    std::u32string run;
    const TerminalScreen::Cell *runCell = &row[0];

    auto flush = [&](int upToX) {
        if (run.empty()) {
            return;
        }
        dc.SetColor(runCell->fg, runCell->bg);
        dc.DrawStringWithAttributesAt(xStart, y, toTextAttributes(runCell->attrs), run);
        run.clear();
        xStart = upToX;
    };

    for (int x = 0; x < (int)row.size(); x++) {
        const auto &cell = row[x];
        if ((cell.fg != runCell->fg) || (cell.bg != runCell->bg) || (cell.attrs != runCell->attrs)) {
            flush(x);
            runCell = &cell;
        }
        run += (cell.ch != 0) ? cell.ch : U' ';   // a wide character's right half
    }
    flush((int)row.size());
}

// DECSCUSR's shapes; the default (0) is a block, as in xterm
Cursor::Shape TerminalView::ToCursorShape(gnilk::vt::CursorShape shape) {
    switch (shape) {
        case gnilk::vt::CursorShape::kBar:
            return Cursor::Shape::kBar;
        case gnilk::vt::CursorShape::kUnderline:
            return Cursor::Shape::kUnderline;
        default:
            return Cursor::Shape::kBlock;
    }
}
