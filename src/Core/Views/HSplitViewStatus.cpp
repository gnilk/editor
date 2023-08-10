//
// Created by gnilk on 09.08.23.
//

#include "Core/Editor.h"

#include "HSplitViewStatus.h"

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


    //model->cursor.position.x
    std::string dummy(dc.GetRect().Width(), ' ');
    std::string statusLine = " " + Editor::Instance().GetAppName() + " v" + Editor::Instance().GetVersion() + " | ";
    // Indicate whatever the editor is in edit or cmd state.
    if (Editor::Instance().GetState() == Editor::ViewState) {
        statusLine += "E | ";
    } else {

        auto &quickController = Editor::Instance().GetQuickCommandController();
        quickModeStatusLineCursorOfs = (int)statusLine.length();

        statusLine += quickController.GetPrompt();
        auto currentCmdLine = quickController.GetCmdLine();
        statusLine += currentCmdLine;
        statusLine += " | ";
    }

    // If we have a model - draw details...
    auto model = Editor::Instance().GetActiveModel();
    if (model != nullptr) {
        if (model->GetTextBuffer()->GetBufferState() == TextBuffer::kBuffer_Changed) {
            statusLine += "* ";
        }

        statusLine += model->GetTextBuffer()->GetName();
        statusLine += " | ";
        if (!model->GetTextBuffer()->CanEdit()) {
            statusLine += "[locked] | ";
        }
        statusLine += model->GetTextBuffer()->HaveLanguage() ? model->GetTextBuffer()->GetLanguage().Identifier()
                                                             : "none";
    }

    dc.FillLine(row, kTextAttributes::kInverted, ' ');
    dc.DrawStringWithAttributesAt(0,row, kTextAttributes::kInverted, statusLine.c_str());

    // Draw right-centered details about cursor and active line...
    if (model) {
        auto activeLine = model->GetTextBuffer()->LineAt(model->idxActiveLine);
        statusLine = "";
        char tmp[32];
        snprintf(tmp, 32, "Id: %d, Ln: %d, Col: %d", activeLine->Indent(), model->cursor.position.y,
                 model->cursor.position.x);
        statusLine += tmp;

        dc.DrawStringWithAttributesAt(dc.GetRect().Width() - statusLine.size() - 4, row, kTextAttributes::kInverted,
                                      statusLine.c_str());
    }
}
