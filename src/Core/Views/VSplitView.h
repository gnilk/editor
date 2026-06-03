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
            // Clamp at the single chokepoint every resize path uses (increase/decrease-width actions,
            // relative positioning). Without it the divider can be dragged off either edge, giving the
            // left or right panel a negative width.
            splitterPos = ClampSplitterPos(newSplitterPos);
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
            SetSplitterPos(GetSplitterPos() + delta);
            splitterPosBeforeReset = GetSplitterPos();      // record the clamped value
        }
        void OnActionDecreaseWidth() override {
            int delta = (rightView != nullptr && rightView->IsActive()) ? 1 : -1;
            SetSplitterPos(GetSplitterPos() + delta);
            splitterPosBeforeReset = GetSplitterPos();
        }

        // A vertical splitter only manages WIDTH. Height changes belong to an enclosing layout
        // (e.g. an HSplitView), so pass them up the layout-handler chain until something that owns
        // height applies (and clamps) them. Without this the action falls through to
        // ViewBase::SetHeight on the splitter itself - unbounded, and it never reaches the HSplitView
        // that actually moves the divide, so the status bar could be driven off-screen.
        void OnActionIncreaseHeight() override {
            auto handler = GetLayoutHandler();
            if (handler != nullptr && handler != this) { handler->OnActionIncreaseHeight(); }
        }
        void OnActionDecreaseHeight() override {
            auto handler = GetLayoutHandler();
            if (handler != nullptr && handler != this) { handler->OnActionDecreaseHeight(); }
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

        // Keep at least this many columns for each of the left/right panels (and the divider) when the
        // view is wide enough to allow it.
        static constexpr int kMinViewExtent = 5;

        // Clamp a candidate divider column into the visible width, leaving room for both panels.
        // Falls back to a strict on-screen clamp when the view is too narrow for the normal margin.
        int ClampSplitterPos(int pos) {
            int w = viewRect.Width();
            int lo = kMinViewExtent;
            int hi = w - kMinViewExtent;
            if (hi < lo) {
                lo = 0;
                hi = (w > 0) ? (w - 1) : 0;
            }
            if (pos < lo) { return lo; }
            if (pos > hi) { return hi; }
            return pos;
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
