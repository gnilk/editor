//
// Created by gnilk on 09.06.26.
//
// EditorViewContainer — the single editing slot in the layout. It holds the EditorView as its one
// child item and sizes it to fill. This is the seam for the future buffer/window split (Phase 2):
// today exactly one item, later the container can hold N items (e.g. via a VSplitView) for
// side-by-side views of the same or different documents. Pure interposition for now — behavior is
// identical to placing the EditorView directly in the layout.
//

#ifndef EDITOR_EDITORVIEWCONTAINER_H
#define EDITOR_EDITORVIEWCONTAINER_H

#include "Core/RuntimeConfig.h"
#include "ViewBase.h"

namespace gedit {

    class EditorViewContainer : public ViewBase {
    public:
        EditorViewContainer() = default;
        explicit EditorViewContainer(const Rect &rect) : ViewBase(rect) {
        }
        virtual ~EditorViewContainer() = default;

        void InitView() override {
            auto screen = RuntimeConfig::Instance().GetScreen();
            if (viewRect.IsEmpty()) {
                viewRect = screen->Dimensions();
            }
            window = screen->CreateWindow(viewRect, WindowBase::kWin_Invisible, WindowBase::kWinDeco_None);
            window->SetCaption("EditorViewContainer");
            LayoutContentView();
        }

        void ReInitView() override {
            auto screen = RuntimeConfig::Instance().GetScreen();
            if (viewRect.IsEmpty()) {
                viewRect = screen->Dimensions();
            }
            window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Invisible, WindowBase::kWinDeco_None);
            LayoutContentView();
        }

        // The single editing item. Modelled on HStackView::AddSubView: the item's layout handler is
        // the container (which delegates resizing on up), and it becomes a regular subview so the
        // base class drives its Initialize/Draw/Resize.
        void SetContentView(ViewBase *view) {
            contentView = view;
            view->SetLayoutHandler(this);
            AddView(view);
            LayoutContentView();
        }

        // The container owns no splitter — delegate the resize actions up its layout chain, exactly
        // like the stack views do.
        void OnActionIncreaseWidth() override  { GetLayoutHandler()->OnActionIncreaseWidth(); }
        void OnActionDecreaseWidth() override  { GetLayoutHandler()->OnActionDecreaseWidth(); }
        void OnActionIncreaseHeight() override { GetLayoutHandler()->OnActionIncreaseHeight(); }
        void OnActionDecreaseHeight() override { GetLayoutHandler()->OnActionDecreaseHeight(); }

        // The item forwards these to its parentView (us); pass them on up the real layout chain.
        void MaximizeContentHeight() override { parentView->MaximizeContentHeight(); }
        void RestoreContentHeight() override  { parentView->RestoreContentHeight(); }
        void ResetContentHeight() override    { parentView->ResetContentHeight(); }

    protected:
        // Single child fills the container's content rect. (When the container later holds N items
        // this becomes a real split layout.)
        void LayoutContentView() {
            if ((window == nullptr) || (contentView == nullptr)) {
                return;
            }
            contentView->SetViewRect(GetContentRect());
        }

    protected:
        ViewBase *contentView = nullptr;
    };
}

#endif //EDITOR_EDITORVIEWCONTAINER_H
