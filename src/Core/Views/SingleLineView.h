//
// Created by gnilk on 16.03.23.
//

#ifndef EDITOR_SINGLELINEVIEW_H
#define EDITOR_SINGLELINEVIEW_H

#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"
#include "VisibleView.h"


namespace gedit {
    class SingleLineView : public VisibleView {
    public:
        SingleLineView() = default;
        void InitView() override {
            viewRect.SetHeight(1);
            VisibleView::InitView();
        }

        void ReInitView() override {
            viewRect.SetHeight(1);
            VisibleView::ReInitView();
        }

        void SetText(const std::string &newHeading) {
            heading = newHeading;
        }
        const std::string &GetText() {
            return heading;
        }
    protected:
        void DrawViewContents() override {
            auto &dc = window->GetContentDC();
            auto theme = Editor::Instance().GetTheme();
            auto &uiColors = theme->GetUIColors();

            if (parentView->IsActive()) {
                dc.SetColor(uiColors["header_active_foreground"], uiColors["header_active_background"]);
            } else {
                dc.SetColor(uiColors["header_foreground"], uiColors["header_background"]);
            }
            dc.FillLine(0, kTextAttributes::kNormal, ' ');
            dc.DrawStringWithAttributesAt(0, 0, kTextAttributes::kNormal, heading.c_str());
        }
    private:
        std::string heading;
    };
}

#endif //EDITOR_SINGLELINEVIEW_H
