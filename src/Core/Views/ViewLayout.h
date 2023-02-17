//
// Created by gnilk on 17.02.23.
//

#ifndef EDITOR_VIEWLAYOUT_H
#define EDITOR_VIEWLAYOUT_H
#include "Core/Rect.h"

namespace gedit {
    class ViewBase;

    // Specific class to recompute the view-rect area depending on anchoring settings
    // We need the following anchor types
    //   Fill = fully attached and filling the parent view
    //   FixedWidth  = this view has a fixed width (Gutter)
    //   FixedHeight = this view has a fixed height (StatusBar, and ?)
    class ViewLayout {
    public:
        typedef enum {
            kViewAnchor_Fill = 0,           // This is the default
            kViewAnchor_FixedWidth = 1,
            kViewAnchor_FixedHeight = 2,
            kViewAnchor_VerticalStack = 3,      // Split sub items vertically (stack subviews on top of each other)
            kViewAnchor_HorizontalStack = 4,    // Split sub items horizontally (stack subviews next of each other)
            // Note: A fully fledged system should have plenty more (Relative, etc..) but I don't need them
        } kViewAnchoring;
    public:
        explicit ViewLayout(const Rect &viewRect, ViewBase *owner) : rect(viewRect), viewBase(owner) {

        }
        virtual  ~ViewLayout() = default;

        // Compute the layout given a certain base...
        void SetNewRect(const Rect &newViewRect) {
            rect = newViewRect;
            ComputeLayout(newViewRect);
        }
        const Rect &GetRect() const {
            return rect;
        }
        void SetAnchoring(kViewAnchoring newAnchoring) {
            anchoring = newAnchoring;
        }
        const Rect &ComputeLayout(const Rect &newRect);
    protected:

    private:
        kViewAnchoring anchoring = kViewAnchor_Fill;
        Rect rect;
        ViewBase *viewBase = nullptr;
    };
}

#endif //EDITOR_VIEWLAYOUT_H
