//
// Created by gnilk on 24.02.23.
//

#ifndef NCWIN_VSPLITVIEW_H
#define NCWIN_VSPLITVIEW_H

#include "ViewBase.h"
#include "Core/RuntimeConfig.h"

namespace gedit {
    // Vertical splitter
    class VSplitView : public ViewBase {
    public:
        VSplitView() = default;
        explicit VSplitView(const Rect &rect) : ViewBase(rect) {

        }
        void InitView() override {
            auto screen = RuntimeConfig::Instance().GetScreen();
            if (viewRect.IsEmpty()) {
                viewRect = screen->Dimensions();
            }
            // Do we have position yet??
            if (splitterPos == 0) {
                splitterPos = viewRect.Width() / 2;
            }
            window = screen->CreateWindow(viewRect, WindowBase::kWin_Invisible, WindowBase::kWinDeco_None);
            UpdateLeftViewRect();
            UpdateRightViewRect();
        }

        void SetSplitterPos(int newSplitterPos) {
            splitterPos = newSplitterPos;
            UpdateRightViewRect();
            UpdateLeftViewRect();
            Initialize();
            InvalidateAll();
        }
        int GetSplitterPos() {
            return splitterPos;
        }
        void SetSplitterPosRelative(float newSplitterPos) {
            SetSplitterPos(viewRect.Width() * newSplitterPos);
        }
        float GetSplitterPosRelative() {
            return splitterPos / (float)viewRect.Width();
        }

        // FIXME: make sure the splitter pos fits within the rect..
        void SetViewRect(const Rect &rect) override {
            viewRect = rect;
            if (splitterPos == 0) {
                splitterPos = viewRect.Width() / 2;
            }
            UpdateRightViewRect();
            UpdateLeftViewRect();
        }

        void SetLeft(ViewBase *newLeft) {
            leftView = newLeft;
            AddView(newLeft);
            UpdateLeftViewRect();
        }

        void SetRight(ViewBase *newRight) {
            rightView = newRight;
            AddView(newRight);
            UpdateRightViewRect();
        }
    protected:
        void UpdateRightViewRect() {
            if (!rightView) return;
            Rect rightRect = viewRect;

            rightRect.SetWidth(viewRect.Width() - splitterPos);
            rightRect.Move(splitterPos,0);

            rightView->SetViewRect(rightRect);
        }
        void UpdateLeftViewRect() {
            if (!leftView) return;
            Rect leftRect = viewRect;
            leftRect.SetWidth(splitterPos);
            leftView->SetViewRect(leftRect);
        }

    protected:
        int splitterPos = 0;
        ViewBase *leftView = nullptr;
        ViewBase *rightView = nullptr;
    };
}

#endif //NCWIN_VSPLITVIEW_H
