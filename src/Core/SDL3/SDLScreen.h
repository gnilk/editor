//
// Created by gnilk on 29.03.23.
//

#ifndef EDITOR_SDLSCREEN_H
#define EDITOR_SDLSCREEN_H

//#include <vector>
//#include <utility>
//#include <utility>
//#include <map>

#include <SDL3/SDL.h>

#include "Core/ScreenBase.h"
#include "Core/WindowBase.h"
#include "Core/Rect.h"

namespace gedit {
    class SDLScreen : public ScreenBase {
    public:
        SDLScreen() = default;
        virtual ~SDLScreen() = default;

        bool Open() override;
        void Close() override;
        void Clear() override;
        void Update() override;
        void CopyToTexture() override;
        void ClearWithTexture() override;

        void BeginRefreshCycle() override;
        void EndRefreshCycle() override;
        WindowBase *CreateWindow(const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) override;
        WindowBase *UpdateWindow(WindowBase *window, const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) override;
        Rect Dimensions() override;
    private:
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        SDL_Texture* screenAsTexture = nullptr;
        SDL_Surface* screenAsSurface = nullptr;

        // Initialize screen dimensions to default values
        // This is in pixels
        int widthPixels = 1920;
        int heightPixels = 1080;
        // In characters (which is what the editor works with)
        int rows = 0;
        int cols = 0;

    };
}


#endif //STBMEETSDL_SDLSCREEN_H
