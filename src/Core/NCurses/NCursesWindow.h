//
// Created by gnilk on 22.02.23.
//

#ifndef NCWIN_NCURSESWINDOW_H
#define NCWIN_NCURSESWINDOW_H

#include "Core/WindowBase.h"
#include "Core/Rect.h"
#include "Core/Cursor.h"
namespace gedit {
    class NCursesWindow : public WindowBase {
    public:
        NCursesWindow() = default;
        explicit NCursesWindow(const Rect &rect) : WindowBase(rect) {

        }
        virtual ~NCursesWindow() = default;
        void Initialize(WindowBase::kWinFlags flags, WindowBase::kWinDecoration newDecoFlags) override;

        DrawContext &GetContentDC() override;
        void Clear() override {
            wclear((WINDOW *)winptr);
        }
        void Refresh() override {
            wnoutrefresh((WINDOW *)winptr);
            if (clientWindow != nullptr) {
                wnoutrefresh((WINDOW *)clientWindow);
            }
        }

        void SetCursor(const Cursor &cursor) override;

    public:
        // Border, menus and such
        void DrawWindowDecoration() override;
    private:
        void CreateNCursesWindows();
    private:
        DrawContext *clientContext = nullptr;
        WINDOW *clientWindow = nullptr;
    };

}

#endif //NCWIN_NCURSESWINDOW_H
