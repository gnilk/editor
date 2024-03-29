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

            UpdateLowerViewRect();
            UpdateUpperViewRect();
        }

        void ReInitView() override {
            VisibleView::ReInitView();

            auto logger = gnilk::Logger::GetLogger("HSplitView");
            logger->Debug("ReInitView, Height=%d", viewRect.Height());

            if (splitterPos == 0) {
                splitterPos = GetContentRect().Height()/2;
            }

            UpdateLowerViewRect();
            UpdateUpperViewRect();
        }

        void SetSplitterPos(int newSplitterPos) {
            auto logger = gnilk::Logger::GetLogger("HSplitView");
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
            auto current = GetSplitterPos();
            current += deltaHeight;
            if (current < 5) {
                current = 5;
            } else if (current > (GetContentRect().Height()-5)) {
                current = GetContentRect().Height()-5;
            }
            SetSplitterPos(current);
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
            auto pos = GetSplitterPos();
            pos += delta;
            splitterPosBeforeReset = pos;
            SetSplitterPos(pos);
        }
        void OnActionDecreaseHight() override {
            int delta = 0;
            if (lowerView->IsActive()) {
                // lower wants to increase..
                delta = 1;
            } else {
                delta = -1;
            }
            auto pos = GetSplitterPos();
            pos += delta;
            splitterPosBeforeReset = pos;
            SetSplitterPos(pos);
        }

    protected:
        int splitterPos = {};
        int splitterPosBeforeReset = -1;
        bool bWasUseFullView = false;
        bool bUseFullView = false;  // This affects the computation of full view semantics

        ViewBase *upperView = nullptr;
        ViewBase *lowerView = nullptr;
    };

}

#endif //NCWIN_HSPLITVIEW_H
