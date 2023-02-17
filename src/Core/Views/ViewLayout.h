//
// Created by gnilk on 17.02.23.
//

#ifndef EDITOR_VIEWLAYOUT_H
#define EDITOR_VIEWLAYOUT_H
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
            // Note: A fully fledged system should have plenty more (Relative, etc..) but I don't need them
        } kViewAnchoring;
    public:
        explicit ViewLayout(const Rect &viewRect) : rect(viewRect) {

        }
        virtual  ~ViewLayout() = default;

        // Compute the layout given a certain base...
        void SetNewRect(const Rect &newViewRect) {
            rect = newViewRect;
            ComputeLayout();
        }
        const Rect &GetRect() const {
            return rect;
        }
        void ComputeLayout() {

        }
    private:
        kViewAnchoring anchoring;
        Rect rect;
    };
}

#endif //EDITOR_VIEWLAYOUT_H
