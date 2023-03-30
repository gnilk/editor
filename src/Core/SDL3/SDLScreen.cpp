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
#include "SDLColorRepository.h"
#include "SDLDrawContext.h"

#include "Core/RuntimeConfig.h"
#include "Core/Config/Config.h"

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

    auto logger = gnilk::Logger::GetLogger("SDLScreen");

    logger->Debug("Opening window");

    SDL_Init(SDL_INIT_VIDEO);
    // FIXME: restore window size!
    window = SDL_CreateWindow("gedit", widthPixels, heightPixels,  SDL_WINDOW_OPENGL);
    renderer = SDL_CreateRenderer(window, nullptr, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    logger->Debug("Resolution: %d x %d", widthPixels, heightPixels);
    logger->Debug("Loading font: '%s'", fontName.c_str());

    // FIXME: Font handling should not be here
    auto font = STBTTF_OpenFont(renderer, fontName.c_str(), 18);
    if (font == nullptr) {
        logger->Error("Unable to open font: '%s'\n", fontName.c_str());
        return -1;
    }
    SDLFontManager::Instance().SetActiveFont(font);

    if (!Config::Instance().ColorConfiguration().HasColor("background")) {
        logger->Warning("No background color found - assigning default");
        // Auto assign background here..
        backgroundColor = SDLColor(ColorRGBA::FromRGBA(46, 54, 62, 255));
    } else {
        backgroundColor = SDLColor(Config::Instance().ColorConfiguration().GetColor("background"));
    }
    // This allows access to all different rendering sub-systems (text/window/etc...)
    SDLColorRepository::Instance().SetBackgroundColor(backgroundColor);


    float line_margin = Config::Instance()["sdl3"].GetInt("line_margin", 4);
    rows = heightPixels / (font->baseline + line_margin); // baseline = font->ascent * font->scale


    // subjective representation of average type of chars you might find in a something
    // small,wide,average type of chars
    std::string textToMeasure = "AaWwiI109 []{}/*.,\"";
    auto textWidth =  STBTTF_MeasureText(font, textToMeasure.c_str());
    auto fontWidthAverage = textWidth / (float)textToMeasure.length();
    cols = widthPixels / fontWidthAverage;

    logger->Debug("Font scaling factors:");
    logger->Debug("  Height: %d px (font: %d, line margin: %f)", font->baseline + line_margin, font->baseline, line_margin);
    logger->Debug("  Width : %d px (based on average widht for '%s')", fontWidthAverage, textToMeasure.c_str());


    logger->Debug("Text to Graphics defined as");
    logger->Debug("Rows=%d, Cols=%d", rows, cols);


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
    backgroundColor.Use(renderer);
    //SDL_SetRenderDrawColor(renderer, 46, 54, 62, 255);
    SDL_RenderClear(renderer);
}


void SDLScreen::Update() {
    SDL_SetRenderTarget(renderer, nullptr);

    // MEGA TEST
//    SDLDrawContext dc = SDLDrawContext(renderer, nullptr, Rect(widthPixels, heightPixels));
//    SDL_SetRenderDrawColor(renderer, 0,0,0,0);
//    backgroundColor.Use(renderer);
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
    SDL_RenderPresent(renderer);

    //
    // TODO: Move the event loop out from here
    //
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EventType::SDL_EVENT_QUIT) {
            SDL_Quit();
            exit(0);
        }
        if ((event.type == SDL_EventType::SDL_EVENT_KEY_DOWN) && (event.key.keysym.sym == SDLK_ESCAPE)) {
            SDL_Quit();
            exit(1);
            //bQuit = true;
            continue;
        }
    }
    SDL_Delay(1000/60);
    //
    // End of event loop
    //


}

void SDLScreen::RegisterColor(int appIndex, const ColorRGBA &foreground, const ColorRGBA &background) {
    SDLColorRepository::Instance().RegisterColor(appIndex, foreground, background);
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
    sdlWindow->Update(rect, flags, decoFlags);
    return window;
}

Rect SDLScreen::Dimensions() {
    Rect rect(cols, rows);
    return rect;
}
