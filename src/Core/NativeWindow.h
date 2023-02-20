//
// Created by gnilk on 20.02.23.
//

#ifndef EDITOR_NATIVEWINDOW_H
#define EDITOR_NATIVEWINDOW_H

#include "Core/DrawContext.h"
#include "Core/NativeWindowHandle.h"

namespace gedit {
    // Native window is a base class representing the underlying UI framework window concept
    // Each graphical subsystem implementation should provide an implementation to create a
    // NativeWindow inherited specialization.
    // The Native Window is used primarily as a link between the View <-> DrawContext for
    // a specific graphics subsystem..
    // Ergo; NCurses have a Window and DrawContext specialization which are used to localize drawing
    //       in NCurses the root-window (i.e. primary surface) is represented by the stdscr Window..
    class NativeWindow {
    public:
        NativeWindow() = default;
        virtual ~NativeWindow() = default;

        virtual DrawContext *CreateDrawContext() {
            return new DrawContext();
        }
        virtual void Scroll(int rows) {}
        virtual void Refresh() {}
        virtual NativeWindowHandle NativeHandle() { return nullptr; }
    };
}

#endif //EDITOR_NATIVEWINDOW_H
