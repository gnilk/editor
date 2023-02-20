//
// Created by gnilk on 20.02.23.
//

#ifndef EDITOR_NCURSESWINDOW_H
#define EDITOR_NCURSESWINDOW_H

#include "Core/NativeWindow.h"
#include "NCursesDrawContext.h"
namespace gedit {
    class NCursesWindow : public NativeWindow {
    public:
        NCursesWindow() = default;
        explicit NCursesWindow(WINDOW *ncursesWindow) : window(ncursesWindow) {

        }
        virtual ~NCursesWindow() = default;

        DrawContext *CreateDrawContext() override {
            return new NCursesDrawContext((NativeWindowHandle)window);
        }


        void Scroll(int rows) override;

        void Refresh() override;

        NativeWindowHandle NativeHandle() override {
            return (NativeWindowHandle)window;
        }


    private:
        WINDOW *window = nullptr;
    };
}


#endif //EDITOR_NCURSESWINDOW_H
