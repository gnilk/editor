//
// Created by gnilk on 22.02.2024.
//

#include <algorithm>
#include <mutex>

#include "TerminalView.h"
#include "Core/Editor.h"
#include "Core/LineRender.h"

using namespace gedit;

static const std::string cfgSectionName = "commandview";

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

bool TerminalView::OnAction(const KeyPressAction &kpAction) {
    switch (kpAction.action) {
        case kAction::kActionCommitLine :
            return OnActionCommitLine();
        default:
            break;
    }
    if (controller.OnAction(kpAction)) {
        cursor.position.x = controller.GetCursorXPos();
        return true;
    }
    return ViewBase::OnAction(kpAction);
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
    auto &scrollback = screen.GetScrollback();

    int viewHeight = dc.GetRect().Height();

    // The cursor row holds the current shell output (prompt). Everything above
    // it is history. The cursor row itself is composed with the user's inputLine
    // and shown at the bottom — this keeps the prompt and user input on the same
    // visual line, matching the original lastLine+inputLine behaviour.
    int cursorGridRow = screen.GetCursorPos().y;
    int promptLen     = screen.GetCursorPos().x;

    int totalHistory = (int)scrollback.size() + cursorGridRow;
    int startIdx = std::max(0, totalHistory - (viewHeight - 1));

    for (int viewY = 0; viewY < viewHeight - 1; viewY++) {
        int idx = startIdx + viewY;
        dc.ClearLine(viewY);
        if (idx < (int)scrollback.size()) {
            DrawScreenRow(dc, scrollback[idx], viewY);
        } else {
            int gridY = idx - (int)scrollback.size();
            if (gridY < cursorGridRow) {
                DrawScreenRow(dc, screen.GetRow(gridY), viewY);
            }
        }
    }
    auto inputLine = controller.GetInputLine();
    auto currentLine = Line::Create();
    auto &cursorRow = screen.GetRow(cursorGridRow);
    for (int x = 0; x < promptLen && x < (int)cursorRow.size(); x++) {
        currentLine->Append(cursorRow[x].ch);
    }
    currentLine->Append(inputLine);

    int inputViewRow = viewHeight - 1;
    dc.ClearLine(inputViewRow);
    LineRender lineRender(dc);
    lineRender.DrawLine(0, inputViewRow, currentLine);

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
