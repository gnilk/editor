//
// Created by gnilk on 22.02.2024.
//

#include "TerminalView.h"
#include "Core/Editor.h"
#include "Core/LineRender.h"

using namespace gedit;

static const std::string cfgSectionName = "commandview";


void TerminalView::InitView() {
    // We own the cursor, so we need to reset it on new lines...
    logger = gnilk::Logger::GetLogger("TerminalView");
    auto screen = RuntimeConfig::Instance().GetScreen();
    logger->Debug("InitView!");
    if (viewRect.IsEmpty()) {
        logger->Debug("View Rect is empty, initalizing to screen dimensions");
        viewRect = screen->Dimensions();
    }
    logger->Debug("ViewRect, TL:(%d:%d) BR:(%d:%d) Dim:(%d:%d)",
                  viewRect.TopLeft().x, viewRect.TopLeft().y,
                  viewRect.BottomRight().x, viewRect.BottomRight().y,
                  viewRect.Width(), viewRect.Height());

    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    window->SetCaption("Terminal");
//    commandController.SetNewLineNotificationHandler([this]()->void {
//        OnNewLineNotification();
//    });
//    commandController.Begin();

    controller.Begin();
    RuntimeConfig::Instance().SetOutputConsole(this);
}

void TerminalView::ReInitView() {
    logger->Debug("ReInitialize View!");
    auto screen = RuntimeConfig::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        logger->Debug("View Rect is empty, initalizing to screen dimensions");
        viewRect = screen->Dimensions();
    }
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
}

void TerminalView::OnActivate(bool isActive) {
    logger->Debug("OnActive, isActive: %s", isActive?"yes":"no");
    if (!isActive) {
        // restore the content height
        parentView->RestoreContentHeight();
    } else {
        // Reset content height to 50/50 when we become active..
        // parentView->ResetContentHeight();
        parentView->RestoreContentHeight();

        // Set the keymap for this view or default if not found...
        Editor::Instance().SetActiveKeyMapping(Config::Instance()[cfgSectionName].GetStr("keymap", "default_keymap"));
    }
}


void TerminalView::OnKeyPress(const KeyPress &keyPress) {
    logger->Debug("OnKeyPress");

    // FIXME: after the CMD+] we sometimes get 'text_event' with ' which is wrong
    //        must revist the keyboard handler...
    if (keyPress.key > 0x2000) {
        return;
    }

    auto strCursor = cursor;
    size_t dummyLineIndex = 0;  // Need this as the HandleKeyPress takes a reference
    if (controller.HandleKeyPress(strCursor, dummyLineIndex, keyPress)) {
        cursor = strCursor;
        return;
    }

    ViewBase::OnKeyPress(keyPress);
}

bool TerminalView::OnAction(const KeyPressAction &kpAction) {
    switch(kpAction.action) {
        case kAction::kActionCommitLine :
            return OnActionCommitLine();
        default:
            break;
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

    LineRender lineRender(dc);
    auto &lines = controller.Lines();

    int line_offset = 0;
    if (lines.size() > dc.GetRect().Height()) {
        line_offset = lines.size() - dc.GetRect().Height()-1;
    }

    int line_ypos = 0;
    if (!lines.empty()) {
        for (int i = 0; i < (dc.GetRect().Height() - 1); i++) {
            if ((i + line_offset) >= lines.size()) {
                break;
            }
            dc.ClearLine(line_ypos);
            lineRender.DrawLine(0, line_ypos, lines[i + line_offset]);
            line_ypos++;
        }
    }

    auto currentLine = controller.CurrentLine();
    dc.ClearLine(line_ypos);

    //lineRender.DrawLine(0,line_ypos, currentLine);
    static auto colRED = ColorRGBA::FromRGB(255,0,0);
    dc.SetFGColor(colRED);
    dc.DrawStringAt(0,line_ypos, currentLine->Buffer());

    cursor.position.y = line_ypos;
    // FIXME: THIS IS WRONG!
    cursor.position.x = currentLine->Buffer().length();

}


// old shell
void TerminalView::WriteLine(const std::u32string &str) {

}
