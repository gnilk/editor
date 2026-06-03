//
// Created by gnilk on 24.02.23.
//

#ifndef NCWIN_HSPLITVIEW_H
#define NCWIN_HSPLITVIEW_H

#include "logger.h"
#include "ViewBase.h"
#include "Core/RuntimeConfig.h"
#include "VisibleView.h"

namespace gedit {
    // Horizontal splitter
    class HSplitView : public VisibleView {
    public:
        HSplitView() = default;
        explicit HSplitView(const Rect &rect) : VisibleView(rect) {

        }

        void InitView() override {
            VisibleView::InitView();

            window->SetCaption("HSplitView");

            if (splitterPos == 0) {
                splitterPos = GetContentRect().Height()/2;
            }

            UpdateUpperViewRect();
            UpdateLowerViewRect();

            if (upperView) knownUpperHeight = upperView->GetHeight();
            if (lowerView) knownLowerHeight = lowerView->GetHeight();
        }

        void ReInitView() override {
            VisibleView::ReInitView();

            auto logger = gnilk::Logger::GetLogger("HSplitView");
            logger->Debug("ReInitView, Height=%d", viewRect.Height());

            if (splitterPos == 0) {
                splitterPos = GetContentRect().Height()/2;
            }

            if (upperView && knownUpperHeight > 0 && upperView->GetHeight() != knownUpperHeight) {
                splitterPos = upperView->GetHeight();
            } else if (lowerView && knownLowerHeight > 0 && lowerView->GetHeight() != knownLowerHeight) {
                splitterPos = GetContentRect().Height() - lowerView->GetHeight() - 1;
            }

            UpdateUpperViewRect();
            UpdateLowerViewRect();

            if (upperView) knownUpperHeight = upperView->GetHeight();
            if (lowerView) knownLowerHeight = lowerView->GetHeight();
        }

        void SetSplitterPos(int newSplitterPos) {
            auto logger = gnilk::Logger::GetLogger("HSplitView");

            // Clamp here - this is the single chokepoint every resize path goes through (the
            // increase/decrease actions, relative positioning, AdjustHeight). Without it the splitter
            // (which carries the status line, see HSplitViewStatus::DrawSplitter) can be pushed past
            // the bottom of the content area or above the top, leaving the status bar off-screen or
            // a view with negative height.
            newSplitterPos = ClampSplitterPos(newSplitterPos);
            logger->Debug("SetSplitterPos, newPos=%d, height=%d", newSplitterPos, GetViewRect().Height());

            splitterPos = newSplitterPos;
            UpdateUpperViewRect();
            UpdateLowerViewRect();
            Initialize();
            InvalidateAll();
        }

        int GetSplitterPos() {
            return splitterPos;
        }

        void SetSplitterPosRelative(float newSplitterPos) {
            SetSplitterPos(GetContentRect().Height() * newSplitterPos);
        }
        float GetSplitterPosRelative() {
            return splitterPos / (float)GetContentRect().Height();
        }

        void AdjustHeight(int deltaHeight) override {
            // SetSplitterPos clamps for us (see ClampSplitterPos).
            SetSplitterPos(GetSplitterPos() + deltaHeight);
        }

        // If we toggle this - we need a way to untoggle it as well...
        void MaximizeContentHeight() override {
            bUseFullView = true;
            if (upperView->IsActive()) {
                upperView->SetVisible(true);
                lowerView->SetVisible(false);
            } else {
                lowerView->SetVisible(true);
                upperView->SetVisible(false);
            }
            UpdateUpperViewRect();
            UpdateLowerViewRect();
            Initialize();
            InvalidateAll();
        }

        // This restore the content stuff to whatever it was before maximize
        void RestoreContentHeight() override {
            bUseFullView = bWasUseFullView;
            if (splitterPosBeforeReset > 0) {
                splitterPos = splitterPosBeforeReset;
            }
            if (bUseFullView) {
                MaximizeContentHeight();
                return;
            }
            upperView->SetVisible(true);
            lowerView->SetVisible(true);
            UpdateUpperViewRect();
            UpdateLowerViewRect();
            Initialize();
            InvalidateAll();
        }

        void ResetContentHeight() override {
            // Save these so we can restore...
            bWasUseFullView = bUseFullView;
            splitterPosBeforeReset = splitterPos;


            bUseFullView = false;
            upperView->SetVisible(true);
            lowerView->SetVisible(true);
            splitterPos = 0;        // This will cause default initialization behaviour
            Initialize();
            InvalidateAll();
        }


        void SetViewRect(const Rect &rect) override {
            viewRect = rect;

            if (splitterPos == 0) {
                splitterPos = GetContentRect().Height() / 2;
            }
            UpdateUpperViewRect();
            UpdateLowerViewRect();
        }


        void SetUpper(ViewBase *newUpper) {
            upperView = newUpper;
            upperView->SetLayoutHandler(this);
            AddView(upperView);
            UpdateUpperViewRect();
        }

        void SetLower(ViewBase *newLower) {
            lowerView = newLower;
            lowerView->SetLayoutHandler(this);
            AddView(lowerView);
            UpdateLowerViewRect();

        }
    protected:
        void DrawViewContents() override {
            DrawSplitter(GetSplitRow());
        }
        int GetSplitRow() {
            if (!bUseFullView) {
                return splitterPos;
            }
            auto &dc = window->GetContentDC();
            int splitRow =  dc.GetRect().Height()-1;
            if (splitRow < 0) {
                splitRow = 0;
            }
            return splitRow;
        }

        virtual void DrawSplitter(int row) {
            auto &dc = window->GetContentDC();

            auto logger = gnilk::Logger::GetLogger("HSplitView");
            logger->Debug("DrawRow, Row=%d, Height=%d", row, dc.GetRect().Height());

            std::string dummy(dc.GetRect().Width(), '-');
            dc.DrawStringWithAttributesAt(0,row, kTextAttributes::kInverted, dummy.c_str());

        }

        void UpdateUpperViewRect() {
            if (!upperView) return;

            // Computations are with screen-coordinates but we use content area (in screen-coords)
            // so we need to adjust the height relative to upper-left corner (the offset from screen border)
            // Since it will change depending on if the window has borders or not...
            Rect upperRect = GetContentRect();
            if (!bUseFullView) {
                upperRect.SetHeight(splitterPos - upperRect.TopLeft().y);
            } else {
                upperRect.SetHeight(upperRect.Height()-1);
            }

//            auto logger = gnilk::Logger::GetLogger("HSplitView");
//            logger->Debug("UpperRect: W=%d, H=%d (%d,%d)-(%d,%d)",
//                          upperRect.Width(), upperRect.Height(),
//                          upperRect.TopLeft().x, upperRect.TopLeft().y,
//                          upperRect.BottomRight().x,upperRect.BottomRight().y);

            upperView->SetViewRect(upperRect);
        }
        void UpdateLowerViewRect() {
            if (!lowerView) return;
            Rect lowerRect = GetContentRect();

            if (!bUseFullView) {
                // Splitter occupies the first line of the second part in the splitter view...
                // So we must offset everything by '1' in addition to offset by screen position (in case borders)
                lowerRect.SetHeight(lowerRect.Height() - splitterPos - 1 + lowerRect.TopLeft().y);
                lowerRect.Move(0, splitterPos + 1 - lowerRect.TopLeft().y);
            } else {
                lowerRect.SetHeight(lowerRect.Height()-1);
            }

            auto logger = gnilk::Logger::GetLogger("HSplitView");
            logger->Debug("SplitterPos: %d (ViewRect.Height = %d)", splitterPos, viewRect.Height());
            logger->Debug("LowerRect: W=%d, H=%d TL:(%d,%d) BR:(%d,%d)",
                          lowerRect.Width(), lowerRect.Height(),
                          lowerRect.TopLeft().x, lowerRect.TopLeft().y,
                          lowerRect.BottomRight().x,lowerRect.BottomRight().y);


            lowerView->SetViewRect(lowerRect);
        }
    protected:
        void OnViewInitialized() override {
            ViewBase::OnViewInitialized();
        }

        void OnActionIncreaseHeight() override {
            int delta = 0;
            if (lowerView->IsActive()) {
                // lower wants to increase..
                delta = -1;
            } else {
                delta = 1;
            }
            SetSplitterPos(GetSplitterPos() + delta);
            // Record the actually-applied (clamped) position so RestoreContentHeight can't reintroduce
            // an out-of-range splitter.
            splitterPosBeforeReset = GetSplitterPos();
        }
        void OnActionDecreaseHeight() override {
            int delta = 0;
            if (lowerView->IsActive()) {
                // lower wants to increase..
                delta = 1;
            } else {
                delta = -1;
            }
            SetSplitterPos(GetSplitterPos() + delta);
            splitterPosBeforeReset = GetSplitterPos();
        }

        // A horizontal splitter only manages HEIGHT. Width changes belong to an enclosing layout
        // (e.g. a VSplitView), so pass them up the layout-handler chain rather than letting them fall
        // through to ViewBase::SetWidth on the splitter itself (unbounded). Top-level => no-op.
        void OnActionIncreaseWidth() override {
            auto handler = GetLayoutHandler();
            if (handler != nullptr && handler != this) { handler->OnActionIncreaseWidth(); }
        }
        void OnActionDecreaseWidth() override {
            auto handler = GetLayoutHandler();
            if (handler != nullptr && handler != this) { handler->OnActionDecreaseWidth(); }
        }

        // Keep at least this many rows for each of the upper/lower views (and thus the splitter row
        // itself on-screen) whenever the content area is large enough to allow it.
        static constexpr int kMinViewExtent = 5;

        // Clamp a candidate splitter row into the visible content area, leaving room for both views.
        // Falls back to a strict on-screen clamp when the content is too small for the normal margin.
        int ClampSplitterPos(int pos) {
            int h = GetContentRect().Height();
            int lo = kMinViewExtent;
            int hi = h - kMinViewExtent;
            if (hi < lo) {
                lo = 0;
                hi = (h > 0) ? (h - 1) : 0;
            }
            if (pos < lo) { return lo; }
            if (pos > hi) { return hi; }
            return pos;
        }

    protected:
        int splitterPos = {};
        int splitterPosBeforeReset = -1;
        bool bWasUseFullView = false;
        bool bUseFullView = false;  // This affects the computation of full view semantics

        int knownUpperHeight = 0;
        int knownLowerHeight = 0;

        ViewBase *upperView = nullptr;
        ViewBase *lowerView = nullptr;
    };

}

#endif //NCWIN_HSPLITVIEW_H
