//
// Created by gnilk on 29.03.23.
//

//

#ifndef EDITOR_SDL2_SDLSCREEN_H
#define EDITOR_SDL2_SDLSCREEN_H

//#include <vector>
//#include <utility>
//#include <utility>
//#include <map>

#include <SDL2/SDL.h>
#include <logger.h>

#include "Core/UI/Graphics/ScreenBase.h"
#include "Core/MouseEvent.h"
#include "Core/UI/Graphics/WindowBase.h"
#include "Core/Rect.h"

namespace gedit::SDL2 {
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
        bool CellSizeInPixels(float &width, float &height) override;
        float PixelDensity() override {
            return pixelDensity;
        }
        void OnSizeChanged() override;
        void OnMoved() override;
        void PollEvents() override;
    protected:
        bool GetPrimaryDisplayBounds(int &x, int &y, int &width, int &height) override;
    private:
        void ComputeScalingFactors();
        void CreateTextures();
        // Screen pixels per logical (window) pixel, asked of SDL - 2 on a Retina display
        float WindowPixelDensity();
        // The theme's font, rasterised for 'density' (docs/open-bugs.md #18)
        void LoadFontFromTheme(float density);
        // The window's pixel size changed - lay out again only when it is because the density did
        void OnPixelSizeChanged();
        void SetMousePosition(MouseEvent &mouseEvent, float pixelX, float pixelY);
        // Load the app icon (appicon.png) via the asset loader and set it as the window/taskbar
        // icon. Without this the WM has no icon to show (ALT+TAB) unless a matching .desktop is
        // installed system-wide — which is not the case for an AppImage or a dev build.
        void SetWindowIcon();
        // Read the live SDL window geometry and report it into the session (in memory).
        void ReportWindowGeometry();

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
        float pixelDensity = 1.0f;      // screen pixels per logical pixel, as the font was built for

        gnilk::Logger::ILogger *logger = nullptr;

    };
}


#endif //EDITOR_SDL2_SDLSCREEN_H
