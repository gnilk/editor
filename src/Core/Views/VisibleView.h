//
// Created by gnilk on 16.04.23.
//

#ifndef EDITOR_VISIBLEVIEW_H
#define EDITOR_VISIBLEVIEW_H

#include "ViewBase.h"
namespace gedit {
    // This implements the common Init/ReInit needed by basically all VisibleViews
    class VisibleView : public ViewBase {
    public:
        virtual ~VisibleView() = default;
        void InitView() override;
        void ReInitView() override;
    };
}


#endif //EDITOR_VISIBLEVIEW_H
