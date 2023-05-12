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
        NCursesWindow(const Rect &rect, NCursesWindow *other);


        virtual ~NCursesWindow() noexcept;
        void Initialize(WindowBase::kWinFlags flags, WindowBase::kWinDecoration newDecoFlags) override;

        DrawContext &GetContentDC() override;
        void Clear() override;

        void Refresh()  override {
            wnoutrefresh((WINDOW *)winptr);
            if (clientWindow != nullptr) {
                wnoutrefresh((WINDOW *)clientWindow);
            }
        }
        void TestRefreshEx() override {
            wrefresh((WINDOW *)winptr);
            if (clientWindow != nullptr) {
                wrefresh((WINDOW *)clientWindow);
            }
        }

        void SetCursor(const Cursor &cursor) override;

    public:
        // Border, menus and such
        void DrawWindowDecoration() override;
    protected:
        void OnDrawCursor(const Cursor &cursor);
        void CreateNCursesWindows();
    private:
        char tmp_fillChar = 'x';
        DrawContext *clientContext = nullptr;
        WINDOW *clientWindow = nullptr;
    };

}

#endif //NCWIN_NCURSESWINDOW_H
