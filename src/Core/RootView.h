//
// Created by gnilk on 15.02.23.
//

#ifndef EDITOR_ROOTVIEW_H
#define EDITOR_ROOTVIEW_H

#include "Core/ViewBase.h"

namespace gedit {
    class RootView : public ViewBase {
    public:
        RootView() = default;
        explicit RootView(const Rect &viewArea) : ViewBase(viewArea) {

        }
        virtual ~RootView() = default;

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
            }
        }

        void OnKeyPress(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) override;
    private:
        int idxCurrentTopView = -1;
        std::vector<ViewBase *> topViews;
    };
}


#endif //EDITOR_ROOTVIEW_H
