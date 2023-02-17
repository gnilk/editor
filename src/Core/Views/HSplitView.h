//
// Created by gnilk on 17.02.23.
//

#ifndef EDITOR_HSPLITVIEW_H
#define EDITOR_HSPLITVIEW_H

#include "ViewBase.h"
namespace gedit {
    class HSplitView : public ViewBase {
    public:
        explicit HSplitView(const Rect &viewArea) : ViewBase(viewArea) {

        }
        virtual ~HSplitView() = default;

        void SetLeftView(ViewBase *newLeftView) {
            leftView = newLeftView;
        }
        void SetRightView(ViewBase *newRightView) {
            rightView = newRightView;
        }

        void OnKeyPress(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) override;
    protected:
        void MaximizeView();
    private:
        ViewBase *leftView = nullptr;
        ViewBase *rightView = nullptr;
    };
}


#endif //EDITOR_HSPLITVIEW_H
