//
// Created by gnilk on 14.04.23.
//

#ifndef EDITOR_MODALVIEW_H
#define EDITOR_MODALVIEW_H

#include "logger.h"
#include "ViewBase.h"

namespace gedit {
    class ModalView : public ViewBase {
    public:
        ModalView() = default;
        explicit ModalView(const Rect &viewArea) : ViewBase(viewArea) {

        }
        ModalView(const Rect &viewArea, ViewBase *rootView) : ViewBase(viewArea), viewPtr(rootView) {

        }
        virtual ~ModalView() = default;
        void InitView() override;
        void ReInitView() override;

        void SetDispatchView(ViewBase *newDispatchView) {
            dispatchView = newDispatchView;
        }

        bool OnAction(const KeyPressAction &kpAction) override;
    protected:
        void OnKeyPress(const KeyPress &keyPress) override;
        void DrawViewContents() override;
    private:
        ViewBase *viewPtr = nullptr;
        ViewBase *dispatchView = nullptr;   // Where to dispatch actions
        gnilk::ILogger *logger = nullptr;
    };

}


#endif //EDITOR_MODALVIEW_H
