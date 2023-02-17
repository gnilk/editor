//
// Created by gnilk on 17.02.23.
//

#include "ViewLayout.h"
#include "ViewBase.h"

#include "logger.h"

#include <functional>


using namespace gedit;

struct CalcRect {
    Point ptStart;
    Rect initialRect;
    int stepX;
    int stepY;
    int moveX;
    int moveY;

    std::function<void(CalcRect &, const Rect &, int)> Init = nullptr;
    std::function<Rect(CalcRect &, int, ViewBase *view)> Next = nullptr;
};

static void InitHZStackCalc(CalcRect &calcRect, const Rect &initialRect, int nSubViews) {
    calcRect.ptStart = initialRect.TopLeft();
    calcRect.initialRect = initialRect;
    calcRect.stepX = initialRect.Width() / nSubViews;
    calcRect.moveX = 0;
}

static Rect NextHZStackRect(CalcRect &calcRect, int viewNum, ViewBase *view) {
    Rect nextRect(calcRect.ptStart, calcRect.initialRect.BottomRight());
    nextRect.SetWidth(calcRect.stepX);
    nextRect.Move(calcRect.moveX, 0);
    calcRect.moveX += calcRect.stepX;
    return nextRect;

}

static void InitVertStackCalc(CalcRect &calcRect, const Rect &initialRect, int nSubViews) {
    calcRect.ptStart = initialRect.TopLeft();
    calcRect.initialRect = initialRect;
    calcRect.stepY = initialRect.Height() / nSubViews;
    calcRect.moveY = 0;
}
static Rect NextVertStackRect(CalcRect &calcRect, int viewNum, ViewBase *view) {
    Rect nextRect(calcRect.ptStart, calcRect.initialRect.BottomRight());
    nextRect.SetHeight(calcRect.stepY);
    nextRect.Move(0, calcRect.moveY);
    calcRect.moveY += calcRect.stepY;
    return nextRect;
}


//
// Called with the suggested rect to be used by the layout
// Note: We (shouldn't really) go outside, but we can limit it (in case we have a fixed size)
//
const Rect &ViewLayout::ComputeLayout(const Rect &suggestedRect) {

    Rect newRect = suggestedRect;

    CalcRect calcRect;
    switch(anchoring) {
        case kViewAnchor_Fill :
            // Fill mode - parent decides, do nothing...
            calcRect.Init = [](CalcRect &calcRect, const Rect &initialRect, int nSubViews) {
                calcRect.initialRect = initialRect;
            };
            calcRect.Next = [](CalcRect &calcRect, int viewNum, ViewBase *view) -> Rect {
                return calcRect.initialRect;
            };
            break;
        case kViewAnchor_FixedWidth :
            // We have fixed with, so set it...
            calcRect.Init = [](CalcRect &calcRect, const Rect &initialRect, int nSubViews) {
                calcRect.initialRect = initialRect;
            };
            calcRect.Next = [](CalcRect &calcRect, int viewNum, ViewBase *view) -> Rect {
                return calcRect.initialRect;
            };
            newRect.SetWidth((rect.Width()));
            break;
        case kViewAnchor_FixedHeight :
            // We have fixed height, so set it...
            calcRect.Init = [](CalcRect &calcRect, const Rect &initialRect, int nSubViews) {
                calcRect.initialRect = initialRect;
            };
            calcRect.Next = [](CalcRect &calcRect, int viewNum, ViewBase *view) -> Rect {
                return calcRect.initialRect;
            };
            newRect.SetHeight(rect.Height());
            break;
        case kViewAnchor_HorizontalStack :
            calcRect.Init = InitHZStackCalc;
            calcRect.Next = NextHZStackRect;
            break;
        case kViewAnchor_VerticalStack :
            calcRect.Init = InitVertStackCalc;
            calcRect.Next = NextVertStackRect;
            break;
    }
    // Now commit the new rectangle to our rect...
    rect = newRect;

    // If we have sub-views, do the initial calculations..
    if (viewBase->subviews.size() > 0) {
        if ((calcRect.Init == nullptr) || (calcRect.Next == nullptr)) {
            auto logger = gnilk::Logger::GetLogger("ViewLayout");
            logger->Error("CalcRect Init or Next are NULL and you have subviews!!!!");
            exit(1);
        }
        calcRect.Init(calcRect, newRect, viewBase->subviews.size());
        Rect next;
        for (int i = 0; i < viewBase->subviews.size(); i++) {
            auto view = viewBase->subviews[i];
            next = calcRect.Next(calcRect, i, view);
            view->layout.ComputeLayout(next);
        }
    }
    return rect;
}

