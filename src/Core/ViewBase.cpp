//
// Created by gnilk on 13.02.23.
//
#include "ViewBase.h"
#include "Core/ScreenBase.h"
#include "Core/RuntimeConfig.h"


using namespace gedit;

void ViewBase::Draw() {
    auto screen = RuntimeConfig::Instance().Screen();
    screen->SetClipRect(viewRect);
    for(auto subView : subviews) {
        subView->Draw();
    }
    if (flags & kViewDrawBorder) {
        // TODO: Fix drawing flags for rect in screen
        screen->DrawRect(viewRect);
    }
    if ((!caption.empty()) && (flags & kViewDrawCaption)) {
        DrawCaption();
    }

    // FIXME: Make sure we have the content area rect setup properly..

    screen->SetClipRect(contentRect);
    DrawViewContents();

}
void ViewBase::DrawCaption() {
    auto screen = RuntimeConfig::Instance().Screen();
    auto topLeft = viewRect.TopLeft();
    screen->DrawCharAt(topLeft.x+2,topLeft.y,'|');
    screen->DrawStringAt(topLeft.x+3, topLeft.y, caption.c_str());
    screen->DrawCharAt(topLeft.x+3+caption.length(),topLeft.y,'|');
}
