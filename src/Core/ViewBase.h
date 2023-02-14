//
// Created by gnilk on 12.02.23.
//

#ifndef EDITOR_VIEWBASE_H
#define EDITOR_VIEWBASE_H

#include <string>
#include <vector>

#include "Rect.h"
#include "Point.h"
#include "Line.h"

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
    class DrawContextBase {
    public:
        // Parameters?
        DrawContextBase(const Rect &rect) : clipRect(rect) {}

        void SetTextColor() {}
        void SetTextAttributes() {}

        void DrawStringAt(const Point &pt, const char *str) {}
        void DrawStringAt(int x, int y, const char *str) {}
    private:
        Rect clipRect;
    };

    class LineDrawing {
    public:
        LineDrawing(DrawContextBase *wef) {}
        virtual void DrawLines(const std::vector<Line *> &lines, int idxActiveLine) {}
        virtual void DrawLineAt(int row, const std::string &prefix, const Line *line) {}
    };


// Make views contain views - this will give us a nice docking feature layout..
// if you want 'floating' we just de-couple a view from the parent...
// Might need something to handle the layout and reposition the views
    class ViewBase {
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
        ViewBase() = default;
        explicit ViewBase(const Rect &viewArea) : viewRect(viewArea) {

            contentRect = viewRect;
            contentRect.Deflate(1,1);
        }
        ~ViewBase() = default;

        virtual void Begin() {}

        void SetFlags(kViewFlags newFlags) {
            flags = newFlags;
        }
        kViewFlags Flags() {
            return flags;
        }
        void SetCaption(const std::string &newCaption) {
            caption = newCaption;
        }

        // These are specifically for drawing of the view itself
        virtual void Draw();
        virtual void DrawCaption();
        // This is what a view normally should override - called to draw the view contents
        virtual void DrawViewContents() {}

        const Rect &Dimensions() const {
            return viewRect;
        }

        const Rect &ContentRect() const {
            return contentRect;
        }

        void AddView(ViewBase *view) {
            subviews.push_back(view);
        }
        // Methods (?) for drawing IN the view



        // Events, need proper interface
        virtual void OnKeyPress(gedit::NCursesKeyboardDriverNew::KeyPress keyPress) {}

    private:
        kViewFlags flags = (kViewFlags)(kViewDrawBorder | kViewDrawCaption);
        std::string caption = "";
        Rect viewRect;
        Rect contentRect;   // Content rectangle is the rect -1
        std::vector<ViewBase *> subviews;
    };


    class CommandView : public ViewBase {
    public:
    private:
        // Rewrite this to be a bit more like a controller serving the view
        //   View <-> Controller <-> Data
        // CommandMode commandMode;
    };

}
#endif //EDITOR_VIEWBASE_H