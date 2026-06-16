//
// Created by gnilk on 22.02.2024.
//

#include <algorithm>
#include <mutex>

#include "TerminalView.h"
#include "Core/Editor.h"
#include "Core/Editor/LineRender.h"

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

bool TerminalView::OnAction(const EditorAction &kpAction) {
    // Full-screen apps (alt-screen) and an in-progress shell completion both let
    // readline/the app own the line, so actions are forwarded straight to the pty.
    if (controller.GetScreen().IsAltScreen() || controller.DoesShellOwnLineEditing()) {
        return controller.ForwardActionToShell(kpAction);
    }
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

    // Shell mode: scrollback + grid history above, cursor row composed with the
    // user's inputLine at the bottom (keeps prompt and input on the same line).
    auto &scrollback = screen.GetScrollback();
    int cursorGridRow = screen.GetCursorPos().y;
    int promptLen     = screen.GetCursorPos().x;

    int totalHistory = (int)scrollback.size() + cursorGridRow;
    // When content is shorter than the view, anchor it to the bottom with blank
    // space at the top (standard terminal behaviour — content flows up from prompt).
    int startIdx  = std::max(0, totalHistory - (viewHeight - 1));
    int blankRows = std::max(0, (viewHeight - 1) - totalHistory);

    for (int viewY = 0; viewY < viewHeight - 1; viewY++) {
        dc.ClearLine(viewY);
        if (viewY < blankRows) {
            continue;
        }
        int idx = startIdx + (viewY - blankRows);
        if (idx < (int)scrollback.size()) {
            DrawScreenRow(dc, scrollback[idx], viewY);
        } else {
            int gridY = idx - (int)scrollback.size();
            if (gridY < cursorGridRow) {
                DrawScreenRow(dc, screen.GetRow(gridY), viewY);
            }
        }
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
