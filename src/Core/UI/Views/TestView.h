//
// Created by gnilk on 11.05.23.
//

#ifndef EDITOR_TESTVIEW_H
#define EDITOR_TESTVIEW_H

#include "VisibleView.h"

namespace gedit {
    class TestView : public VisibleView {
    public:
        TestView() = default;
        virtual ~TestView() = default;
        explicit TestView(const Rect &rect) : VisibleView(rect) {
        }

        void DrawViewContents() override;
        void DrawSplitter(int row);
    };
}


#endif //EDITOR_TESTVIEW_H
