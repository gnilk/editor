//
// Created by gnilk on 17.02.23.
//

#ifndef EDITOR_HSPLITVIEW_H
#define EDITOR_HSPLITVIEW_H

#include "ViewBase.h"
namespace gedit {
    class HSplitView : public ViewBase {
    public:
        HSplitView();
        explicit HSplitView(const Rect &viewArea);
        virtual ~HSplitView() = default;

        void SetTopView(ViewBase *newTopView) {
            topView = newTopView;
            AddView(topView);
        }
        void SetBottomView(ViewBase *newBottomView) {
            bottomView = newBottomView;
            AddView(bottomView);
        }
        void OnKeyPress(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) override;

    protected:
        void MaximizeView();
    private:
        ViewBase *topView = nullptr;
        ViewBase *bottomView = nullptr;
    };
}


#endif //EDITOR_HSPLITVIEW_H
