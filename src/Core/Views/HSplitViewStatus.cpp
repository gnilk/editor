//
// Created by gnilk on 09.08.23.
//

#include "Core/Editor.h"

#include "HSplitViewStatus.h"
#include "RootView.h"

using namespace gedit;

void HSplitViewStatus::SetWindowCursor(const Cursor &cursor) {
    auto &quickController = Editor::Instance().GetQuickCommandController();
    auto &newCursor = quickController.GetCursor();
    // Need to reposition the cursor properly...
    auto logger = gnilk::Logger::GetLogger("HSplitViewStatus");
    logger->Debug("SetWindowCursor, xpos=%d", newCursor.position.x);

    int szPrompt = (int)quickController.GetPrompt().length();

    Cursor dummy = newCursor;
    dummy.position.y = GetSplitRow();
    dummy.position.x = quickModeStatusLineCursorOfs + szPrompt + newCursor.position.x;
    window->SetCursor(dummy);
}

void HSplitViewStatus::DrawSplitter(int row) {
    auto &dc = window->GetContentDC();

    dc.ResetDrawColors();

    auto logger = gnilk::Logger::GetLogger("HSplitViewStatus");
    logger->Debug("DrawSplitter, row=%d, height=%d", row, dc.GetRect().Height());

    std::u32string viewStatusCenter = {};
    std::u32string viewStatusRight = {};

    //model->cursor.position.x
    std::u32string dummy(dc.GetRect().Width(), U' ');
    std::u32string statusLine = U" " + Editor::Instance().GetAppName() + U" v" + Editor::Instance().GetVersion() + U" | ";
    // Indicate whatever the editor is in edit or cmd state.
    if (Editor::Instance().GetState() == Editor::QuickCommandState) {
        auto &quickController = Editor::Instance().GetQuickCommandController();
        quickModeStatusLineCursorOfs = (int) statusLine.length();

        statusLine += quickController.GetPrompt();
        auto currentCmdLine = quickController.GetCmdLine();
        statusLine += currentCmdLine;
        statusLine += U" | ";
    }  else {
        std::tie(viewStatusCenter, viewStatusRight) = GetStatusLineForTopview();
        statusLine += viewStatusCenter;
    }

    dc.FillLine(row, kTextAttributes::kInverted, ' ');
    dc.DrawStringWithAttributesAt(0,row, kTextAttributes::kInverted, statusLine.c_str());


    if (!viewStatusRight.empty()) {
        dc.DrawStringWithAttributesAt(dc.GetRect().Width() - viewStatusRight.size() - 4, row,
                                      kTextAttributes::kInverted,
                                      viewStatusRight.c_str());
    }
}

std::pair<std::u32string,std::u32string> HSplitViewStatus::GetStatusLineForTopview() {
    std::u32string mainStatus = U"";
    std::u32string rightStatus = U"";

    // This can be cleaned up - too convoluted...
    auto rootView = RuntimeConfig::Instance().GetRootViewAs<RootView>();
    if (rootView != nullptr) {
        auto view = rootView->TopView();
        if (view != nullptr) {
            mainStatus += view->GetStatusBarAbbreviation() + U" | ";
            std::u32string strCenter;
            std::tie(strCenter, rightStatus) = view->GetStatusBarInfo();
            mainStatus += strCenter;
        } else {
            mainStatus += U"E | ";
        }
    } else {
        mainStatus += U"E | ";
    }
    return {mainStatus, rightStatus};
}
