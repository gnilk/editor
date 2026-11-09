//
// Created by gnilk on 29.03.23.
//

#ifndef EDITOR_SDL3_SDLSCREEN_H
#define EDITOR_SDL3_SDLSCREEN_H

//#include <vector>
//#include <utility>
//#include <utility>
//#include <map>

#include <SDL3/SDL.h>

#include "Core/UI/Graphics/ScreenBase.h"
#include "Core/UI/Graphics/WindowBase.h"
#include "Core/Rect.h"
#include "logger.h"
namespace gedit::SDL3 {
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
        void PollEvents() override;
    protected:
        bool GetPrimaryDisplayBounds(int &x, int &y, int &width, int &height) override;
    private:
        void ComputeScalingFactors();
        void CreateTextures();
        // Screen pixels per logical (window) pixel - 2 on a Retina display
        float PixelDensity();
        // The theme's font, rasterised for 'density' (docs/open-bugs.md #18)
        void LoadFontFromTheme(float density);
        // The window's pixel size changed - lay out again only when it is because the density did
        void OnPixelSizeChanged();
        // Load the app icon (appicon.png) via the asset loader and set it as the window/taskbar
        // icon. Without this the WM has no icon to show (ALT+TAB) unless a matching .desktop is
        // installed system-wide — which is not the case for an AppImage or a dev build.
        void SetWindowIcon();
        // Read the live SDL window geometry and report it into the session (in memory).
        void ReportWindowGeometry();

    private:
        gnilk::ILogger *logger = nullptr;
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
        // The cell the pointer was last reported in - a move is only sent when it leaves it
        int lastMouseCol = -1;
        int lastMouseRow = -1;

    };
}


#endif //EDITOR_SDL3_SDLSCREEN_H
