//
// Created by gnilk on 13.02.23.
//
#include "ViewBase.h"
#include "Core/ScreenBase.h"
#include "Core/RuntimeConfig.h"
#include "logger.h"


using namespace gedit;

// This prepares and returns a reference to a bunch of drawing routines...
DrawContext &ViewBase::ContentAreaDrawContext() {
    RecomputeContentRect();
    contentAreaDrawContext.Update(contentRect);
    return contentAreaDrawContext;
}
void ViewBase::RecomputeContentRect() {
    contentRect = viewRect;
    // FIXME: Need to take care of FLAGS - for instance 'editView' has no bottom border
    //        Deflate will push all by delta

    // Verify this
    // contentRect.Deflate(1,0);
    // This works, now we can draw in context from 0.. < Height()
    contentRect.SetHeight(contentRect.Height()-1);
    contentRect.SetWidth(contentRect.Width()-1);
    contentRect.Move(1,1);
}
void ViewBase::OnKeyPress(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) {
    // Send down to root..
    if (parentView != nullptr) {
        parentView->OnKeyPress(keyPress);
        return;
    }
}



void ViewBase::Begin() {
    RecomputeContentRect();
    for(auto subView : subviews) {
        subView->Begin();
    }
}

void ViewBase::Draw() {
    auto screen = RuntimeConfig::Instance().Screen();

    screen->SetClipRect(viewRect);
    for(auto subView : subviews) {
        subView->Draw();
        // reset this regardless - drawing should not cause full redraw - that is triggered by something else
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
    invalidate = false;

    // Update cursor - if this view is active
    if (isActive) {
        Cursor screenCursor;
        auto &ctx = ViewBase::ContentAreaDrawContext();
        screenCursor.position = ctx.ToScreen(cursor.position);
        screen->SetCursor(screenCursor);
    }
}

void ViewBase::DrawCaption() {
    auto screen = RuntimeConfig::Instance().Screen();
    auto topLeft = viewRect.TopLeft();
    screen->DrawCharAt(topLeft.x+2,topLeft.y,'|');
    screen->DrawStringAt(topLeft.x+3, topLeft.y, caption.c_str());
    screen->DrawCharAt(topLeft.x+3+caption.length(),topLeft.y,'|');
}
