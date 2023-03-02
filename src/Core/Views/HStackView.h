//
// Created by gnilk on 24.02.23.
//

#ifndef NCWIN_HSTACKVIEW_H
#define NCWIN_HSTACKVIEW_H

#include <vector>

#include "Core/RuntimeConfig.h"
#include "ViewBase.h"

namespace gedit {
    class HStackView : public ViewBase {
    public:
        typedef enum : int32_t {
            kFill = 1,
            kFixed = 2,
        } kLayout;
        struct StackableView {
            kLayout layout = kFill;
            ViewBase *view = nullptr;
        } ;
    public:
        HStackView() = default;
        explicit HStackView(const Rect &rect) : ViewBase (rect) {

        }
        virtual ~HStackView() = default;
        void InitView() override {
            auto screen = RuntimeConfig::Instance().Screen();
            if (viewRect.IsEmpty()) {
                viewRect = screen->Dimensions();
            }
            window = screen->CreateWindow(viewRect, WindowBase::kWin_Invisible, WindowBase::kWinDeco_None);
            //window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, (WindowBase::kWinDecoration)(WindowBase::kWinDeco_Border | WindowBase::kWinDeco_DrawCaption));
            //window->SetCaption("HStackView");
            RecomputeLayout();
        }

        void AddSubView(ViewBase *view, kLayout layout) {
            StackableView stackView = {.layout = layout, .view = view};
            viewStack.push_back(stackView);
            AddView(view);
            RecomputeLayout();
        }
    protected:
        // Recompute the layout - we stack items horizontally (i.e. along the X-axis)
        // This is VERY simple
        // 1) Calculate how much all fixed items need
        // 2) Distribute the rest evenly across the 'fill' (dynamic) items
        // Enforce the height to be the full height of the view...
        void RecomputeLayout() {
            if (window == nullptr) {
                return;
            }

            // 1) Calculate amount occupied by fixed width items
            int fixedWidth = 0;
            int nFixed = 0;
            for(auto &view : viewStack) {
                if (view.layout == kFixed) {
                    fixedWidth += view.view->GetViewRect().Width();
                    nFixed++;
                }
            }
            // 2) Distribute the rest amongst the flexible width items..
            int flexWidth = 0;
            if ((viewStack.size() - nFixed) > 0) {
                flexWidth = (GetContentRect().Width() - fixedWidth) / (viewStack.size() - nFixed);
            }

            // All subwindows are full height (enforced)
            int xPos = GetContentRect().TopLeft().x;
            int yPos = GetContentRect().TopLeft().y;

            for(auto &view : viewStack) {
                auto viewRect = view.view->GetViewRect();
                viewRect.MoveTo(xPos,yPos);
                // Set the flex-width, otherwise keep the width (assuming the user has set its width properly)
                if (view.layout != kFixed) {
                    viewRect.SetWidth(flexWidth);
                }
                // Enforce height of stacked views..
                viewRect.SetHeight(GetContentRect().Height());
                view.view->SetViewRect(viewRect);
                xPos += viewRect.Width();
            }
        }
    protected:
        std::vector<StackableView> viewStack;
    };
}

#endif //NCWIN_HSTACKVIEW_H
