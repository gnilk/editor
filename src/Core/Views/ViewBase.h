//
// Created by gnilk on 24.02.23.
//

#ifndef NCWIN_VIEWBASE_H
#define NCWIN_VIEWBASE_H

#include "Core/Rect.h"
#include "Core/WindowBase.h"
#include "Core/KeyPress.h"
#include "Core/Cursor.h"

namespace gedit {
    // Should never be used on it's own...
    class ViewBase {
    public:
        ViewBase() = default;
        explicit ViewBase(const Rect &rect) : viewRect(rect) {

        }
        virtual ~ViewBase() = default;

        virtual void Initialize() final {
            InitView();
            for(auto view : subviews) {
                view->Initialize();
            }
            isInitialized = true;
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


        virtual void Draw() final {
            // Note: Not sure...
            if ((isInvalid) && (parentView == nullptr)) {
                window->Clear();
            }
            window->Draw();

            for(auto view : subviews) {
                view->Draw();
            }

            if (window->GetFlags() & WindowBase::kWin_Visible) {
                DrawViewContents();
            }
            window->Refresh();

            // Reset once we are done..
            isInvalid = false;
        }

        virtual void AddView(ViewBase *view) final {
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

        virtual void InvalidateAll() final {
            isInvalid = true;
            for(auto s : subviews) {
                s->InvalidateAll();
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
    protected:
        virtual void OnKeyPress(const KeyPress &keyPress) {}
        virtual void OnResized() {}

    protected:
        // You should override these to draw your contents
        virtual void InitView() {

        }
        virtual void DrawViewContents() {

        }

    protected:
        Cursor cursor = {};
        std::vector<ViewBase *> subviews = {};
        WindowBase *window = nullptr;
        ViewBase *parentView = nullptr;
        Rect viewRect = {};
        bool isInitialized = false;
        bool isInvalid = false;
        void *sharedDataPtr = nullptr;
    };

}
#endif //NCWIN_VIEWBASE_H
