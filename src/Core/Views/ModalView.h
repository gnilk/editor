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
        virtual ~ModalView() = default;
        void InitView() override;
        void ReInitView() override;

        bool OnAction(const KeyPressAction &kpAction) override;
    protected:
        void OnKeyPress(const KeyPress &keyPress) override;
        void DrawViewContents() override;
    private:
        gnilk::ILogger *logger = nullptr;
    };
}


#endif //EDITOR_MODALVIEW_H
