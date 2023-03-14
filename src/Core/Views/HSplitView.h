//
// Created by gnilk on 24.02.23.
//

#ifndef NCWIN_HSPLITVIEW_H
#define NCWIN_HSPLITVIEW_H

#include "logger.h"
#include "ViewBase.h"
#include "Core/RuntimeConfig.h"

namespace gedit {
    // Horizontal splitter
    class HSplitView : public ViewBase {
    public:
        HSplitView() = default;
        explicit HSplitView(const Rect &rect) : ViewBase(rect) {

        }

        void InitView() override {
            auto screen = RuntimeConfig::Instance().Screen();
            if (viewRect.IsEmpty()) {
                viewRect = screen->Dimensions();
            }

            window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
//            window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, (WindowBase::kWinDecoration)(WindowBase::kWinDeco_Border | WindowBase::kWinDeco_DrawCaption));
            window->SetCaption("HSplitView");

            if (splitterPos == 0) {
                splitterPos = GetContentRect().Height()/2;
            }
//            auto logger = gnilk::Logger::GetLogger("HSplitView");
//            logger->Debug("InitView, splitterPos=%d", splitterPos);

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
            auto yPos = GetViewRect().TopLeft().y;
            if (current < 1) {
                current = 1;
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
        void RestoreContentHeight() override {
            bUseFullView = false;
            upperView->SetVisible(true);
            lowerView->SetVisible(true);
            UpdateUpperViewRect();
            UpdateLowerViewRect();
            Initialize();
            InvalidateAll();
        }


            void SetViewRect(const Rect &rect) override {
            viewRect = rect;
            // FIXME: make sure the splitter pos fits within the rect..
            if (splitterPos == 0) {
                splitterPos = GetContentRect().Height() / 2;
            }
            UpdateUpperViewRect();
            UpdateLowerViewRect();
        }


        void SetUpper(ViewBase *newUpper) {
            upperView = newUpper;
            AddView(upperView);
            UpdateUpperViewRect();
        }

        void SetLower(ViewBase *newLower) {
            lowerView = newLower;
            AddView(newLower);
            UpdateLowerViewRect();
        }
    protected:
        void DrawViewContents() override {
            if (!bUseFullView) {
                auto &dc = window->GetWindowDC();
                std::string dummy(viewRect.Width(), '*');
                dc.DrawStringAt(0, splitterPos, dummy.c_str());
            }
        }

        void UpdateUpperViewRect() {
            if (!upperView) return;

            // Computations are with screen-coordinates but we use content area (in screen-coords)
            // so we need to adjust the height relative to upper-left corner (the offset from screen border)
            // Since it will change depending on if the window has borders or not...
            Rect upperRect = GetContentRect();
            if (!bUseFullView) {
                upperRect.SetHeight(splitterPos - upperRect.TopLeft().y);
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
            }

            auto logger = gnilk::Logger::GetLogger("HSplitView");
            logger->Debug("LowerRect: W=%d, H=%d", lowerRect.Width(), lowerRect.Height());

            lowerView->SetViewRect(lowerRect);
        }
    protected:
        void OnViewInitialized() override {
            ViewBase::OnViewInitialized();
        }

    protected:
        int splitterPos;
        bool bUseFullView = false;  // This affects the computation of full view semantics

        ViewBase *upperView = nullptr;
        ViewBase *lowerView = nullptr;
    };

}

#endif //NCWIN_HSPLITVIEW_H
