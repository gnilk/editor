//
// Created by gnilk on 14.02.23.
//

#ifndef EDITOR_GUTTERVIEW_H
#define EDITOR_GUTTERVIEW_H

#include <cstdint>

#include "Core/Rect.h"
#include "Core/UI/Graphics/DrawContext.h"
#include "Core/UI/Views/ViewBase.h"

namespace gedit {
    class GutterView : public ViewBase {
    public:
        GutterView();
        explicit GutterView(const Rect &viewArea);
        virtual ~GutterView() = default;

        void InitView() override;
        void ReInitView() override;
        void DrawViewContents() override;

        // Where the change mark goes: between the line number ('%6zu') and the active line's '*'
        static constexpr int CHANGE_MARK_COLUMN = 6;
    private:
        void DrawChangeMark(DrawContext &dc, int row, uint8_t mark);
    };
}


#endif //EDITOR_GUTTERVIEW_H
