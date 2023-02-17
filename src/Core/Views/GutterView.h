//
// Created by gnilk on 14.02.23.
//

#ifndef EDITOR_GUTTERVIEW_H
#define EDITOR_GUTTERVIEW_H

#include "Core/Rect.h"
#include "ViewBase.h"

namespace gedit {
    class GutterView : public ViewBase {
    public:
        GutterView();
        explicit GutterView(const Rect &viewArea);

        virtual ~GutterView() = default;
        void DrawViewContents() override;
    };
}


#endif //EDITOR_GUTTERVIEW_H
