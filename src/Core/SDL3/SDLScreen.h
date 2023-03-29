//
// Created by gnilk on 29.03.23.
//

#ifndef EDITOR_SDLSCREEN_H
#define EDITOR_SDLSCREEN_H

#include <vector>
#include <utility>
#include "Core/ScreenBase.h"
#include "Core/WindowBase.h"
#include "Core/Rect.h"

#include <SDL3/SDL.h>
#include <utility>
#include <map>

namespace gedit {
    class SDLScreen : public ScreenBase {
    public:
        SDLScreen() = default;
        virtual ~SDLScreen() = default;

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

    public:
    private:
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        // Initialize screen dimensions to default values
        // This is in pixels
        int widthPixels = 1920;
        int heightPixels = 1080;
        // In characters (which is what the editor works with)
        int rows = 0;
        int cols = 0;

        std::map<int, std::pair<ColorRGBA, ColorRGBA>> colorpairs;
    };
}


#endif //STBMEETSDL_SDLSCREEN_H
