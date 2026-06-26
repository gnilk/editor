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
    // Full-screen apps (alt-screen) and an in-progress shell completion both let
    // readline/the app own the line, so actions are forwarded straight to the pty.
    if (controller.GetScreen().IsAltScreen() || controller.DoesShellOwnLineEditing()) {
        return controller.ForwardActionToShell(kpAction);
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

bool TerminalView::OnMouseEvent(const MouseEvent &mouseEvent) {
    if (mouseEvent.kind != MouseEvent::kMouseEventKind_Wheel) {
        return ViewBase::OnMouseEvent(mouseEvent);
    }
    // Alt-screen apps (vi/less) own scrolling themselves (§5.2: never scrollable here).
    if (controller.GetScreen().IsAltScreen()) {
        return false;
    }
    controller.ScrollViewport(-(int64_t)mouseEvent.wheelDelta * linesPerScrollWheelNotch);
    InvalidateView();
    return true;
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

    std::lock_guard<std::mutex> guard(controller.GetScreenLock());
    auto &screen = controller.GetScreen();
    int viewHeight = dc.GetRect().Height();

    if (screen.IsAltScreen()) {
        // Full-screen mode (vi, less, ...): render the entire grid directly.
        // No inputLine overlay — keystrokes go straight to the pty.
        for (int y = 0; y < viewHeight; y++) {
            dc.ClearLine(y);
            if (y < screen.Rows()) {
                DrawScreenRow(dc, screen.GetRow(y), y);
            }
        }
        cursor.position.x = screen.GetCursorPos().x;
        cursor.position.y = screen.GetCursorPos().y;
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
            dc.SetFGColor(highlightColor);
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

void TerminalView::DrawScreenRow(DrawContext &dc, const TerminalScreen::Row &row, int y) {
    if (row.empty()) {
        return;
    }
    // Render as colour runs: consecutive cells with the same fg+bg are batched into one draw call
    int xStart = 0;
    std::u32string run;
    ColorRGBA runFg = row[0].fg;
    ColorRGBA runBg = row[0].bg;

    auto flush = [&](int upToX) {
        if (run.empty()) {
            return;
        }
        dc.SetColor(runFg, runBg);
        dc.DrawStringAt(xStart, y, run);
        run.clear();
        xStart = upToX;
    };

    for (int x = 0; x < (int)row.size(); x++) {
        if (row[x].fg == runFg && row[x].bg == runBg) {
            run += row[x].ch;
        } else {
            flush(x);
            runFg = row[x].fg;
            runBg = row[x].bg;
            run += row[x].ch;
        }
    }
    flush((int)row.size());
}
