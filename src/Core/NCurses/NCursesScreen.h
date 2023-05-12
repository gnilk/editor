//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_NCURSESSCREEN_H
#define EDITOR_NCURSESSCREEN_H

#include <vector>
#include <utility>

#include "logger.h"

#include "Core/ScreenBase.h"
#include "Core/WindowBase.h"
#include "Core/Rect.h"

namespace gedit {
    class NCursesScreen : public ScreenBase {
    public:
        NCursesScreen() = default;
        virtual ~NCursesScreen() = default;
        bool Open() override;
        void Close() override;
        void Clear() override;
        void Update() override;
        void RegisterColor(int appIndex, const ColorRGBA &foreground, const ColorRGBA &background) override;

        void BeginRefreshCycle() override;
        void EndRefreshCycle() override;
        WindowBase *CreateWindow(const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) override;
        WindowBase *UpdateWindow(WindowBase *window, const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) override;
        Rect Dimensions() override;
    private:
        gnilk::ILogger *logger = nullptr;
        int widthChars = 0;
        int heightChars = 0;
   };
}
#endif //EDITOR_NCURSESSCREEN_H
