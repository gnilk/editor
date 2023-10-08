//
// Created by gnilk on 29.03.23.
//
// We keep everything in editor coordinates (character row/col) until we must communicate with SDL
// Only the native window dimensions are stored in pixel coordinates (might change) - but for now that's how it is...
//
// Note: There are two code-paths available - one using textures and one using primary render target directly..
//
// Note2: I need a color repository (local for SDL) as NCurses is using a color macro and the DrawContext don't have
//        access to the Screen class...
//        This can be a singleton with a simple RegisterColor/GetColor
//

#include "SDLScreen.h"
#include "SDLWindow.h"
#include "SDLTranslate.h"
#include "SDLFontManager.h"
#include "SDLDrawContext.h"
#include "SDLCursor.h"

#include "Core/RuntimeConfig.h"
#include "Core/Config/Config.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include "Core/Editor.h"
#include "Core/WindowLocation.h"

#define STB_RECT_PACK_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "ext/stb_rect_pack.h"
#include "ext/stb_truetype.h"

#define STBTTF_IMPLEMENTATION
#include "ext/stbttf.h"


using namespace gedit;

#define WIDTH 1920
#define HEIGHT 1080


//static const std::string fontName = "Andale Mono.ttf";

ScreenBase::Ref SDLScreen::Create() {
    auto instance = std::make_shared<SDLScreen>();
    return instance;
}


bool SDLScreen::Open() {

    logger = gnilk::Logger::GetLogger("SDLScreen");

    logger->Debug("Opening window");

    int nDrivers = SDL_GetNumVideoDrivers();
    logger->Debug("Available Video Drivers (%d):",nDrivers);
    for(int i=0;i<nDrivers;i++) {
        auto driverName = SDL_GetVideoDriver(i);
        logger->Debug("  %d:%s",i,driverName);
    }

    int err = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_HAPTIC);
    if (err) {
        logger->Error("SDL_Init, %s", SDL_GetError());
        printf("Error: SDL_Init, %s\n", SDL_GetError());
        exit(1);
    }

    logger->Debug("SDL initialized ok, video driver = %s", SDL_GetCurrentVideoDriver());

    int windowFlags = SDL_WINDOW_RESIZABLE;

    // Check SDL 'backend' to see if OPENGL/METAL/Other should be used...
    auto sdlBackend = Config::Instance()["sdl"].GetStr("backend", "opengl");
    if (sdlBackend == "opengl") {
        logger->Debug("Using backend: '%s'", sdlBackend.c_str());
        windowFlags |= SDL_WindowFlags::SDL_WINDOW_OPENGL;
    } else if (sdlBackend == "metal") {
        logger->Debug("Using backend: '%s'", sdlBackend.c_str());
        windowFlags |= SDL_WindowFlags::SDL_WINDOW_METAL;
    } else if (sdlBackend == "vulkan") {
        logger->Debug("Using backend: '%s'", sdlBackend.c_str());
        windowFlags |= SDL_WindowFlags::SDL_WINDOW_VULKAN;
    } else {
        logger->Error("Unknown backend ('%s'), using default", sdlBackend.c_str());
    }

    // Try to fetch this via RuntimeConfig
    auto &windowLocation = RuntimeConfig::Instance().GetWindowLocation();
    if (!windowLocation.Load()) {
        logger->Debug("Previous window location not found - will use defaults!");
    }
    int windowXpos = windowLocation.XPos();
    int windowYpos = windowLocation.YPos();
    widthPixels = windowLocation.Width();
    heightPixels = windowLocation.Height();

    // FIXME: Need to determine how HighDPI stuff works...
    sdlWindow = SDL_CreateWindow("gedit", windowXpos, windowYpos, widthPixels, heightPixels,  windowFlags);
    sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);

    logger->Debug("Resolution: %d x %d", widthPixels, heightPixels);

    LoadFontFromTheme();
    ComputeScalingFactors();
    CreateTextures();
    return true;
}

void SDLScreen::LoadFontFromTheme() {
    // Resolve font name from theme
    auto currentTheme = Editor::Instance().GetTheme();
    if (currentTheme == nullptr) {
        logger->Error("Theme not loaded!!!!");
        return;
    }
    auto fontName = currentTheme->GetStr("font","Andale Mono.ttf");
    logger->Debug("Loading font: '%s'", fontName.c_str());

    // Load the font through the asset loader
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    auto fontAsset = assetLoader.LoadAsset(fontName);
    if (fontAsset == nullptr) {
        logger->Error("Unable to open font: '%s'\n", fontName.c_str());
        return;
    }
    // Create an in-memory loader for this asset and open the font
    auto sdlRWOps = SDL_RWFromConstMem(fontAsset->GetPtrAs<const void *>(), (int)fontAsset->GetSize());
    auto font = STBTTF_OpenFontRW(sdlRenderer, sdlRWOps, 18);

    if (font == nullptr) {
        logger->Error("Unable to load font: '%s'\n", fontName.c_str());
        return;
    }

    // Set the font active..
    SDLFontManager::Instance().SetActiveFont(font);

}

// This is called also from resize...
void SDLScreen::ComputeScalingFactors() {

    auto displayId = SDL_GetWindowDisplayIndex(sdlWindow);
    SDL_DisplayMode displayMode;
    SDL_GetDesktopDisplayMode(displayId, &displayMode);
    SDL_GetWindowSize(sdlWindow, &widthPixels, &heightPixels);
    float ddpi, vdpi, hdpi;
    SDL_GetDisplayDPI(displayId, &ddpi, &hdpi, &vdpi);

    // FIXME: need the correct value here...
    float display_scale = 1.0f;

    logger->Debug("Resolution: %d x %d", widthPixels, heightPixels);
    logger->Debug("Display, pixels: %d x %d (scale: %f)", displayMode.w, displayMode.h, display_scale);
    logger->Debug("Display, points: %d x %d\n", displayMode.w, displayMode.h);

    auto font = SDLFontManager::Instance().GetActiveFont();

    float line_margin = Config::Instance()["sdl"].GetInt("line_margin", 4);
    line_margin *= display_scale;
    rows = heightPixels / (font->baseline + line_margin); // baseline = font->ascent * font->scale

    // subjective representation of average type of chars you might find in a something
    // small,wide,average type of chars
    std::string textToMeasure = "AaWwiI109 []{}/*.,\"";
    auto textWidth =  STBTTF_MeasureText(font, textToMeasure.c_str());
    auto fontWidthAverage = textWidth / (float)textToMeasure.length();
    cols = widthPixels / fontWidthAverage;

    logger->Debug("Font scaling factors:");
    logger->Debug("  Height: %d px (font: %d, line margin: %f)", (int)(font->baseline + line_margin), font->baseline, line_margin);
    logger->Debug("  Width : %d px (based on average widht for '%s')", (int)fontWidthAverage, textToMeasure.c_str());

    logger->Debug("Text to Graphics defined as");
    cols *= display_scale;
    rows *= display_scale;
    logger->Debug("Rows=%d, Cols=%d", rows, cols);

    // Setup translation
    SDLTranslate::fac_x_to_rc = (float)cols / (float)(widthPixels * display_scale);
    SDLTranslate::fac_y_to_rc = (float)rows / (float)(heightPixels * display_scale);

    logger->Debug("Scaling factors = (%f,%f)", SDLTranslate::fac_x_to_rc, SDLTranslate::fac_y_to_rc);

}

void SDLScreen::CreateTextures() {

    SDL_GetWindowSize(sdlWindow, &widthPixels, &heightPixels);

    if (sdlScreenAsSurface != nullptr) {
        SDL_FreeSurface(sdlScreenAsSurface);
    }
    if (sdlScreenAsTexture != nullptr) {
        SDL_DestroyTexture(sdlScreenAsTexture);
    }
    // Note: Might not need this can perhaps use: SDL_LockTextureToSurface
    sdlScreenAsSurface = SDL_CreateRGBSurfaceWithFormat(0, widthPixels, heightPixels, 0, SDL_PIXELFORMAT_RGBA32);
    sdlScreenAsTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, widthPixels, heightPixels);


}


void SDLScreen::OnSizeChanged() {
    logger->Debug("Size changed!!!");
    logger->Debug("Recomputing scaling factors and recreating tetxures");
    ComputeScalingFactors();
    CreateTextures();
    UpdateWindowLocation();
    logger->Debug("ReInitialize UI!");
    RuntimeConfig::Instance().GetRootView().Resize();
    RuntimeConfig::Instance().GetRootView().InvalidateAll();

    // This a mechanism we can use to trigger a redraw in the main run-loop..
    // Just post a message, but we don't care about the callback - so an empty lambda..
    RuntimeConfig::Instance().GetRootView().PostMessage([](){});
}

void SDLScreen::OnMoved() {
    logger->Debug("Window moved, updating WindowLocation State file");
    UpdateWindowLocation();
}

void SDLScreen::UpdateWindowLocation() {
    auto &windowLocation = RuntimeConfig::Instance().GetWindowLocation();
    // Fetch and update size
    SDL_GetWindowSize(sdlWindow, &widthPixels, &heightPixels);
    windowLocation.SetWidth(widthPixels);
    windowLocation.SetHeight(heightPixels);

    // Fetch and update position
    int winXpos, winYpos;
    SDL_GetWindowPosition(sdlWindow, &winXpos, &winYpos);
    windowLocation.SetXPos(winXpos);
    windowLocation.SetYPos(winYpos);

    // Save
    windowLocation.Save();
}

void SDLScreen::Close() {
    auto font = SDLFontManager::Instance().GetActiveFont();
    STBTTF_CloseFont(font);
    SDL_Quit();
}

void SDLScreen::Clear() {
    SDL_SetRenderTarget(sdlRenderer, nullptr);
    auto theme = Editor::Instance().GetTheme();
    SDLColor bgColor(theme->GetGlobalColors().GetColor("background"));
    bgColor.Use(sdlRenderer);
    SDL_RenderClear(sdlRenderer);
}

void SDLScreen::Update() {
    SDL_SetRenderTarget(sdlRenderer, nullptr);

    // MEGA TEST
//    SDLDrawContext dc = SDLDrawContext(renderer, nullptr, Rect(widthPixels, heightPixels));
//    SDL_SetRenderDrawColor(renderer, 0,0,0,0);
//    SDLColorRepository::Instance().UseBackgroundColor(renderer);
//    //SDL_SetRenderDrawColor(renderer, 46, 54, 62, 255);
//    SDL_RenderClear(renderer);
//    SDL_SetRenderDrawColor(renderer, 255,255,255, 255);
//    dc.DrawStringAt(0,0,"01 23 45 67 89 01 23 45 67 8901234567890123456789012345678901234567890123456789");
//    dc.DrawStringAt(1,1,"01 23 45 67 89 01 23 45 67 8901234567890123456789012345678901234567890123456789");
//    dc.DrawStringAt(2,2,"01 23 45 67 89 01 23 45 67 8901234567890123456789012345678901234567890123456789");
//    dc.DrawStringAt(3,3,"01 23 45 67 89 01 23 45 67 8901234567890123456789012345678901234567890123456789");
//    dc.DrawStringAt(4,4,"01 23 45 67 89 01 23 45 67 8901234567890123456789012345678901234567890123456789");

//    auto font = SDLFontManager::Instance().GetActiveFont();
//    STBTTF_RenderText(renderer, font, 0, font->size * 2, "0123456789012345678901234567890123456789012345678901234567890123456789");
//    STBTTF_RenderText(renderer, font, 1, font->size * 3, "0123456789012345678901234567890123456789012345678901234567890123456789");
//    STBTTF_RenderText(renderer, font, 2, font->size * 4, "0123456789012345678901234567890123456789012345678901234567890123456789");

    SDLCursor::Instance().Draw();
    SDL_RenderPresent(sdlRenderer);
    SDL_RenderReadPixels(sdlRenderer, nullptr, sdlScreenAsSurface->format->format, sdlScreenAsSurface->pixels, sdlScreenAsSurface->pitch);

    // Not quite sure what this is supposed to do...
    // Most SDL example has a small delay - assume they just want 'yield' in order to avoid 100% CPU usage...
    SDL_Delay(1000/60);
}

void SDLScreen::CopyToTexture() {
    SDL_UpdateTexture(sdlScreenAsTexture, nullptr, sdlScreenAsSurface->pixels, sdlScreenAsSurface->pitch);
}
void SDLScreen::ClearWithTexture() {
    Clear();
    SDL_SetRenderTarget(sdlRenderer, nullptr);
    SDL_RenderCopy(sdlRenderer, sdlScreenAsTexture, nullptr, nullptr);
}

void SDLScreen::BeginRefreshCycle() {
    SDL_SetRenderTarget(sdlRenderer, nullptr);
}

void SDLScreen::EndRefreshCycle() {

}

WindowBase *SDLScreen::CreateWindow(const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) {
    auto window = new SDLWindow(rect);
    window->renderer = sdlRenderer;
    window->Initialize(flags, decoFlags);
    return window;
}

WindowBase *SDLScreen::UpdateWindow(WindowBase *window, const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) {
    auto sdlWindowPtr = static_cast<SDLWindow *>(window);
    sdlWindowPtr->Update(rect, flags, decoFlags);
    return window;
}

gedit::Rect SDLScreen::Dimensions() {
    Rect rect(cols, rows);
    return rect;
}
