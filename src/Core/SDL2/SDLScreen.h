//
// Created by gnilk on 29.03.23.
//

//

#ifndef EDITOR_SDLSCREEN_H
#define EDITOR_SDLSCREEN_H

//#include <vector>
//#include <utility>
//#include <utility>
//#include <map>

#include <SDL2/SDL.h>
#include <logger.h>

#include "Core/ScreenBase.h"
#include "Core/WindowBase.h"
#include "Core/Rect.h"

namespace gedit {
    class SDLScreen : public ScreenBase {
    public:
        SDLScreen() = default;
        virtual ~SDLScreen() = default;

        static ScreenBase::Ref Create();

        bool Open() override;
        void Close() override;
        void Clear() override;
        void Update() override;
        bool UpdateClipboardData() override;

        void CopyToTexture() override;
        void ClearWithTexture() override;

        void BeginRefreshCycle() override;
        void EndRefreshCycle() override;
        WindowBase *CreateWindow(const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) override;
        WindowBase *UpdateWindow(WindowBase *window, const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) override;
        Rect Dimensions() override;
        void OnSizeChanged() override;
        void OnMoved() override;
    private:
        void ComputeScalingFactors();
        void CreateTextures();
        void LoadFontFromTheme();
        void UpdateWindowLocation();

    private:
        SDL_Window* sdlWindow = nullptr;
        SDL_Renderer* sdlRenderer = nullptr;
        SDL_Texture* sdlScreenAsTexture = nullptr;
        SDL_Surface* sdlScreenAsSurface = nullptr;


        // Initialize screen dimensions to default values
        // This is in pixels
        int widthPixels = 1920;
        int heightPixels = 1080;
        // In characters (which is what the editor works with)
        int rows = 0;
        int cols = 0;

        gnilk::Logger::ILogger *logger = nullptr;

    };
}


#endif //STBMEETSDL_SDLSCREEN_H
