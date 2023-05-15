//
// Created by gnilk on 18.03.23.
//

#ifndef EDITOR_HSPLITVIEWSTATUS_H
#define EDITOR_HSPLITVIEWSTATUS_H
#include "HSplitView.h"
#include "Core/RuntimeConfig.h"

namespace gedit {
    class HSplitViewStatus : public HSplitView {
    public:
        HSplitViewStatus() = default;
        explicit HSplitViewStatus(const Rect &rect) : HSplitView(rect) {

        }
        virtual ~HSplitViewStatus() = default;

    protected:
        void DrawSplitter(int row) override {
            auto &dc = window->GetContentDC();

            dc.ResetDrawColors();

            auto logger = gnilk::Logger::GetLogger("HSplitViewStatus");
            logger->Debug("DrawSplitter, row=%d, height=%d", row, dc.GetRect().Height());

            auto model = RuntimeConfig::Instance().ActiveEditorModel();
            if (model == nullptr) {
                logger->Error("model is nullptr!");
                exit(1);
            }
            //model->cursor.position.x
            std::string dummy(dc.GetRect().Width(), ' ');
            std::string statusLine = " GoatEdit V0.1 | ";
            // Indicate whatever the editor is in edit or cmd state.
            if (Editor::Instance().GetState() == Editor::EditState) {
                statusLine += "E | ";
            } else {
                statusLine += "C | ";
            }

            statusLine += model->GetTextBuffer()->GetName();

            statusLine += " | ";
            statusLine += model->GetTextBuffer()->HaveLanguage()?model->GetTextBuffer()->LangParser().Identifier():"none";

            dc.FillLine(row, kTextAttributes::kInverted, ' ');
            dc.DrawStringWithAttributesAt(0,row, kTextAttributes::kInverted, statusLine.c_str());

            statusLine = "";
            char tmp[32];
            snprintf(tmp,32, "Ln: %d, Col: %d", model->cursor.position.y, model->cursor.position.x);
            statusLine += tmp;

            dc.DrawStringWithAttributesAt(dc.GetRect().Width()-statusLine.size()-4,row, kTextAttributes::kInverted, statusLine.c_str());
        }
    };
}

#endif //EDITOR_HSPLITVIEWSTATUS_H
