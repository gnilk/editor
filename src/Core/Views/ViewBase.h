//
// Created by gnilk on 24.02.23.
//

#ifndef NCWIN_VIEWBASE_H
#define NCWIN_VIEWBASE_H

#include "Core/Rect.h"
#include "Core/WindowBase.h"
#include "Core/KeyPress.h"
#include "Core/Cursor.h"
#include "Core/Action.h"
#include "Core/KeyMapping.h"
#include "Core/SafeQueue.h"

namespace gedit {
    // Should never be used on it's own...
    class ViewBase {
    public:
        using MessageCallback = std::function<void(void)>;
    public:
        ViewBase() = default;
        explicit ViewBase(const Rect &rect) : viewRect(rect) {

        }
        virtual ~ViewBase() = default;

        virtual void Initialize() final {
            if (!isInitialized) {
                DoInitialize();
            } else {
                DoReInitialize();
            }
        }

        virtual void SetViewRect(const Rect &rect) {
            viewRect = rect;
        }

        virtual Rect GetViewRect() {
            return viewRect;
        }
        virtual Rect GetContentRect() {
            if (window == nullptr) {
                return viewRect;
            }
            return window->GetContentDC().GetRect();
        }

        // this is just a shortcut
        virtual void SetWidth(int newWidth) final {
            viewRect.SetWidth(newWidth);
        }
        virtual void SetHeight(int newHeight) final {
            viewRect.SetHeight(newHeight);
        }
        virtual void AdjustHeight(int deltaHeight) {
            // we do nothing, you are supposed to override this
        }

        virtual void MaximizeContentHeight() {
            // Do nothing, override in specific view to handle
        }
        // Lousy name????
        virtual void RestoreContentHeight() {

        }

        virtual void ResetContentHeight() {

        }
        // This is purely done in order to handle the window stuff...
        virtual void Draw() {
            if (modal != nullptr) {
                modal->Draw();
                return;
            }
            DoDraw();
        }

        virtual void FinalizeDraw() final {
            window->Refresh();
            for (auto view : subviews) {
                view->FinalizeDraw();
            }
        }

        virtual void DoDraw() {

            window->DrawWindowDecoration();
            if (window->GetFlags() & WindowBase::kWin_Visible) {
                DrawViewContents();
            }
            window->Refresh();


            for(auto view : subviews) {
                if (view->IsVisible()) {
                    view->Draw();
                }
            }

            if (isActive) {
                SetWindowCursor(cursor);
            }

            // Reset once we are done..
            // FIXME: This should perhaps be placed in separate call - as 'isVisible' will stop the recursion...
            isInvalid = false;
        }

        // We override this in the editor view as we have a cursor in the EditorModel that is shared
        virtual void SetWindowCursor(const Cursor &cursor) {
            window->SetCursor(cursor);
        }

        virtual void AddView(ViewBase *view) final {
            // We don't allow an empty view rect, so let's set it to our own...
            if (view->GetViewRect().IsEmpty()) {
                view->SetViewRect(viewRect);
            }
            subviews.push_back(view);
            view->parentView = this;
        }

        virtual ViewBase *GetParentView() {
            return parentView;
        }

        virtual WindowBase *GetWindow() final {
            return window;
        }

        bool IsInitialized() {
            return isInitialized;
        }
        bool IsInvalid() {
            return isInvalid;
        }
        bool IsVisible() {
            return isVisible;
        }
        virtual void SetVisible(bool newIsVisible) final {
            isVisible = newIsVisible;
        }
        virtual void SetActive(bool newIsActive) final {
            isActive = newIsActive;
            OnActivate(isActive);
        }
        bool IsActive() {
            if (!isActive) {
                for(auto s : subviews) {
                    if (s->IsActive()) {
                        return true;
                    }
                }
            }
            return isActive;
        }

        // This will mark all views in the full view hierarchy as invalid...
        virtual void InvalidateAll() final {
            isInvalid = true;
            for(auto s : subviews) {
                if (!s->IsInvalid()) {
                    s->InvalidateAll();
                }
            }
            if (parentView != nullptr) {
                parentView->InvalidateAll();
            }

        }
        // This will mark the hierarchy from the view and down to root as invalid - without touching siblings of
        // the underlying windows
        virtual void InvalidateView() {
            isInvalid = true;
            if (parentView != nullptr) {
                parentView->InvalidateView();
            }
        }

        void SetSharedData(void *newSharedData) {
            sharedDataPtr = newSharedData;
        }
        template<typename T>
        const T *GetSharedData() {
            return ((T *)(sharedDataPtr));
        }
        bool HasSharedData() {
            return (sharedDataPtr != nullptr);
        }
        virtual void HandleKeyPress(const KeyPress &keyPress) {
            OnKeyPress(keyPress);
        }

        // TESTING ACTIONS!!!!
        virtual bool OnAction(const KeyPressAction &action) {
            return false;
        }

        virtual void PostMessage(MessageCallback callback) final;
        virtual int ProcessMessageQueue() final;
        // END OF ACTION TEST

        void CloseModal() {
            SetActive(false);
        }

        //
        // Override these to handles, they are called in an event kind of manner on certain actions
        //
    protected:
        virtual void OnViewInitialized() {
            for(auto view : subviews) {
                view->OnViewInitialized();
            }
        }
        virtual void OnActivate(bool isActive) {
        }

        virtual void OnKeyPress(const KeyPress &keyPress) {
            if (parentView != nullptr) {
                parentView->OnKeyPress(keyPress);
            }
        }
        virtual void OnResized() {}
    protected:
        // Called the first time a view should be created
        virtual void InitView() {

        }
        // Called when view proprties (like size, and so forth) has changed and you
        // need to reinitialize/move or other wise fiddle with the underlying graphical subsystem
        virtual void ReInitView() {

        }
        // You should override these to draw your contents
        virtual void DrawViewContents() {

        }
    private:
        void DoInitialize() {
            InitView();
            for(auto view : subviews) {
                view->Initialize();
            }
            isInitialized = true;
            OnViewInitialized();
        }
        void DoReInitialize() {
            ReInitView();
            for(auto view : subviews) {
                view->Initialize();
            }
            isInitialized = true;
            OnViewInitialized();
        }

    protected:
        bool isActive = false;  // If receiving keyboard/mouse input
        bool isVisible = true; // If subject to draw-call's
        Cursor cursor = {};
        std::vector<ViewBase *> subviews = {};
        WindowBase *window = nullptr;       // The underlying graphical window
        ViewBase *parentView = nullptr;
        ViewBase *modal = nullptr;
        Rect viewRect = {};
        bool isInitialized = false;
        bool isInvalid = false;
        void *sharedDataPtr = nullptr;      // Whatever you want - this is to share data between views - liked HSTack or Tab's or similar
        SafeQueue<MessageCallback> threadMessages;
    };

}
#endif //NCWIN_VIEWBASE_H
