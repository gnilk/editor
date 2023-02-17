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
    Rect next;
    int stepX;
    int stepY;
    int moveX;
    int moveY;


    std::function<void(CalcRect &, const Rect &, int)> Init = nullptr;
    std::function<void(CalcRect &, bool, ViewBase *view)> Next = nullptr;
};

static void InitHZStackCalc(CalcRect &calcRect, const Rect &initialRect, int nSubViews) {
    calcRect.ptStart = initialRect.TopLeft();
    calcRect.initialRect = initialRect;
    calcRect.stepX = initialRect.Width() / nSubViews;
    calcRect.moveX = 0;

    // 0
    calcRect.next = initialRect;
    calcRect.next.SetWidth(calcRect.stepX - 1);

}

static void NextHZStackRect(CalcRect &calcRect, bool last, ViewBase *view) {
    // Width of previous
    calcRect.moveX += calcRect.next.Width() + 1;

    calcRect.next = Rect(calcRect.initialRect.TopLeft(), calcRect.initialRect.BottomRight());
    if (!last) {
        calcRect.next.SetWidth(calcRect.stepX - 1);
    } else {
        calcRect.next.SetWidth(calcRect.initialRect.Width() - calcRect.moveX - 1);
    }
    calcRect.next.Move(calcRect.moveX, 0);
}

static void InitVertStackCalc(CalcRect &calcRect, const Rect &initialRect, int nSubViews) {
    calcRect.ptStart = initialRect.TopLeft();
    calcRect.initialRect = initialRect;
    calcRect.stepY = initialRect.Height() / nSubViews;
    calcRect.moveY = 0;

    // 0
    calcRect.next = initialRect;
    calcRect.next.SetHeight(calcRect.stepY - 1);

}
static void NextVertStackRect(CalcRect &calcRect, bool last, ViewBase *view) {
    calcRect.moveY += calcRect.next.Height()+1;
    calcRect.next = Rect(calcRect.ptStart, calcRect.initialRect.BottomRight());
    if (!last) {
        calcRect.next.SetHeight(calcRect.stepY - 1);
    } else {
        calcRect.next.SetHeight(calcRect.initialRect.Height() - calcRect.moveY -1);
    }
    calcRect.next.Move(0, calcRect.moveY);
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
                calcRect.next = calcRect.initialRect;
            };
            calcRect.Next = [](CalcRect &calcRect, bool last, ViewBase *view) {
                calcRect.next = calcRect.initialRect;
            };
            break;
        case kViewAnchor_FixedWidth :
            // We have fixed with, so set it...
            calcRect.Init = [](CalcRect &calcRect, const Rect &initialRect, int nSubViews) {
                calcRect.initialRect = initialRect;
                calcRect.next = calcRect.initialRect;
            };
            calcRect.Next = [](CalcRect &calcRect, bool last, ViewBase *view)  {
                calcRect.next = calcRect.initialRect;
            };
            rect.SetHeight(suggestedRect.Height());
            break;
        case kViewAnchor_FixedHeight :
            // We have fixed height, so set it...
            calcRect.Init = [](CalcRect &calcRect, const Rect &initialRect, int nSubViews) {
                calcRect.initialRect = initialRect;
                calcRect.next = calcRect.initialRect;
            };
            calcRect.Next = [](CalcRect &calcRect, bool last, ViewBase *view) {
                calcRect.next = calcRect.initialRect;
            };
            rect.SetWidth(suggestedRect.Width());
            //newRect.SetHeight(rect.Height());
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
    // If our rect is already set, we have decided already - don't mess it up...
    if (rect.IsEmpty()) {
        rect = newRect;
    }

    // If we have sub-views, do the initial calculations..
    if (viewBase->subviews.size() > 0) {
        if ((calcRect.Init == nullptr) || (calcRect.Next == nullptr)) {
            auto logger = gnilk::Logger::GetLogger("ViewLayout");
            logger->Error("CalcRect Init or Next are NULL and you have subviews!!!!");
            exit(1);
        }
        calcRect.Init(calcRect, rect, viewBase->subviews.size());
        Rect next(0,0);
        for (int i = 0; i < viewBase->subviews.size(); i++) {
            auto view = viewBase->subviews[i];
            calcRect.next = view->layout.ComputeLayout(calcRect.next);

            bool bLast = ((i+1) == (viewBase->subviews.size()-1));
            calcRect.Next(calcRect, bLast, view);
        }
    }
    return rect;
}

