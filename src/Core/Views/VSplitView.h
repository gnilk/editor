//
// Created by gnilk on 17.02.23.
//

#ifndef EDITOR_VSPLITVIEW_H
#define EDITOR_VSPLITVIEW_H

#include "ViewBase.h"

namespace gedit {
    class VSplitView : public ViewBase {
    public:
        VSplitView();
        explicit VSplitView(const Rect &viewRect);
        virtual ~VSplitView() = default;

        void SetLeftView(ViewBase *newLeftView) {
            leftView = newLeftView;
            AddView(leftView);
        }
        void SetRightView(ViewBase *newRightView) {
            rightView = newRightView;
            AddView(rightView);
        }
    private:
        ViewBase *leftView;
        ViewBase *rightView;
    };
}


#endif //EDITOR_VSPLITVIEW_H
