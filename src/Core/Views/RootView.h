//
// Created by gnilk on 15.02.23.
//

#ifndef EDITOR_ROOTVIEW_H
#define EDITOR_ROOTVIEW_H

#include "ViewBase.h"

namespace gedit {
    class RootView : public ViewBase {
    public:
        RootView() = default;
        explicit RootView(const Rect &viewArea) : ViewBase(viewArea) {

        }
        virtual ~RootView() = default;

        void Draw() override;

        ViewBase *TopView() {
            if (idxCurrentTopView == -1) {
                // No views added
                return nullptr;
            }
            return topViews[idxCurrentTopView];
        }

        void AddTopView(ViewBase *newTopView) {
            topViews.push_back(newTopView);
            if (idxCurrentTopView == -1) {
                idxCurrentTopView = 0;
                TopView()->SetActive(true);
            }
        }

        void OnKeyPress(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) override;
    protected:
        void MaximizeView();
    private:
        int idxCurrentTopView = -1;
        std::vector<ViewBase *> topViews;
    };
}


#endif //EDITOR_ROOTVIEW_H
