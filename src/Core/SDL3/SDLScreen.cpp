//
// Created by gnilk on 29.03.23.
//
// We keep everything in editor coordinates (character row/col) until we must communicate with SDL
// Only the native window dimensions are stored in pixel coordinates (might change) - but for now that's how it is...
//

#include "SDLScreen.h"
#include "SDLWindow.h"
#include "SDLTranslate.h"
#include "SDLFontManager.h"

#include <SDL3/SDL.h>

#define STB_RECT_PACK_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "ext/stb_rect_pack.h"
#include "ext/stb_truetype.h"

#define STBTTF_IMPLEMENTATION
#include "ext/stbttf.h"


using namespace gedit;

#define WIDTH 1920
#define HEIGHT 1080

static const std::string fontName = "Andale Mono.ttf";

bool SDLScreen::Open() {
    SDL_Init(SDL_INIT_VIDEO);
    // FIXME: restore window size!
    window = SDL_CreateWindow("gedit", widthPixels, heightPixels,  SDL_WINDOW_OPENGL);
    renderer = SDL_CreateRenderer(window, nullptr, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // FIXME: Font handling should not be here
    auto font = STBTTF_OpenFont(renderer, fontName.c_str(), 18);
    if (font == nullptr) {
        printf("Unable to open font: '%s'\n", fontName.c_str());
        return -1;
    }
    SDLFontManager::Instance().SetActiveFont(font);


    rows = heightPixels / font->size;
    cols = widthPixels / font->size;

    // Setup translation
    SDLTranslate::fac_x_to_rc = (float)cols / (float)widthPixels;
    SDLTranslate::fac_y_to_rc = (float)rows / (float)heightPixels;


    return true;
}

void SDLScreen::Close() {
    auto font = SDLFontManager::Instance().GetActiveFont();
    STBTTF_CloseFont(font);
    SDL_Quit();
}

void SDLScreen::Clear() {
    SDL_SetRenderTarget(renderer, nullptr);
    // FIXME: This is not correct
    SDL_SetRenderDrawColor(renderer, 46, 54, 62, 255);
    SDL_RenderClear(renderer);
}

void SDLScreen::Update() {
    SDL_SetRenderTarget(renderer, nullptr);
    auto font = SDLFontManager::Instance().GetActiveFont();
    STBTTF_RenderText(renderer, font, 0, font->size * 10, "0123456789012345678901234567890123456789012345678901234567890123456789");

    SDL_RenderPresent(renderer);
}

void SDLScreen::RegisterColor(int appIndex, const ColorRGBA &foreground, const ColorRGBA &background) {

}

void SDLScreen::BeginRefreshCycle() {
    SDL_SetRenderTarget(renderer, nullptr);
}

void SDLScreen::EndRefreshCycle() {

}

WindowBase *SDLScreen::CreateWindow(const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) {
    auto window = new SDLWindow(rect);
    window->renderer = renderer;
    window->Initialize(flags, decoFlags);
    return window;
}

WindowBase *SDLScreen::UpdateWindow(WindowBase *window, const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) {
    auto sdlWindow = static_cast<SDLWindow *>(window);
    //sdlWindow->Update(rect, flags, decoFlags);
    return nullptr;
}

Rect SDLScreen::Dimensions() {
    Rect rect(cols, rows);
    return rect;
}
