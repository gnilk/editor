//
// Created by gnilk on 24.02.23.
//

#ifndef NCWIN_VIEWBASE_H
#define NCWIN_VIEWBASE_H

#include <functional>
#include <memory>

#include "Core/Rect.h"
#include "Core/WindowBase.h"
#include "Core/KeyPress.h"
#include "Core/Cursor.h"
#include "Core/Action.h"
#include "Core/KeyMapping.h"
#include "Core/SafeQueue.h"
#include "Core/KeypressAndActionHandler.h"

namespace gedit {

    class VStackView;

    // Should never be used on it's own...
    class ViewBase : public KeypressAndActionHandler {
        friend VStackView;
    public:
        using Ref = std::shared_ptr<ViewBase>;
        using MessageCallback = std::function<void(void)>;
    public:
        ViewBase() = default;
        explicit ViewBase(const Rect &rect) : viewRect(rect) {
            hasExplicitSize = true;
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
            hasExplicitSize = true;
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
        virtual void SetWidth(int newWidth) {
            viewRect.SetWidth(newWidth);
        }
        virtual void SetHeight(int newHeight) {
            viewRect.SetHeight(newHeight);
        }
        virtual int GetWidth() final {
            return viewRect.Width();
        }
        virtual int GetHeight() final {
            return viewRect.Height();
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
            isInvalid = false;
        }

        // We override this in the editor view as we have a cursor in the EditorModel that is shared
        virtual void SetWindowCursor(const Cursor &cursor);

        virtual void AddView(ViewBase *view) final {
            // We don't allow an empty view rect, so let's set it to our own...
            if (view->GetViewRect().IsEmpty()) {
                view->SetViewRect(GetContentRect());
            }
            subviews.push_back(view);
            view->parentView = this;
            if (!view->IsInitialized()) {
                view->Initialize();
            }

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
        virtual bool IsVisible() {
            return isVisible;
        }
        virtual void SetVisible(bool newIsVisible) {
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

        bool HandleAction(const KeyPressAction &action) override {
            return OnAction(action);
        }
        void HandleKeyPress(const KeyPress &keyPress) override;

        virtual void OnKeyPress(const KeyPress &keyPress) {
            if (parentView != nullptr) {
                parentView->OnKeyPress(keyPress);
            }
        }

        virtual bool OnAction(const KeyPressAction &action);

        virtual void PostMessage(MessageCallback callback) final;
        //virtual int ProcessMessageQueue() final;

        void CloseModal() {
            SetActive(false);
        }

        virtual void Resize() final {
            DoResize();
            Initialize();
        }

        // Override this in any TOPVIEW (i.e. views that can receive input) class to show an short for the active view in the statusbar
        virtual const std::u32string &GetStatusBarAbbreviation() {
            static std::u32string defaultAbbr = U"X";
            return defaultAbbr;
        }
        virtual std::pair<std::u32string, std::u32string> GetStatusBarInfo() {
            return {U"",U""};
        }

        void SetLayoutHandler(ViewBase *newLayoutHandler) {
            layoutHandler = newLayoutHandler;
        }

        ViewBase *GetLayoutHandler() {
            if (layoutHandler == nullptr) {
                // During startup...
                if (parentView != nullptr) {
                    return parentView->GetLayoutHandler();
                }
                return nullptr;
            }
            return layoutHandler;
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
        virtual void OnActivate(bool newIsActive) {
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

    protected:
        virtual void OnActionIncreaseWidth();
        virtual void OnActionDecreaseWidth();
        virtual void OnActionIncreaseHeight();
        virtual void OnActionDecreaseHight();

    private:
        void DoResize() {
            // unless the user has set the size explicitly, we clear it...
            if (!hasExplicitSize) {
                viewRect = {};
            }

            ReInitView();
            for(auto &view : subviews) {
                view->DoResize();
            }
        }

        void DoInitialize() {
            InitView();
            for(auto &view : subviews) {
                view->Initialize();
            }
            isInitialized = true;
            OnViewInitialized();
        }
        void DoReInitialize() {
            ReInitView();
            for(auto &view : subviews) {
                view->Initialize();
            }
            isInitialized = true;
            OnViewInitialized();
        }


    protected:
        bool hasExplicitSize = false;  // Won't be affected by resize
        bool isActive = false;  // If receiving keyboard/mouse input
        bool isVisible = true; // If subject to draw-call's
        Cursor cursor = {};
        std::vector<ViewBase *> subviews = {};
        WindowBase *window = nullptr;       // The underlying graphical window
        ViewBase *layoutHandler = nullptr;
        ViewBase *parentView = nullptr;
        ViewBase *modal = nullptr;
        Rect viewRect = {};
        bool isInitialized = false;
        bool isInvalid = false;
        void *sharedDataPtr = nullptr;      // Whatever you want - this is to share data between views - liked HSTack or Tab's or similar
        //SafeQueue<MessageCallback> threadMessages;
    };

}
#endif //NCWIN_VIEWBASE_H
