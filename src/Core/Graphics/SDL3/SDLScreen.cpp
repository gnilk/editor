//
// Created by gnilk on 29.03.23.
//
// We keep everything in editor coordinates (character row/col) until we must communicate with SDL
// Only the native window dimensions are stored in pixel coordinates (might change) - but for now that's how it is...
//
// Note: There are two code-paths available - one using textures and one using primary render target directly..
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
#include "Core/Editor.h"

#include <SDL3/SDL.h>

#ifndef STBTTF_IMPLEMENTATION
#define STBTTF_IMPLEMENTATION
#endif
#include "ext/stbttf.h"

// Implementation lives in src/ext/stb/stb_impl.cpp — header only here.
#include "stb_image.h"


using namespace gedit;
using namespace gedit::SDL3;

#define WIDTH 1920
#define HEIGHT 1080


ScreenBase::Ref SDLScreen::Create() {
    auto instance = std::make_shared<SDLScreen>();
    return instance;
}


bool SDLScreen::Open() {

    logger = gnilk::Logger::GetLogger("SDL3Screen");
    logger->Debug("Opening window");
    logger->Debug("SDL version: %s", SDL_GetRevision());

    int nDrivers = SDL_GetNumVideoDrivers();
    logger->Debug("Available Video Drivers (%d):", nDrivers);
    for (int i = 0; i < nDrivers; i++) {
        logger->Debug("  %d:%s", i, SDL_GetVideoDriver(i));
    }

#if defined(GEDIT_MACOS)
    // Map the LEFT Option key to Alt so UI-navigation shortcuts (UINavigationModifier
    // is Alt = Option) deliver a real SDL_EVENT_KEY_DOWN. Without this, Cocoa dead-key
    // composition eats the keydown for composing Option+<letter> combos (e.g. Option+E
    // '´', Option+G '©' on some layouts) and only emits SDL_EVENT_TEXT_INPUT, so those
    // shortcuts silently never fire. RIGHT Option is left untouched so AltGr-style
    // composition ([ ] { } and accents) still works. Must be set before SDL_Init().
    SDL_SetHint(SDL_HINT_MAC_OPTION_AS_ALT, "only_left");
#endif

    // SDL3: SDL_INIT_HAPTIC was removed; SDL_INIT_EVENTS is implied by SDL_INIT_VIDEO
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        logger->Error("SDL_Init, %s", SDL_GetError());
        exit(1);
    }

    logger->Debug("SDL initialized ok, video driver = %s", SDL_GetCurrentVideoDriver());

    // SDL3: SDL_WINDOW_ALLOW_HIGHDPI → SDL_WINDOW_HIGH_PIXEL_DENSITY
    SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    auto sdlBackend = Config::Instance()["sdl"].GetStr("backend", "opengl");
    if (sdlBackend == "opengl") {
        logger->Debug("Using backend: '%s'", sdlBackend.c_str());
        windowFlags |= SDL_WINDOW_OPENGL;
    } else if (sdlBackend == "metal") {
        logger->Debug("Using backend: '%s'", sdlBackend.c_str());
        windowFlags |= SDL_WINDOW_METAL;
    } else if (sdlBackend == "vulkan") {
        logger->Debug("Using backend: '%s'", sdlBackend.c_str());
        windowFlags |= SDL_WINDOW_VULKAN;
    } else {
        logger->Error("Unknown backend ('%s'), using default", sdlBackend.c_str());
    }

    // Resolve startup geometry: the glue layer fed the requested (session) geometry in before Open();
    // the base class turns it into a sane on-screen rect (default + clamp using the display).
    int geoX = 0, geoY = 0;
    ResolveStartupGeometry(geoX, geoY, widthPixels, heightPixels);

    // SDL3: SDL_CreateWindow no longer takes x/y position
    window = SDL_CreateWindow("goatedit", widthPixels, heightPixels, windowFlags);
    if (!window) {
        logger->Error("SDL_CreateWindow failed: %s", SDL_GetError());
        exit(1);
    }
    SDL_SetWindowPosition(window, geoX, geoY);
    SetWindowIcon();

    // Enable the screensaver - SDL blocks it by default - but in this case it makes sense..
    if (!SDL_ScreenSaverEnabled()) {
        logger->Warning("Screenserver disable, enabling again");
        SDL_EnableScreenSaver();
    }

    // SDL3: SDL_RENDERER_ACCELERATED removed (default); SDL_RENDERER_PRESENTVSYNC kept
    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        logger->Error("SDL_CreateRenderer failed: %s", SDL_GetError());
        exit(1);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderVSync(renderer, 1);

    // Report the geometry actually realised, so a window that is never moved/resized still persists its
    // position/size.
    ReportWindowGeometry();

    logger->Debug("Resolution: %d x %d", widthPixels, heightPixels);

    // SDL3: text input now requires the window pointer
    SDL_StartTextInput(window);

    LoadFontFromTheme();
    ComputeScalingFactors();
    CreateTextures();

    // Sanity check..
    if (!SDL_ScreenSaverEnabled()) {
        logger->Error("Sanity check, end of SDL3 Initialization - screenserver still disabled, enabling again");
        SDL_EnableScreenSaver();
    }

    return true;
}

void SDLScreen::SetWindowIcon() {
    if (window == nullptr) {
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
    // SDL3: SDL_CreateSurfaceFrom(w, h, format, pixels, pitch); free with SDL_DestroySurface.
    SDL_Surface *iconSurface = SDL_CreateSurfaceFrom(
        width, height, SDL_PIXELFORMAT_RGBA32, pixels, 4 * width);
    if (iconSurface != nullptr) {
        SDL_SetWindowIcon(window, iconSurface);
        SDL_DestroySurface(iconSurface);
    } else {
        logger->Error("SDL_CreateSurfaceFrom failed: %s", SDL_GetError());
    }
    stbi_image_free(pixels);
}

void SDLScreen::LoadFontFromTheme() {
    auto currentTheme = Editor::Instance().GetTheme();
    if (currentTheme == nullptr) {
        logger->Error("Theme not loaded!!!!");
        return;
    }
    auto fontName = currentTheme->GetStr("font", "Andale Mono.ttf");
    logger->Debug("Loading font: '%s'", fontName.c_str());

    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    auto fontAsset = assetLoader.LoadAsset(fontName);
    if (fontAsset == nullptr) {
        logger->Error("Unable to open font: '%s'\n", fontName.c_str());
        return;
    }
    // SDL3: SDL_RWFromConstMem → SDL_IOFromConstMem
    auto sdlIO = SDL_IOFromConstMem(fontAsset->GetPtrAs<const void *>(), (size_t)fontAsset->GetSize());
    auto font = STBTTF_OpenFontRW(renderer, sdlIO, 18);

    if (font == nullptr) {
        logger->Error("Unable to load font: '%s'\n", fontName.c_str());
        return;
    }

    SDLFontManager::Instance().SetActiveFont(font);
}

// Called also from resize.
void SDLScreen::ComputeScalingFactors() {

    SDL_GetWindowSize(window, &widthPixels, &heightPixels);

    auto displayId = SDL_GetDisplayForWindow(window);
    auto displayMode = SDL_GetDesktopDisplayMode(displayId);

    logger->Debug("Resolution: %d x %d", widthPixels, heightPixels);
    if (displayMode) {
        // SDL3: pixel_density replaces the old pixel_w/pixel_h and display_scale fields
        logger->Debug("Display: %d x %d, pixel_density: %f", displayMode->w, displayMode->h, displayMode->pixel_density);
    }

    auto font = SDLFontManager::Instance().GetActiveFont();

    float line_margin = Config::Instance()["sdl"].GetInt("line_margin", 4);
    rows = heightPixels / (font->baseline + line_margin);

    std::string textToMeasure = "AaWwiI109 []{}/*.,\"";
    auto textWidth = STBTTF_MeasureText(font, textToMeasure.c_str());
    auto fontWidthAverage = textWidth / (float)textToMeasure.length();
    cols = widthPixels / fontWidthAverage;

    logger->Debug("Font scaling factors:");
    logger->Debug("  Height: %d px (font: %d, line margin: %f)", (int)(font->baseline + line_margin), font->baseline, line_margin);
    logger->Debug("  Width : %d px (based on average width for '%s')", (int)fontWidthAverage, textToMeasure.c_str());
    logger->Debug("Rows=%d, Cols=%d", rows, cols);

    SDLTranslate::fac_x_to_rc = (float)cols / (float)widthPixels;
    SDLTranslate::fac_y_to_rc = (float)rows / (float)heightPixels;

    logger->Debug("Scaling factors = (%f,%f)", SDLTranslate::fac_x_to_rc, SDLTranslate::fac_y_to_rc);

    // Handle HiDPI: renderer output may be larger than window logical size
    // SDL3: SDL_GetRendererOutputSize → SDL_GetCurrentRenderOutputSize
    //       SDL_RenderSetScale → SDL_SetRenderScale
    int rw = 0, rh = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &rw, &rh);
    if (rw != widthPixels) {
        logger->Debug("HiDPI display detected, render output: %d x %d", rw, rh);
        float widthScale  = (float)rw / (float)widthPixels;
        float heightScale = (float)rh / (float)heightPixels;
        if (widthScale != heightScale) {
            logger->Debug("WARNING: width scale != height scale");
        }
        logger->Debug("Render scale: %f, %f", widthScale, heightScale);
        SDL_SetRenderScale(renderer, widthScale, heightScale);
    } else {
        logger->Debug("Non-HiDPI display, resetting render scale");
        SDL_SetRenderScale(renderer, 1.0f, 1.0f);
    }
}

void SDLScreen::CreateTextures() {

    SDL_GetWindowSize(window, &widthPixels, &heightPixels);

    if (screenAsSurface != nullptr) {
        SDL_DestroySurface(screenAsSurface);
    }
    if (screenAsTexture != nullptr) {
        SDL_DestroyTexture(screenAsTexture);
    }
    screenAsSurface = SDL_CreateSurface(widthPixels, heightPixels, SDL_PIXELFORMAT_RGBA32);
    screenAsTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, widthPixels, heightPixels);
}


void SDLScreen::OnSizeChanged() {
    logger->Debug("Size changed — recomputing scaling factors and recreating textures");
    ComputeScalingFactors();
    CreateTextures();
    ReportWindowGeometry();
    logger->Debug("ReInitialize UI!");
    RuntimeConfig::Instance().GetRootView().Resize();
    RuntimeConfig::Instance().GetRootView().InvalidateAll();
    RuntimeConfig::Instance().GetRootView().PostMessage([](){});
}

void SDLScreen::OnMoved() {
    logger->Debug("Window moved, reporting geometry to session");
    ReportWindowGeometry();
}

void SDLScreen::ReportWindowGeometry() {
    SDL_GetWindowSize(window, &widthPixels, &heightPixels);
    int winXpos, winYpos;
    SDL_GetWindowPosition(window, &winXpos, &winYpos);
    NotifyWindowGeometryChanged(winXpos, winYpos, widthPixels, heightPixels);
}

bool SDLScreen::GetPrimaryDisplayBounds(int &x, int &y, int &width, int &height) {
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    if (display == 0) {
        return false;
    }
    SDL_Rect rect;
    if (!SDL_GetDisplayUsableBounds(display, &rect)) {
        return false;
    }
    x = rect.x;
    y = rect.y;
    width = rect.w;
    height = rect.h;
    return true;
}

void SDLScreen::Close() {
    // SDL3: SDL_StopTextInput requires the window pointer
    SDL_StopTextInput(window);
    auto font = SDLFontManager::Instance().GetActiveFont();
    STBTTF_CloseFont(font);
    SDL_Quit();
}

void SDLScreen::Clear() {
    SDL_SetRenderTarget(renderer, nullptr);
    auto theme = Editor::Instance().GetTheme();
    SDLColor bgColor(theme->GetGlobalColors().GetColor("background"));
    bgColor.Use(renderer);
    SDL_RenderClear(renderer);
}

void SDLScreen::Update() {
    SDL_SetRenderTarget(renderer, nullptr);
    SDLCursor::Instance().Draw();
    SDL_RenderPresent(renderer);
    // SDL3: SDL_RenderReadPixels signature changed (returns SDL_Surface*, no pixel buffer args).
    // The Linux-only readback used in SDL2 is omitted here; add back if needed with the new API.
}

bool SDLScreen::UpdateClipboardData() {
    auto utfClipboardText = SDL_GetClipboardText();
    if (utfClipboardText == nullptr) {
        return false;
    }
    auto &myClipboard = Editor::Instance().GetClipBoard();
    auto result = myClipboard.CopyFromExternal(utfClipboardText);
    // We must free - was leaking memory here...
    SDL_free(utfClipboardText);
    return result;;
}

void SDLScreen::CopyToTexture() {
    SDL_UpdateTexture(screenAsTexture, nullptr, screenAsSurface->pixels, screenAsSurface->pitch);
}

void SDLScreen::ClearWithTexture() {
    Clear();
    SDL_SetRenderTarget(renderer, nullptr);
    SDL_RenderTexture(renderer, screenAsTexture, nullptr, nullptr);
}

void SDLScreen::BeginRefreshCycle() {
    SDL_SetRenderTarget(renderer, nullptr);
}

void SDLScreen::EndRefreshCycle() {

}

WindowBase *SDLScreen::CreateWindow(const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) {
    auto win = new SDLWindow(rect);
    win->renderer = renderer;
    win->Initialize(flags, decoFlags);
    return win;
}

WindowBase *SDLScreen::UpdateWindow(WindowBase *window, const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) {
    auto sdlWin = static_cast<SDLWindow *>(window);
    sdlWin->Update(rect, flags, decoFlags);
    return window;
}

gedit::Rect SDLScreen::Dimensions() {
    Rect rect(cols, rows);
    return rect;
}

// PollEvents is the single SDL event pump for all platforms.
// Must be called from the main thread (Cocoa/SDL requirement on macOS).
// Blocks up to ~250 ms waiting for events, then drains the queue.
// SDL3 uses flat per-window event types instead of SDL_WINDOWEVENT + sub-event.
void SDLScreen::PollEvents() {
    SDL_WaitEventTimeout(nullptr, 250);

    auto keyDriver = std::static_pointer_cast<SDLKeyboardDriver>(
        RuntimeConfig::Instance().GetKeyboard());

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                SDL_Quit();
                exit(0);

            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_TEXT_INPUT: {
                auto kp = keyDriver->ProcessEvent(event);
                if (kp.has_value() && kp->IsAnyValid()) {
                    KeyPress captured = *kp;
                    Runloop::PostMessage(0, [captured](uint32_t) {
                        Runloop::ProcessKeyPress(captured);
                    });
                }
                break;
            }

            // SDL3: window sub-events are now first-class event types
            case SDL_EVENT_WINDOW_RESIZED:
                logger->Debug("SDL_EVENT_WINDOW_RESIZED");
                OnSizeChanged();
                break;

            case SDL_EVENT_WINDOW_MOVED:
                logger->Debug("SDL_EVENT_WINDOW_MOVED");
                OnMoved();
                break;

            case SDL_EVENT_CLIPBOARD_UPDATE:
                logger->Debug("SDL_EVENT_CLIPBOARD_UPDATE");
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
