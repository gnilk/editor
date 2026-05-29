//
// Created by gnilk on 24.02.23.
//

#ifndef NCWIN_VSPLITVIEW_H
#define NCWIN_VSPLITVIEW_H

#include "ViewBase.h"
#include "Core/RuntimeConfig.h"

namespace gedit {
    // Vertical splitter — left | right panels separated by a vertical divider
    class VSplitView : public ViewBase {
    public:
        VSplitView() = default;
        explicit VSplitView(const Rect &rect) : ViewBase(rect) {}
        virtual ~VSplitView() = default;

        void InitView() override {
            auto screen = RuntimeConfig::Instance().GetScreen();
            if (viewRect.IsEmpty()) {
                viewRect = screen->Dimensions();
            }
            if (splitterPos == 0) {
                splitterPos = viewRect.Width() / 2;
            }
            window = screen->CreateWindow(viewRect, WindowBase::kWin_Invisible, WindowBase::kWinDeco_None);
            UpdateLeftViewRect();
            UpdateRightViewRect();
            if (leftView != nullptr) { knownLeftWidth = leftView->GetWidth(); }
            if (rightView != nullptr) { knownRightWidth = rightView->GetWidth(); }
        }

        void ReInitView() override {
            auto screen = RuntimeConfig::Instance().GetScreen();
            if (viewRect.IsEmpty()) {
                viewRect = screen->Dimensions();
            }
            window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Invisible, WindowBase::kWinDeco_None);

            if (splitterPos == 0) {
                splitterPos = viewRect.Width() / 2;
            }

            if (leftView != nullptr && knownLeftWidth > 0 && leftView->GetWidth() != knownLeftWidth) {
                splitterPos = leftView->GetWidth();
            } else if (rightView != nullptr && knownRightWidth > 0 && rightView->GetWidth() != knownRightWidth) {
                splitterPos = viewRect.Width() - rightView->GetWidth();
            }

            UpdateLeftViewRect();
            UpdateRightViewRect();

            if (leftView != nullptr) { knownLeftWidth = leftView->GetWidth(); }
            if (rightView != nullptr) { knownRightWidth = rightView->GetWidth(); }
        }

        void SetSplitterPos(int newSplitterPos) {
            splitterPos = newSplitterPos;
            UpdateLeftViewRect();
            UpdateRightViewRect();
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

        void SetViewRect(const Rect &rect) override {
            viewRect = rect;
            if (splitterPos == 0) {
                splitterPos = viewRect.Width() / 2;
            }
            UpdateLeftViewRect();
            UpdateRightViewRect();
        }

        void SetInitialSplitterPos(int pos) {
            splitterPos = pos;
        }

        void SetLeft(ViewBase *newLeft) {
            leftView = newLeft;
            leftView->SetLayoutHandler(this);
            AddView(newLeft);
            UpdateLeftViewRect();
        }

        void SetRight(ViewBase *newRight) {
            rightView = newRight;
            rightView->SetLayoutHandler(this);
            AddView(newRight);
            UpdateRightViewRect();
        }

    protected:
        void OnActionIncreaseWidth() override {
            int delta = (rightView != nullptr && rightView->IsActive()) ? -1 : 1;
            int pos = GetSplitterPos() + delta;
            splitterPosBeforeReset = pos;
            SetSplitterPos(pos);
        }
        void OnActionDecreaseWidth() override {
            int delta = (rightView != nullptr && rightView->IsActive()) ? 1 : -1;
            int pos = GetSplitterPos() + delta;
            splitterPosBeforeReset = pos;
            SetSplitterPos(pos);
        }

        void UpdateRightViewRect() {
            if (rightView == nullptr) { return; }
            Rect rightRect = viewRect;
            rightRect.SetWidth(viewRect.Width() - splitterPos);
            rightRect.Move(splitterPos, 0);
            rightView->SetViewRect(rightRect);
        }
        void UpdateLeftViewRect() {
            if (leftView == nullptr) { return; }
            Rect leftRect = viewRect;
            leftRect.SetWidth(splitterPos);
            leftView->SetViewRect(leftRect);
        }

    protected:
        int splitterPos = 0;
        int splitterPosBeforeReset = -1;
        int knownLeftWidth = 0;
        int knownRightWidth = 0;
        ViewBase *leftView = nullptr;
        ViewBase *rightView = nullptr;
    };
}

#endif //NCWIN_VSPLITVIEW_H
