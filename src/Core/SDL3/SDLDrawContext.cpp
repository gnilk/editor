//
// Created by gnilk on 29.03.23.
//

#include "SDLDrawContext.h"
#include "Core/ColorRGBA.h"
#include "Core/Config/Config.h"
#include "SDLColor.h"
#include "SDLTranslate.h"
#include "SDLFontManager.h"

#include "logger.h"

using namespace gedit;

//
// Consider having a special function to clear the area
//
void SDLDrawContext::Clear() const {
    // TEST
    auto &rect = GetRect();
    SDL_SetRenderTarget(renderer, renderTarget);
    //SDLColorRepository::Instance().UseBackgroundColor(renderer, 196);
    SDLColor bgCol(bgColor);
    bgCol.Use(renderer, 196);
    FillRect(0,0, rect.Width(), rect.Height(), true);
    // END TEST
}

void SDLDrawContext::ClearLine(int y) const {
    // Not needed
}

void SDLDrawContext::FillLine(int y, kTextAttributes attrib, char c)  const {

    if (attrib & kTextAttributes::kInverted) {
        SDLColor(fgColor).Use(renderer);
    } else {
        SDLColor(bgColor).Use(renderer);
    }

    FillRect(0,y,rect.Width(),1, true);
}

void SDLDrawContext::Scroll(int nRows) const {
    // Not needed
}

std::pair<float, float> SDLDrawContext::CoordsToScreen(float x, float y) const {
    auto pixWinOfs = SDLTranslate::RowColToPixel(rect.TopLeft());

    float screenXPos = SDLTranslate::ColToXPos(x) + pixWinOfs.x;
    float screenYPos = SDLTranslate::RowToYPos(y) + pixWinOfs.y;

    return {screenXPos, screenYPos};
}

void SDLDrawContext::FillRect(float x, float y, float w, float h, bool isColorSet) const {
    auto [pixXStart, pixYStart] = CoordsToScreen(x, y);

    auto pixWidth = SDLTranslate::ColToXPos(w);
    auto pixHeight = SDLTranslate::RowToYPos(h);

    if (!isColorSet) {
        SetRenderColor();
    }

    SDL_FRect rect = {pixXStart, pixYStart, pixWidth, pixHeight};
    SDL_RenderFillRect(renderer, &rect);
}

void SDLDrawContext::DrawLine(float x1, float y1, float x2, float y2) const {
    auto [px1, py1] = CoordsToScreen(x1, y1);
    auto [px2, py2] = CoordsToScreen(x2, y2);

    SetRenderColor();
    SDL_RenderLine(renderer, px1, py1, px2, py2);
}

void SDLDrawContext::DrawLineWithPixelOffset(float x1, float y1, float x2, float y2, float ofsX, float ofsY) const {
    auto [px1, py1] = CoordsToScreen(x1, y1);
    auto [px2, py2] = CoordsToScreen(x2, y2);

    SetRenderColor();
    SDL_RenderLine(renderer, px1 + ofsX, py1 + ofsY, px2 + ofsX, py2 + ofsY);
}

void SDLDrawContext::DrawLineOverlays(int y) const {

    if (!overlay.isActive) {
        return;
    }
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

    SetRenderColor();
    FillRect(start, y, end - start, 1, true);
}


//
// ALL CHARS ARE DRAWN BOTTOM UP
// YPOS means the LOWER scanline of the text-texture
//
void SDLDrawContext::DrawStringAt(int x, int y, const char *str) const {
    auto font = SDLFontManager::Instance().GetActiveFont();

    SDL_SetRenderTarget(renderer, renderTarget);
    SetRenderColor();
    auto [px, py] = CoordsToScreen(x, y);
    STBTTF_RenderText(renderer, font, px, py + font->baseline, str);
}

void SDLDrawContext::DrawStringWithAttributesAndColAt(int x, int y, kTextAttributes attrib, int idxColor, const char *str) const {
    // This should not be used...
    exit(1);
}

void SDLDrawContext::DrawStringWithAttributesAt(int x, int y, kTextAttributes attrib, const char *str) const {
    auto font = SDLFontManager::Instance().GetActiveFont();
    SDL_SetRenderTarget(renderer, renderTarget);


    // If we are inverted, flip the useage of color (Note: I know this can be a oneliner - perhaps in this case it would ease readability)
    if (attrib & kTextAttributes::kInverted) {
        SDLColor(fgColor).Use(renderer);
    } else {
        SDLColor(bgColor).Use(renderer);
    }
    // Fill the background, tell the fill-rect colors are already set...
    FillRect(x, y, strlen(str), 1, true);

    // Now set the render colors
    SetRenderColor(attrib);

    // Translate coordinates and draw text...
    auto [px, py] = CoordsToScreen(x, y);
    STBTTF_RenderText(renderer, font, px, py + font->baseline, str);

    // underlined???  draw a line under the text
    if (attrib & kTextAttributes::kUnderline) {
        auto margin = Config::Instance()["sdl"].GetInt("text_underline_margin",2);
        if (margin > Config::Instance()["sdl"].GetInt("line_margin",4)) {
            margin = Config::Instance()["sdl"].GetInt("line_margin",4) - 1;
        }
        DrawLineWithPixelOffset(x, y , x + strlen(str), y,0,font->baseline+margin);
    }
}


void SDLDrawContext::SetRenderColor() const {
    SDLColor fgCol(fgColor);
    fgCol.Use(renderer);
}

void SDLDrawContext::SetRenderColor(kTextAttributes attrib) const {
    SDLColor col(fgColor);
    if (attrib & kTextAttributes::kInverted) {
        col = SDLColor(bgColor);
    }
    col.Use(renderer);

}
