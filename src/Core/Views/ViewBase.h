//
// Created by gnilk on 12.02.23.
//

#ifndef EDITOR_VIEWBASE_H
#define EDITOR_VIEWBASE_H

#include <string>
#include <vector>

#include "Core/Rect.h"
#include "Core/Point.h"
#include "Core/Line.h"
#include "Core/Cursor.h"
#include "Core/ScreenBase.h"
#include "Core/NativeWindow.h"
#include "Core/DrawContext.h"
#include "ViewLayout.h"

// TEMP
#include "Core/NCurses/NCursesKeyboardDriver.h"

namespace gedit {

    // Just a draft...
    // Perhaps let the underlying graphical subsystem a chance to allocate a 'window'..
    // NCurses support this as does any other UI library (IMGUI, Win32, Cocoa, Carbon, etc..)
    //
    // How to implement drawing
    // 1) Either do the Win32 way - get a 'Context' of some sort and let that do drawing
    // 2) Let the view implement the drawing directly
    //
    // Pro/Con?
    // Seems to me the 'view' is perhaps more like a 'window'... In that case the view/window
    // can implement the drawing calls directly..
    //
    // A view should also have 'Resize', 'Hide', 'Show', etc...
    //
    // How to implement a tabbed view??
    // One root-view - with sub-views of each view and an index to the active view?
    // the root view has a special "caption" drawing routine?
    //
    // How to navigate views (specifically - if views are not 'locked')
    // Lock some key-combination (CMD+F1..F10) for views, and you can only have 10 views?
    // Same for buffers, CMD+0..9 switch buffer...
    //
    // Should we implement some 'modal' stuff later on?
    // Consider this directly in the design if the case..
    //
    // Now - in case of keyboard handling - we probably need to push this from a central location to
    // the currently active view...
    //
    // Also, the editor and terminal will now be Sublclasses (through composition?) to the views...
    //


    // The base of a context area where we can draw text

//    class LineDrawing {
//    public:
//        LineDrawing(DrawContextBase *wef) {}
//        virtual void DrawLines(const std::vector<Line *> &lines, int idxActiveLine) {}
//        virtual void DrawLineAt(int row, const std::string &prefix, const Line *line) {}
//    };


// Make views contain views - this will give us a nice docking feature layout..
// if you want 'floating' we just de-couple a view from the parent...
// Might need something to handle the layout and reposition the views



    class ViewBase {
        friend ViewLayout;
    public:
        typedef enum {
            kViewNone = 0,
            kViewDrawUpperBorder = 1,
            kViewDrawLowerBorder = 2,
            kViewDrawLeftBorder = 4,
            kViewDrawRightBorder = 8,
            kViewDrawBorder = (kViewDrawUpperBorder | kViewDrawLowerBorder | kViewDrawRightBorder | kViewDrawLeftBorder),
            kViewDrawCaption = 16,
        } kViewFlags;
    public:
        ViewBase() : layout(Rect(), this) {

        }
        explicit ViewBase(const Rect &viewArea) : layout(viewArea, this) {
            contentRect = layout.GetRect();
            contentRect.Deflate(1,1);
        }
        ~ViewBase() = default;

        virtual void Begin();

        // Call this to move the window but OnResize to handle the result
        virtual void Move(const Rect &newViewArea) final {
            layout.SetNewRect(newViewArea);
            RecomputeContentRect();
//            contentRect = newViewArea;
//            contentRect.Deflate(1,1);
            OnResized();
            InvalidateAll();
        }

        void SetFlags(kViewFlags newFlags) {
            flags = newFlags;
            RecomputeContentRect();
        }

        kViewFlags Flags() {
            return flags;
        }

        void SetCaption(const std::string &newCaption) {
            caption = newCaption;
        }

        DrawContext *ContentAreaDrawContext();

        // These are specifically for drawing of the view itself
        virtual void Draw();
        virtual void DrawCaption();
        // This is what a view normally should override - called to draw the view contents
        virtual void DrawViewContents() {}

        const Rect &ViewRect() const {
            return layout.GetRect();
        }

        // FIXME: Better naming and also definition if this is in screen-coords or window-coords
        //        currently this is in window coords..
        const Rect &ContentRect() {
            RecomputeContentRect();
            return contentRect;
        }

        void SetSharedData(void *newSharedData) {
            sharedDataPtr = newSharedData;
        }
        template<typename T>
        const T *GetSharedData() {
            return ((T *)(sharedDataPtr));
        }

        void AddView(ViewBase *view) {
            view->parentView = this;
            subviews.push_back(view);
        }

        void SetActive(bool newIsActive) {
            isActive = newIsActive;
        }

        // Invalidate all nodes in the tree...
        void InvalidateAll() {
            // Set to invalid
            invalidate = true;
            // Walk down, but NOT if parent already is set (i.e. avoid infinte recursion)
            if ((parentView != nullptr) && (!parentView->IsInvalid())){
                parentView->InvalidateAll();
            }
            // For each sub-view not already invalidated, set it...
            for(auto subview : subviews) {
                if (!subview->IsInvalid()) {
                    subview->InvalidateAll();
                }
            }
        }

        void SetViewAnchoring(ViewLayout::kViewAnchoring newAnchoring) {
            layout.SetAnchoring(newAnchoring);
        }

        bool IsInvalid() {
            return invalidate;
        }

        ViewBase *ParentView() {
            return parentView;
        }
        // Methods (?) for drawing IN the view
        const std::string &Caption() const { return caption; }

        // Events, need proper interface
        virtual void OnKeyPress(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress);
        // You should implement this one...
        virtual void OnResized() {
            for(auto s : subviews) {
                s->OnResized();
            }
        }

        // This is just a gateway to kick off the Layout manager - you should NOT call this
        virtual void ComputeInitialLayout(const Rect &rect) final;

        void DumpViewTree();

        NativeWindow *GetNativeWindow() {
            return nativeWindow;
        }

    protected:
        void RecomputeContentRect();
    private:
        void DoDumpViewTree(ViewBase *view, int depth);

    protected:
        Cursor cursor = {};
        std::vector<ViewBase *> subviews;       // List of all topviews
        ViewLayout layout;
        NativeWindow *nativeWindow = nullptr;   // Underlying Window

    private:
        kViewFlags flags = (kViewFlags)(kViewDrawCaption);
        std::string caption = "";
        bool invalidate = false;
        Rect contentRect;   // Content rectangle is the rect -1
        void *sharedDataPtr = nullptr;
        ViewBase *parentView = nullptr;
        //std::vector<ViewBase *> subviews;       // List of all topviews
        DrawContext *contentAreaDrawContext = nullptr;
        bool isActive = false;
    };


    // REMOVE THIS

    // This view is simply to hold and position other views..
    class LayoutView : public ViewBase {
    public:
        explicit LayoutView(const Rect &viewArea) : ViewBase(viewArea) {
            SetFlags(kViewFlags::kViewNone);
        }
        // Events, need proper interface
        void OnKeyPress(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) override {
            ViewBase::OnKeyPress(keyPress);
        }
    };


}
#endif //EDITOR_VIEWBASE_H
