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
    // Reset the content rect...
    contentRect = ViewRect();
    // FIXME: Need to take care of FLAGS - for instance 'editView' has no bottom border
    //        Deflate will push all by delta

    // Verify this
    // contentRect.Deflate(1,0);
    // This works, now we can draw in context from 0.. < Height()
    if (flags & kViewFlags::kViewDrawLowerBorder) {
        contentRect.SetHeight(contentRect.Height() - 1);
    }

    if (flags & kViewFlags::kViewDrawRightBorder) {
        contentRect.SetWidth(contentRect.Width() - 1);
    }
    if (flags & kViewFlags::kViewDrawLeftBorder) {
        contentRect.Move(1,0);
    }

    if ((flags & kViewFlags::kViewDrawUpperBorder) || ((flags & kViewFlags::kViewDrawCaption) && !caption.empty())) {
        contentRect.Move(0, 1);
    }
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
    nativeWindow = RuntimeConfig::Instance().Screen()->CreateWindow(ViewRect());
    for(auto subView : subviews) {
        subView->Begin();
    }
}

void ViewBase::Draw() {
    auto screen = RuntimeConfig::Instance().Screen();

    screen->SetClipRect(ViewRect());
    for(auto subView : subviews) {
        subView->Draw();
        // reset this regardless - drawing should not cause full redraw - that is triggered by something else
    }

//    screen->DrawRect(ViewRect());
//    DrawCaption();



    if ((flags & kViewDrawBorder) == kViewDrawBorder) {
        screen->DrawRect(ViewRect());
    } else {
        // Rect in is drawn Horizontal first then Vertical
        if (flags & kViewDrawUpperBorder) {
            auto ptEnd = ViewRect().BottomRight();
            ptEnd.y = ViewRect().TopLeft().y;
            screen->DrawHLine(ViewRect().TopLeft(), ptEnd);
        }
        if (flags & kViewDrawLowerBorder) {
            auto ptStart = ViewRect().TopLeft();
            ptStart.y = ViewRect().BottomRight().y;
            screen->DrawHLine(ptStart, ViewRect().BottomRight());
        }
        if (flags & kViewDrawLeftBorder) {
            auto ptEnd = ViewRect().TopLeft();
            ptEnd.y = ViewRect().BottomRight().y;
            screen->DrawVLine(ViewRect().TopLeft(), ptEnd);
        }
        if (flags & kViewDrawRightBorder) {
            auto ptStart = ViewRect().TopLeft();
            ptStart.x = ViewRect().BottomRight().x;
            screen->DrawVLine(ptStart, ViewRect().BottomRight());
        }
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
    auto topLeft = ViewRect().TopLeft();
    if (!(flags & kViewDrawUpperBorder)) {
        screen->DrawCharAt(topLeft.x,topLeft.y,'-');
        screen->DrawCharAt(topLeft.x+1,topLeft.y,'-');
    }
    screen->DrawCharAt(topLeft.x+2,topLeft.y,'|');
    screen->DrawStringAt(topLeft.x+3, topLeft.y, caption.c_str());
    screen->DrawCharAt(topLeft.x+3+caption.length(),topLeft.y,'|');
}

void ViewBase::ComputeInitialLayout(const Rect &rect) {

    layout.ComputeLayout(rect);

//    layout.SetNewRect(rect);
//    auto &subViewArea = ContentRect();
//
//    for(auto s : subviews) {
//        s->ComputeInitialLayout(subViewArea);
//        s->ViewRect();
//    }
}


void ViewBase::DumpViewTree() {
    DoDumpViewTree(this, 0);
}

void ViewBase::DoDumpViewTree(ViewBase *view, int depth) {
    std::string indent(depth*4, ' ');
    auto logger = gnilk::Logger::GetLogger("DumpViewTree");
    logger->Debug("%s%s (%d x %d) - (%d,%d)",
           indent.c_str(), view->Caption().c_str(),
           view->ViewRect().TopLeft().x,
           view->ViewRect().TopLeft().y,
           view->ViewRect().Width(),
           view->ViewRect().Height());
    for(auto s : subviews) {
        s->DoDumpViewTree(s, depth+1);
    }
}
