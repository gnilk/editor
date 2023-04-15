//
// Created by gnilk on 29.03.23.
//

#include "SDLDrawContext.h"
#include "Core/ColorRGBA.h"
#include "Core/Config/Config.h"
#include "SDLColor.h"
#include "SDLTranslate.h"
#include "SDLFontManager.h"
#include "SDLColorRepository.h"

#include "logger.h"

using namespace gedit;

void SDLDrawContext::Clear() const {

}

void SDLDrawContext::ClearLine(int y) const {

}

void SDLDrawContext::FillLine(int y, kTextAttributes attrib, char c)  const {
    auto [fg, bg] = SDLColorRepository::Instance().GetColor(0);
    if (attrib & kTextAttributes::kInverted) {
        fg.Use(renderer);
    } else {
        bg.Use(renderer);
    }
    FillRect(0,y,rect.Width(),1);
}

void SDLDrawContext::Scroll(int nRows) const {

}

std::pair<float, float> SDLDrawContext::CoordsToScreen(float x, float y) const {
    auto pixWinOfs = SDLTranslate::RowColToPixel(rect.TopLeft());

    float screenXPos = SDLTranslate::ColToXPos(x) + pixWinOfs.x;
    float screenYPos = SDLTranslate::RowToYPos(y) + pixWinOfs.y;

    return {screenXPos, screenYPos};
}

void SDLDrawContext::FillRect(float x, float y, float w, float h) const {
    auto [pixXStart, pixYStart] = CoordsToScreen(x, y);

    auto pixWidth = SDLTranslate::ColToXPos(w);
    auto pixHeight = SDLTranslate::RowToYPos(h);

    SDL_FRect rect = {pixXStart, pixYStart, pixWidth, pixHeight};
    SDL_RenderFillRectF(renderer, &rect);
}

void SDLDrawContext::DrawLine(float x1, float y1, float x2, float y2) const {
    auto [px1, py1] = CoordsToScreen(x1, y1);
    auto [px2, py2] = CoordsToScreen(x2, y2);

    SDL_RenderDrawLineF(renderer, px1, py1, px2, py2);
}

void SDLDrawContext::DrawLineWithPixelOffset(float x1, float y1, float x2, float y2, float ofsX, float ofsY) const {
    auto [px1, py1] = CoordsToScreen(x1, y1);
    auto [px2, py2] = CoordsToScreen(x2, y2);

    SDL_RenderDrawLineF(renderer, px1 + ofsX, py1 + ofsY, px2 + ofsX, py2 + ofsY);
}


void SDLDrawContext::DrawLineOverlays(int y) const {

    if (!overlay.isActive) return;
    if (!overlay.IsLinePartiallyCovered(y)) {
        return;
    }

    // Assume fully covered line...
    int start = 0;
    int end = GetRect().Width();
    // If only partially covered, take start/end column values depending on which line we are rendering
    if(overlay.IsLinePartiallyCovered(y)) {
        if (y == overlay.start.y) start = overlay.start.x;
        if (y == overlay.end.y) end = overlay.end.x;
    }

    SDL_SetRenderDrawColor(renderer, 80, 100, 128, 64);
    FillRect(start, y, end, 1);

}


//
// ALL STRINGS ARE DRAWN BOTTOM UP
// YPOS means the LOWER scanline of the text-texture
//
void SDLDrawContext::DrawStringAt(int x, int y, const char *str) const {
    auto font = SDLFontManager::Instance().GetActiveFont();

    SDL_SetRenderTarget(renderer, renderTarget);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    auto [px, py] = CoordsToScreen(x, y);

    STBTTF_RenderText(renderer, font, px, py + font->baseline, str);
}

// Note: 'Blink' is NOT supported
void SDLDrawContext::DrawStringWithAttributesAndColAt(int x, int y, kTextAttributes attrib, int idxColor, const char *str) const {
    auto font = SDLFontManager::Instance().GetActiveFont();
    SDL_SetRenderTarget(renderer, renderTarget);

    // Get colors
    auto [fg, bg] = SDLColorRepository::Instance().GetColor(idxColor);

    // If we are inverted, flip the useage of color (Note: I know this can be a oneliner - perhaps in this case it would ease readability)
    if (attrib & kTextAttributes::kInverted) {
        fg.Use(renderer);
    } else {
        bg.Use(renderer);
    }
    // Fill the background...
    // SDL_SetRenderDrawColor(renderer,0,255,0,255);
    FillRect(x, y, strlen(str), 1);

    // Change color to use depending on inverted or not
    if (attrib & kTextAttributes::kInverted) {
        bg.Use(renderer);
    } else {
        fg.Use(renderer);
    }

    // Translate coordinates and draw text...
    auto [px, py] = CoordsToScreen(x, y);
    STBTTF_RenderText(renderer, font, px, py + font->baseline, str);

    // underlined???  draw a line under the text
    if (attrib & kTextAttributes::kUnderline) {
        // FIXME: Cache this value...
        auto margin = Config::Instance()["sdl3"].GetInt("text_underline_margin",2);
        if (margin > Config::Instance()["sdl3"].GetInt("line_margin",4)) {
            margin = Config::Instance()["sdl3"].GetInt("line_margin",4) - 1;
        }
        DrawLineWithPixelOffset(x, y , x + strlen(str), y,0,font->baseline+margin);
    }
}

void SDLDrawContext::DrawStringWithAttributesAt(int x, int y, kTextAttributes attrib, const char *str) const {
    DrawStringWithAttributesAndColAt(x,y, attrib, 0, str);
}