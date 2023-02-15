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

    // Hmm, normally this would be 'Deflate(1,1)' to shrink screen with one row above and below..
    // But for NCurses we don't waste space on the extra row a border char would need...
    contentRect.Deflate(1,0);
    contentRect.Move(0,1);
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
