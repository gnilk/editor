//
// Created by gnilk on 16.03.23.
//

#ifndef EDITOR_VSTACKVIEW_H
#define EDITOR_VSTACKVIEW_H

#include "Core/RuntimeConfig.h"
#include "ViewBase.h"
#include "StackableView.h"
#include <vector>



namespace gedit {
    // A view that stacks subviews vertically
    // This is very similar to HStackView just swap width/height
    class VStackView : public ViewBase {
    public:
        VStackView() = default;
        explicit VStackView(const Rect &rect) : ViewBase (rect) {

        }
        void InitView() override {
            auto screen = RuntimeConfig::Instance().GetScreen();
            if (viewRect.IsEmpty()) {
                viewRect = screen->Dimensions();
            }
            window = screen->CreateWindow(viewRect, WindowBase::kWin_Invisible, WindowBase::kWinDeco_None);
            window->SetCaption("VStackView");
            RecomputeLayout();
        }

        void ReInitView() override {
            auto screen = RuntimeConfig::Instance().GetScreen();
            if (viewRect.IsEmpty()) {
                viewRect = screen->Dimensions();
            }
            window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Invisible, WindowBase::kWinDeco_None);
            RecomputeLayout();
        }

        void AddSubView(ViewBase *view, kLayout layout) {
            StackableView stackView = {.layout = layout, .view = view};
            viewStack.push_back(stackView);
            AddView(view);
            RecomputeLayout();
        }

        // Pass this on to someone who actually know how to deal with this - HStackView don't deal with height related stuff
        void MaximizeContentHeight() override {
            parentView->MaximizeContentHeight();
        }
        void RestoreContentHeight() override {
            parentView->RestoreContentHeight();
        }
        void ResetContentHeight() override {
            parentView->ResetContentHeight();
        }

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
            int fixedHeight = 0;
            for(auto &view : viewStack) {
                if (!view.view->IsInitialized()) {
                    view.view->Initialize();
                }
                if ((view.layout == kFixed) && (view.view->IsVisible())) {
                    fixedHeight += view.view->GetViewRect().Height();
                }
            }

            int nFlexible = 0;
            for(auto &view : viewStack) {
                if ((view.layout == kFill) && view.view->IsVisible()) {
                    nFlexible++;
                }
            }

            // 2) Distribute the rest amongst the flexible width items..
            int flexHeight = 0;
            if (nFlexible > 0) {
                flexHeight = (GetContentRect().Height() - fixedHeight) / nFlexible;
            }

            // All subwindows are full height (enforced)
            int xPos = GetContentRect().TopLeft().x;
            int yPos = GetContentRect().TopLeft().y;

            for(auto &view : viewStack) {
                if (!view.view->IsVisible()) {
                    continue;
                }
                auto vRect = view.view->GetViewRect();
                vRect.MoveTo(xPos,yPos);
                // Set the flex-width, otherwise keep the width (assuming the user has set its width properly)
                if (view.layout != kFixed) {
                    vRect.SetHeight(flexHeight);
                }
                // Enforce height of stacked views..
                vRect.SetWidth(GetContentRect().Width());
                view.view->SetViewRect(vRect);
                yPos += vRect.Height();
                // Need too call initialize, if the view has already been initalized this will call 'reinit' which moves the view
                view.view->Initialize();
            }
        }
    protected:
        std::vector<StackableView> viewStack;



    };
}

#endif //EDITOR_VSTACKVIEW_H
