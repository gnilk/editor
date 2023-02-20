//
// Created by gnilk on 20.02.23.
//

#ifndef EDITOR_NATIVEWINDOW_H
#define EDITOR_NATIVEWINDOW_H

namespace gedit {
    class NativeWindow {
    public:
        NativeWindow() = default;
        virtual ~NativeWindow() = default;

        virtual void Scroll(int rows) {}
    };
}

#endif //EDITOR_NATIVEWINDOW_H
