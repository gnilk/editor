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
#include "SDLKeyboardDriver.h"

#include "Core/RuntimeConfig.h"
#include "Core/Config/Config.h"
#include "Core/Runloop.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include "Core/Editor.h"

#ifndef STBTTF_IMPLEMENTATION
#define STBTTF_IMPLEMENTATION
#endif
#include "ext/stbttf.h"

// Implementation lives in src/ext/stb/stb_impl.cpp — header only here.
#include "stb_image.h"


using namespace gedit;
using namespace gedit::SDL2;

#define WIDTH 1920
#define HEIGHT 1080


//static const std::string fontName = "Andale Mono.ttf";

ScreenBase::Ref SDLScreen::Create() {
    auto instance = std::make_shared<SDLScreen>();
    return instance;
}


bool SDLScreen::Open() {

    logger = gnilk::Logger::GetLogger("SDL2Screen");

    logger->Debug("Opening window");
    SDL_version sdlVersion = {};
    SDL_GetVersion(&sdlVersion);
    logger->Debug("SDL Version: %d.%d.%d", sdlVersion.major, sdlVersion.minor, sdlVersion.patch);

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

    int windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;

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

    // Resolve startup geometry: the glue layer fed the requested (session) geometry in before Open();
    // the base class turns it into a sane on-screen rect (default + clamp using the display).
    int geoX = 0, geoY = 0;
    ResolveStartupGeometry(geoX, geoY, widthPixels, heightPixels);

    // FIXME: Need to determine how HighDPI stuff works...
    sdlWindow = SDL_CreateWindow("gedit", geoX, geoY, widthPixels, heightPixels,  windowFlags);
    SetWindowIcon();
    sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);

    // Report the geometry actually realised, so a window that is never moved/resized still persists its
    // position/size.
    ReportWindowGeometry();

    logger->Debug("Resolution: %d x %d", widthPixels, heightPixels);

    LoadFontFromTheme();
    ComputeScalingFactors();
    CreateTextures();
    return true;
}

void SDLScreen::SetWindowIcon() {
    if (sdlWindow == nullptr) {
        return;
    }
    // The icon ships as a regular asset (appicon.png) so it is found through the normal search
    // paths (dev resources/, installed share/goatedit, or the AppImage bundle).
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    auto iconAsset = assetLoader.LoadAsset("appicon.png");
    if (iconAsset == nullptr) {
        logger->Warning("App icon 'appicon.png' not found - window will have no icon");
        return;
    }

    int width = 0, height = 0, channels = 0;
    unsigned char *pixels = stbi_load_from_memory(
        iconAsset->GetPtrAs<const stbi_uc *>(), static_cast<int>(iconAsset->GetSize()),
        &width, &height, &channels, 4);   // force RGBA
    if (pixels == nullptr) {
        logger->Error("Failed to decode app icon: %s", stbi_failure_reason());
        return;
    }

    // stb returns tightly-packed R,G,B,A bytes; SDL_PIXELFORMAT_RGBA32 matches that byte order
    // on both endiannesses. SDL copies the pixels into the WM, so we free them right after.
    SDL_Surface *iconSurface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels, width, height, 32, 4 * width, SDL_PIXELFORMAT_RGBA32);
    if (iconSurface != nullptr) {
        SDL_SetWindowIcon(sdlWindow, iconSurface);
        SDL_FreeSurface(iconSurface);
    } else {
        logger->Error("SDL_CreateRGBSurfaceWithFormatFrom failed: %s", SDL_GetError());
    }
    stbi_image_free(pixels);
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
// Should perhaps take a look at this: https://discourse.libsdl.org/t/high-dpi-mode/34411
void SDLScreen::ComputeScalingFactors() {

    auto displayId = SDL_GetWindowDisplayIndex(sdlWindow);
    SDL_DisplayMode displayMode;
    SDL_GetDesktopDisplayMode(displayId, &displayMode);
    SDL_GetWindowSize(sdlWindow, &widthPixels, &heightPixels);
    float ddpi, vdpi, hdpi;
    SDL_GetDisplayDPI(displayId, &ddpi, &hdpi, &vdpi);

    //
    // The following is a bit convoluted but...
    // primary reason is to calculate rows/cols as the rest of the UI is driven by it..
    //
    logger->Debug("Resolution: %d x %d", widthPixels, heightPixels);
    logger->Debug("Display, pixels: %d x %d", displayMode.w, displayMode.h);

    auto font = SDLFontManager::Instance().GetActiveFont();

    float line_margin = Config::Instance()["sdl"].GetInt("line_margin", 4);
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
    logger->Debug("Rows=%d, Cols=%d", rows, cols);

    // Setup translation
    SDLTranslate::fac_x_to_rc = (float)cols / (float)(widthPixels);
    SDLTranslate::fac_y_to_rc = (float)rows / (float)(heightPixels);

    logger->Debug("Scaling factors = (%f,%f)", SDLTranslate::fac_x_to_rc, SDLTranslate::fac_y_to_rc);

    // This should make it possible to move the window between HDPI screens and regular screens
    // or if you just use a high-dpi screen...
    int rw = 0, rh = 0;
    SDL_GetRendererOutputSize(sdlRenderer, &rw, &rh);
    if(rw != widthPixels) {
        logger->Debug("Renderer Output != widthPixels - assuming high DPI display");
        float widthScale = (float)rw / (float) widthPixels;
        float heightScale = (float)rh / (float) heightPixels;

        if(widthScale != heightScale) {
            logger->Debug("WARNING: width scale != height scale");
        }
        logger->Debug("Scaling factors: %f, %f", widthScale, heightScale);
        SDL_RenderSetScale(sdlRenderer, widthScale, heightScale);
    } else {
        logger->Debug("Non HDPI display, resetting scaling factors");
        SDL_RenderSetScale(sdlRenderer, 1,1);
    }
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
    ReportWindowGeometry();
    logger->Debug("ReInitialize UI!");
    RuntimeConfig::Instance().GetRootView().Resize();
    RuntimeConfig::Instance().GetRootView().InvalidateAll();

    // This a mechanism we can use to trigger a redraw in the main run-loop..
    // Just post a message, but we don't care about the callback - so an empty lambda..
    RuntimeConfig::Instance().GetRootView().PostMessage([](){});
}

void SDLScreen::OnMoved() {
    logger->Debug("Window moved, reporting geometry to session");
    ReportWindowGeometry();
}

void SDLScreen::ReportWindowGeometry() {
    SDL_GetWindowSize(sdlWindow, &widthPixels, &heightPixels);
    int winXpos, winYpos;
    SDL_GetWindowPosition(sdlWindow, &winXpos, &winYpos);
    NotifyWindowGeometryChanged(winXpos, winYpos, widthPixels, heightPixels);
}

bool SDLScreen::GetPrimaryDisplayBounds(int &x, int &y, int &width, int &height) {
    SDL_Rect rect;
    if (SDL_GetDisplayUsableBounds(0, &rect) != 0) {
        return false;
    }
    x = rect.x;
    y = rect.y;
    width = rect.w;
    height = rect.h;
    return true;
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
    // Enable this IF the keyboard emulator is enabled on macos!!!
    // SDL_Event event;
    // SDL_PollEvent(&event);
    // TMP TMP TMP..



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


#ifdef GEDIT_LINUX
    // This is horribly slow on macos - now quite sure why I do it..
    SDL_RenderReadPixels(sdlRenderer, nullptr, sdlScreenAsSurface->format->format, sdlScreenAsSurface->pixels, sdlScreenAsSurface->pitch);
#endif
    // Not quite sure what this is supposed to do...
    // Most SDL example has a small delay - assume they just want 'yield' in order to avoid 100% CPU usage...
    // Verify if this delay should be preset on Linux...
    //SDL_Delay(1000/60);
}

bool SDLScreen::UpdateClipboardData() {
    auto utfClipboardText = SDL_GetClipboardText();
    if (utfClipboardText == nullptr) {
        return false;
    }

    auto &myClipboard = Editor::Instance().GetClipBoard();
    auto result = myClipboard.CopyFromExternal(utfClipboardText);

    // We need to free the clip board data
    SDL_free(utfClipboardText);
    return result;

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

// PollEvents is the single SDL event pump for all platforms.
// It must be called from the main thread (Cocoa/SDL requirement on macOS).
// Blocks up to ~250 ms waiting for events, then drains the queue.
// Keyboard events are translated by SDLKeyboardDriver and posted to the Runloop.
// Window and clipboard events are handled directly here.
void SDLScreen::PollEvents() {
    SDL_WaitEventTimeout(nullptr, 250);

    auto keyDriver = std::static_pointer_cast<SDLKeyboardDriver>(
        RuntimeConfig::Instance().GetKeyboard());

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                SDL_Quit();
                exit(0);

            case SDL_KEYDOWN:
            case SDL_TEXTINPUT: {
                auto kp = keyDriver->ProcessEvent(event);
                if (kp.has_value() && kp->IsAnyValid()) {
                    KeyPress captured = *kp;
                    Runloop::PostMessage(0, [captured](uint32_t) {
                        Runloop::ProcessKeyPress(captured);
                    });
                }
                break;
            }

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    logger->Debug("SDL_EVENT_WINDOW_RESIZED");
                    OnSizeChanged();
                } else if (event.window.event == SDL_WINDOWEVENT_MOVED) {
                    logger->Debug("SDL_EVENT_WINDOW_MOVED");
                    OnMoved();
                }
                break;

            case SDL_CLIPBOARDUPDATE:
                logger->Debug("SDL_EVENT_CLIPBOARDUPDATE!!!");
                if (SDL_HasClipboardText()) {
                    auto clipBoardText = SDL_GetClipboardText();
                    Editor::Instance().GetClipBoard().CopyFromExternal(clipBoardText);
                    SDL_free(clipBoardText);
                }
                break;

            default:
                break;
        }
    }
}
