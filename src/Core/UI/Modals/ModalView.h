//
// Created by gnilk on 14.04.23.
//

#ifndef EDITOR_MODALVIEW_H
#define EDITOR_MODALVIEW_H

#include <memory>
#include <string>

#include "logger.h"
#include "Core/UI/Views/ViewBase.h"

namespace gedit {
    //
    // A modal is an overlay hosted by the RootView (RootView::ShowModal), not a second run loop. While it
    // is the top modal it gets all input. Close/Cancel only mark it - the host pops it after the current
    // dispatch, then calls OnClosed exactly once and destroys it. Subclasses turn OnClosed into their own
    // typed callbacks (a choice, a picked item, a text).
    //
    class ModalView : public ViewBase {
    public:
        using Ptr = std::unique_ptr<ModalView>;
    public:
        ModalView() = default;
        explicit ModalView(const Rect &viewArea) : ViewBase(viewArea) {

        }
        ModalView(const Rect &viewArea, ViewBase *rootView) : ViewBase(viewArea), viewPtr(rootView) {

        }
        virtual ~ModalView() = default;
        void InitView() override;
        void ReInitView() override;

        void SetCaption(const std::string &newCaption);

        void SetDispatchView(ViewBase *newDispatchView) {
            dispatchView = newDispatchView;
        }

        bool OnAction(const EditorAction &kpAction) override;

        // Done with a result (the subclass holds it)
        void Close();
        // Done without a result - Esc (CloseModal)
        void Cancel();
        bool IsClosing() const {
            return isClosing;
        }
        bool IsCancelled() const {
            return isCancelled;
        }

        // Host only - called once, after the modal has been popped
        void NotifyClosed() {
            OnClosed(isCancelled);
        }
    protected:
        virtual void OnClosed(bool wasCancelled) {}
        void DrawViewContents() override;
        // Where the modal goes on a screen of this size - default: half the screen, in the middle
        virtual Rect ComputeRect(const Rect &screenRect);
        // A box of this size (clamped to the screen) in the middle
        static Rect CenterIn(const Rect &screenRect, int width, int height);
    private:
        ViewBase *viewPtr = nullptr;
        ViewBase *dispatchView = nullptr;   // Where to dispatch actions
        std::string caption = {};
        bool isClosing = false;
        bool isCancelled = false;
    };

}


#endif //EDITOR_MODALVIEW_H
