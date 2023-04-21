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
            dc.ResetDrawColors();
            dc.FillLine(0, kTextAttributes::kNormal | kTextAttributes::kInverted, ' ');
            dc.DrawStringWithAttributesAt(0, 0, kTextAttributes::kNormal | kTextAttributes::kInverted, heading.c_str());
        }
    private:
        std::string heading;
    };
}

#endif //EDITOR_SINGLELINEVIEW_H
