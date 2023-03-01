//
// Created by gnilk on 22.02.23.
//

#ifndef NCWIN_WINDOWBASE_H
#define NCWIN_WINDOWBASE_H

#include <string>
#include <vector>
#include <stdint.h>

#include "Core/Rect.h"
#include "Core/DrawContext.h"

namespace gedit {
    class WindowBase {
    public:
        typedef enum : int32_t {
            kWin_None = 0,
            kWin_Invisible = 1,
            kWin_Visible = 2,
            kWin_Default = kWin_Visible,
        } kWinFlags;
        // Decoration flags
        typedef enum : int32_t {
            kWinDeco_None = 0,
            kWinDeco_LeftBorder = 1,
            kWinDeco_RightBorder = 2,
            kWinDeco_TopBorder = 4,
            kWinDeco_BottomBorder = 8,
            kWinDeco_Border = 15,
            kWinDeco_DrawCaption = 16,
            kWinDeco_Default = kWinDeco_Border | kWinDeco_DrawCaption,
        } kWinDecoration;
    public:
        WindowBase() = default;
        explicit WindowBase(const Rect &rect) : windowRect(rect) {
        }
        virtual ~WindowBase() = default;

        virtual void Initialize(WindowBase::kWinFlags newFlags, WindowBase::kWinDecoration newDecoFlags) {
            flags = newFlags;
            decorationFlags =  newDecoFlags;
        }

        void AddSubWindow(WindowBase *subWindow) {
            subwindows.push_back(subWindow);
        }

        virtual void Clear() {

        }

        virtual void Draw() {
            if (flags & WindowBase::kWin_Visible) {
                DrawWindowDecoration();
            }
        }

        virtual void Refresh() {

        }

        WindowBase::kWinFlags GetFlags() {
            return flags;
        }
        NativeWindow GetNativeWindow() {
            return winptr;
        }

        virtual DrawContext &GetWindowDC() {
            return *drawContext;
        }

        virtual DrawContext &GetContentDC() {
            return GetWindowDC();
        }

        void SetCaption(const std::string &newCaption) {
            caption = newCaption;
        }

    public:
        // Border, menus and such
        virtual void DrawWindowDecoration() {

        }
    protected:
        std::string caption = "";
        WindowBase::kWinFlags flags = kWin_Default;
        WindowBase::kWinDecoration decorationFlags = kWinDeco_Default;
        Rect windowRect = {};
        NativeWindow winptr = nullptr;
        DrawContext *drawContext = nullptr;

        std::vector<WindowBase *> subwindows;
    };
}

#endif //NCWIN_WINDOWBASE_H
