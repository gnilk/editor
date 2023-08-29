//
// Created by gnilk on 18.03.23.
//

#ifndef EDITOR_EDITORHEADERVIEW_H
#define EDITOR_EDITORHEADERVIEW_H

#include "SingleLineView.h"

namespace gedit {
    class EditorHeaderView : public SingleLineView {
    public:
        EditorHeaderView() = default;
        virtual ~EditorHeaderView() = default;
    protected:
        void DrawViewContents() override {
            auto &dc = window->GetContentDC();

            dc.ResetDrawColors();

            auto workspace = Editor::Instance().GetWorkspace();
            auto &models = Editor::Instance().GetModels();
            auto theme = Editor::Instance().GetTheme();
            auto uiColors = theme->GetUIColors();
            if (parentView->IsActive()) {
                dc.SetColor(uiColors["header_active_foreground"], uiColors["header_active_background"]);
            } else {
                dc.SetColor(uiColors["header_foreground"], uiColors["header_background"]);
            }

            dc.FillLine(0, kTextAttributes::kNormal, ' ');

            int xp = 0;

            std::string header = "[Files]";
            dc.DrawStringWithAttributesAt(xp,0,kTextAttributes::kNormal, header.c_str());

            xp += header.length();
            dc.DrawStringWithAttributesAt(xp,0,kTextAttributes::kNormal, " ");
            xp++;
            for(size_t i=0;i<models.size();i++) {
                auto model = models[i];
                auto node = workspace->GetNodeFromModel(models[i]);
                if (node == nullptr) {
                    continue;
                }
                auto &name = node->GetDisplayName();
                header = name;

                // Add marker for changed
                if (model->GetTextBuffer()->GetBufferState() == TextBuffer::kBuffer_Changed) {
                    dc.DrawStringWithAttributesAt(xp,0,kTextAttributes::kNormal, "* ");
                    xp += 2;
                }
                if (model->GetTextBuffer()->IsReadOnly()) {
                    dc.DrawStringWithAttributesAt(xp,0,kTextAttributes::kNormal, "R ");
                    xp += 2;
                }

                if (model->IsActive()) {
                    dc.DrawStringWithAttributesAt(xp,0,kTextAttributes::kUnderline, header.c_str());
                } else {
                    dc.DrawStringWithAttributesAt(xp,0,kTextAttributes::kNormal, header.c_str());
                }
                xp += header.length();
                if (i < (models.size()-1)) {
                    header = " | ";
                    dc.DrawStringWithAttributesAt(xp, 0, kTextAttributes::kNormal, header.c_str());
                    xp += header.length();
                } else {
                    header = " ";
                    dc.DrawStringWithAttributesAt(xp, 0, kTextAttributes::kNormal, header.c_str());
                    xp += header.length();
                }
            }
            //dc.DrawStringWithAttributesAt(0,0,kTextAttributes::kUnderline, header.c_str());
        }
    };
}


#endif //EDITOR_EDITORHEADERVIEW_H
