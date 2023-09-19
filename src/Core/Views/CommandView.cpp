//
// Created by gnilk on 15.02.23.
//

#include "Core/RuntimeConfig.h"
#include "CommandView.h"
#include "Core/Views/HSplitView.h"
#include "Core/Config/Config.h"
#include "Core/Editor.h"
#include "Core/LineRender.h"
using namespace gedit;

static const std::string cfgSectionName = "commandview";


void CommandView::InitView() {
    // We own the cursor, so we need to reset it on new lines...
    logger = gnilk::Logger::GetLogger("CommandView");
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

    prompt = Config::Instance()[cfgSectionName].GetStr("prompt","gedit>");

    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    window->SetCaption("CommandView");
    commandController.SetNewLineNotificationHandler([this]()->void {
        OnNewLineNotification();
    });
    commandController.Begin();

    RuntimeConfig::Instance().SetOutputConsole(this);
}

void CommandView::ReInitView() {
    logger->Debug("ReInitialize View!");
    auto screen = RuntimeConfig::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        logger->Debug("View Rect is empty, initalizing to screen dimensions");
        viewRect = screen->Dimensions();
    }
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    prompt = Config::Instance()[cfgSectionName].GetStr("prompt","gedit>");
    cursor.position.x = prompt.size();
}

void CommandView::OnActivate(bool isActive) {
    logger->Debug("OnActive, isActive: %s", isActive?"yes":"no");
    if (!isActive) {
        // restore the content height
        parentView->RestoreContentHeight();
    } else {
        // Reset content height to 50/50 when we become active..
        parentView->ResetContentHeight();

        // Set the keymap for this view or default if not found...
        Editor::Instance().SetActiveKeyMapping(Config::Instance()[cfgSectionName].GetStr("keymap", "default_keymap"));
    }
}

bool CommandView::OnAction(const KeyPressAction &kpAction) {
    switch(kpAction.action) {
        case kAction::kActionCommitLine :
            return OnActionCommitLine();
        default:
            break;
    }
    return false;
}

bool CommandView::OnActionCommitLine() {
    commandController.CommitLine();
    return true;
}

void CommandView::OnNewLineNotification() {
    cursor.position.x = prompt.size();
    if (IsActive()) {

        // Let this be handled by the main thread...
        PostMessage([this]()->void {
            auto &dc = window->GetContentDC();
            auto &lines = commandController.Lines();
            // Only adjust height if the amount of text exceeds the height...
            if ((int)lines.size() > dc.GetRect().Height() - 1) {
                parentView->AdjustHeight(-1);
            }
            InvalidateView();
        });
    }
}

void CommandView::OnKeyPress(const KeyPress &keyPress) {
    auto strCursor = cursor;
    size_t dummyLineIndex = 0;  // Need this as the HandleKeyPress takes a reference
    strCursor.position.x -= prompt.size();
    if (commandController.HandleKeyPress(strCursor, dummyLineIndex, keyPress)) {
        cursor = strCursor;
        cursor.position.x += prompt.size();
        return;
    }

    // Must call base class - perhaps this is a stupid thing..
    ViewBase::OnKeyPress(keyPress);
}

void CommandView::DrawViewContents() {
    auto &dc = window->GetContentDC();
    dc.ResetDrawColors();

    auto &lines = commandController.Lines();

    int lOffset = 0;
    if ((int)lines.size() > (dc.GetRect().Height())) {
        lOffset = lines.size() - (int)(dc.GetRect().Height()-1);
    }

    LineRender lineRender(dc);

    cursor.position.y = 0;
    // Never print on the last line - we reserve that for input..
    for(int i=0;i<(dc.GetRect().Height() - 1);i++) {
        if ((i + lOffset) >= (int)lines.size()) {
            break;
        }
        dc.ClearLine(i);

        lineRender.DrawLine(0,i,lines[i+lOffset]);
        cursor.position.y += 1;
    }
    if ((int)lines.size() > dc.GetRect().Height()-1) {
        cursor.position.y = dc.GetRect().Height()-1;
    }
    auto currentLine = commandController.CurrentLine();
    auto prompt = Config::Instance()[cfgSectionName].GetStr("prompt","gedit>");
    dc.DrawStringAt(0, cursor.position.y, prompt.c_str());
    dc.DrawStringAt(prompt.size(), cursor.position.y, currentLine->Buffer().data());

    logger->Debug("DrawViewContents, cursor at: %d,%d", cursor.position.x, cursor.position.y);

}

// impl of IOutputConsole
void CommandView::WriteLine(const std::u32string &str) {
    commandController.WriteLine(str);
    InvalidateView();
}
