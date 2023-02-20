//
// Created by gnilk on 20.02.23.
//

#ifndef EDITOR_NCURSESWINDOW_H
#define EDITOR_NCURSESWINDOW_H

#include "Core/NativeWindow.h"
namespace gedit {
    class NCursesWindow : public NativeWindow {
    public:
        NCursesWindow() = default;
        explicit NCursesWindow(WINDOW *ncursesWindow) : window(ncursesWindow) {

        }
        virtual ~NCursesWindow() = default;

        void Scroll(int rows) override;

    private:
        WINDOW *window = nullptr;
    };
}


#endif //EDITOR_NCURSESWINDOW_H
