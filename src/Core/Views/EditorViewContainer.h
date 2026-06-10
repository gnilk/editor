//
// Created by gnilk on 09.06.26.
//
// EditorViewContainer — the single editing slot in the layout, and the registered *top view* for that
// slot (focus/keymap/status hub). It holds two representations of the active document: a primary text
// EditorView and an alternate read-only HexView, and swaps between them on a view-mode switch action
// (HexView spike, H.4). Exactly one item is active+visible at a time; the container forwards input,
// status and focus to that active item, so the rest of the app keeps talking to one stable top view
// regardless of which representation is showing. This is also the seam the future buffer/window split
// (Phase 3) hangs off — primary/alternate becomes N items.
//

#ifndef EDITOR_EDITORVIEWCONTAINER_H
#define EDITOR_EDITORVIEWCONTAINER_H

#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"
#include "ViewBase.h"
#include "Core/ActionHelper.h"

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
            // A document switch re-initializes the whole view tree (Editor::SetActiveDocument ->
            // RootView::Initialize). Reconcile the visible item to the now-active document's stored
            // view-mode, so the mode restores per-document across a switch.
            SyncToActiveDocument();
        }

        // The primary (text) editing item, and the initial active item. Modelled on HStackView::AddSubView:
        // the item's layout handler is the container (which delegates resizing on up), and it becomes a
        // regular subview so the base class drives its Initialize/Draw/Resize.
        void SetContentView(ViewBase *view) {
            primaryItem = view;
            activeItem = view;
            contentView = view;
            view->SetLayoutHandler(this);
            AddView(view);
            view->SetVisible(true);
            LayoutContentView();
        }

        // The alternate (hex) item — added but hidden until a view-mode switch makes it active.
        void SetAlternateView(ViewBase *view) {
            alternateItem = view;
            view->SetLayoutHandler(this);
            AddView(view);
            view->SetVisible(false);
            LayoutContentView();
        }

        // --- Top-view proxy. The container is registered as the top view for the editing slot, so input,
        //     status and focus are forwarded to whichever item is currently active. ---

        void HandleKeyPress(const KeyPress &keyPress) override {
            if (activeItem != nullptr) {
                activeItem->HandleKeyPress(keyPress);
            }
        }

        bool OnAction(const EditorAction &kpAction) override {
            // The container owns the swap (it holds both items); the active item never sees these.
            if (kpAction.action == kAction::kActionViewModeText) {
                SwitchToViewMode(DocumentViewMode::kText);
                return true;
            }
            if (kpAction.action == kAction::kActionViewModeHex) {
                SwitchToViewMode(DocumentViewMode::kHex);
                return true;
            }
            if (kpAction.action == kAction::kActionCycleActiveBufferNext) {
                ActionHelper::SwitchToNextBuffer();
                InvalidateAll();
                return true;
            }
            if (kpAction.action == kAction::kActionCycleActiveBufferPrev) {
                ActionHelper::SwitchToPreviousBuffer();
                InvalidateAll();
                return true;
            }
            if ((activeItem != nullptr) && activeItem->OnAction(kpAction)) {
                return true;
            }
            return ViewBase::OnAction(kpAction);
        }

        const std::u32string &GetStatusBarAbbreviation() override {
            if (activeItem != nullptr) {
                return activeItem->GetStatusBarAbbreviation();
            }
            return ViewBase::GetStatusBarAbbreviation();
        }
        std::pair<std::u32string, std::u32string> GetStatusBarInfo() override {
            if (activeItem != nullptr) {
                return activeItem->GetStatusBarInfo();
            }
            return ViewBase::GetStatusBarInfo();
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
        // Focus follows the container down to the active item (so its keymap installs and it draws its
        // own caret). The container's own (invisible) window has nothing to draw.
        void OnActivate(bool bActive) override {
            if (activeItem != nullptr) {
                activeItem->SetActive(bActive);
            }
        }
        void SetWindowCursor(const Cursor &) override {
            // no-op: the active item draws its own cursor in its DoDraw.
        }

        // User intent (the toggle action): record the mode on the document so it restores per-document,
        // then apply the swap.
        void SwitchToViewMode(DocumentViewMode mode) {
            auto document = Editor::Instance().GetActiveDocument();
            if (document != nullptr) {
                document->SetViewMode(mode);
            }
            ApplyViewMode(mode);
        }

        // Restore path (a document switch): read the now-active document's stored mode and apply it,
        // without writing intent back. No-ops when the mode already matches the visible item.
        void SyncToActiveDocument() {
            auto document = Editor::Instance().GetActiveDocument();
            if (document == nullptr) {
                return;
            }
            ApplyViewMode(document->GetViewMode());
        }

        // The one place the text<->hex swap happens. Flips active+visible, re-inits the now-active item
        // (re-point at the active document + geometry; HexView rebuilds its byte stream) and installs its
        // keymap. Focus is transferred only if the outgoing item held it — a document-switch sync runs
        // during the view-tree re-init regardless of which view is focused, and must not steal focus.
        void ApplyViewMode(DocumentViewMode mode) {
            ViewBase *next = (mode == DocumentViewMode::kHex) ? alternateItem : primaryItem;
            if ((next == nullptr) || (next == activeItem)) {
                return;
            }
            bool wasActive = activeItem->IsActive();
            activeItem->SetActive(false);
            activeItem->SetVisible(false);

            activeItem = next;
            contentView = next;
            activeItem->SetVisible(true);
            LayoutContentView();
            activeItem->Initialize();
            if (wasActive) {
                activeItem->SetActive(true);
            }
            InvalidateAll();
        }

        // Both items fill the container's content rect (only the active one is visible). When the
        // container later holds N items this becomes a real split layout.
        void LayoutContentView() {
            if (window == nullptr) {
                return;
            }
            auto rect = GetContentRect();
            if (primaryItem != nullptr) {
                primaryItem->SetViewRect(rect);
            }
            if (alternateItem != nullptr) {
                alternateItem->SetViewRect(rect);
            }
        }

    protected:
        ViewBase *contentView = nullptr;    // == activeItem (kept for the LayoutContentView legacy name)
        ViewBase *primaryItem = nullptr;    // text editor — initial active item
        ViewBase *alternateItem = nullptr;  // hex view — hidden until switched to
        ViewBase *activeItem = nullptr;
    };
}

#endif //EDITOR_EDITORVIEWCONTAINER_H
