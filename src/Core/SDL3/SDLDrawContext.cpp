//
// Created by gnilk on 29.03.23.
//

#include "SDLDrawContext.h"
#include "Core/ColorRGBA.h"
#include "SDLColor.h"
#include "SDLTranslate.h"
#include "SDLFontManager.h"
#include "SDLColorRepository.h"

using namespace gedit;

void SDLDrawContext::Clear() {

}

void SDLDrawContext::DrawLine(Line *line, int idxLine) {
    // REMOVE ME!
}

void SDLDrawContext::ClearLine(int y) {

}

void SDLDrawContext::FillLine(int y, kTextAttributes attrib, char c) {
    Rect pixRect = SDLTranslate::RowColToPixel(rect);
    float pixYStart = SDLTranslate::RowToYPos(y);
    float pixYEnd = SDLTranslate::RowToYPos(y+1);
    SDL_FRect rect = {(float)pixRect.TopLeft().x, pixYStart, (float)pixRect.Width(),pixYEnd - pixYStart};

    SDL_SetRenderTarget(renderer, renderTarget);

    auto [fg, bg] = SDLColorRepository::Instance().GetColor(0);
    if (attrib & kTextAttributes::kInverted) {
        fg.Use(renderer);
    } else {
        bg.Use(renderer);
    }

    SDL_RenderFillRect(renderer, &rect);

}

void SDLDrawContext::Scroll(int nRows) {

}

std::pair<float, float>SDLDrawContext::CoordsToScreen(float x, float y) {
    auto pixWinOfs = SDLTranslate::RowColToPixel(rect.TopLeft());

    float screenXPos = SDLTranslate::ColToXPos(x) + pixWinOfs.x;
    float screenYPos = SDLTranslate::RowToYPos(y) + pixWinOfs.y;

    return {screenXPos, screenYPos};

}

void SDLDrawContext::FillRect(float x, float y, float w, float h) {
    auto [pixXStart, pixYStart] = CoordsToScreen(x,y);
    auto [pixWidth, pixHeight] = CoordsToScreen(w,h);

    SDL_FRect rect = {pixXStart, pixYStart, pixWidth, pixHeight};
    SDL_RenderFillRect(renderer, &rect);
}

void SDLDrawContext::DrawLine(float x1, float y1, float x2, float y2) {
    auto [px1, py1] = CoordsToScreen(x1,y1);
    auto [px2, py2] = CoordsToScreen(x2,y2);

    SDL_RenderLine(renderer, px1, py1, px2, py2);
}

//
// ALL STRINGS ARE DRAWN BOTTOM UP
// YPOS means the LOWER scanline of the text-texture
//

void SDLDrawContext::DrawStringAt(int x, int y, const char *str) {
    auto font = SDLFontManager::Instance().GetActiveFont();

    SDL_SetRenderTarget(renderer, renderTarget);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255);

    auto [px, py] = CoordsToScreen(x,y);

    STBTTF_RenderText(renderer, font, px, py + font->baseline , str);
}


// TODO: Refactor this - rect should be encapsulated, etc..
// Note: 'Blink' is NOT supported
void SDLDrawContext::DrawStringWithAttributesAt(int x, int y, kTextAttributes attrib, const char *str) {
    auto font = SDLFontManager::Instance().GetActiveFont();
    SDL_SetRenderTarget(renderer, renderTarget);

    // Get colors
    auto [fg, bg] = SDLColorRepository::Instance().GetColor(0);

    // If we are inverted, flip the useage of color (Note: I know this can be a oneliner - perhaps in this case it would ease readability)
    if (attrib & kTextAttributes::kInverted) {
        fg.Use(renderer);
    } else {
        bg.Use(renderer);
    }
    // Fill the background...
    FillRect(x,y,strlen(str),y+1);

    // Change color to use depending on inverted or not
    if (attrib & kTextAttributes::kInverted) {
        bg.Use(renderer);
    } else {
        fg.Use(renderer);
    }

    // Translate coordinates and draw text...
    auto[px, py] = CoordsToScreen(x,y);
    STBTTF_RenderText(renderer, font, px, py + font->baseline, str);

    // underlined???  draw a line under the text
    if (attrib & kTextAttributes::kUnderline) {
        DrawLine(x,y+1,x + strlen(str),y+1);
    }
}

// This assumes X = 0
void SDLDrawContext::DrawLines(const std::vector<Line *> &lines, int idxTopLine, int idxBottomLine) {
    SDL_SetRenderTarget(renderer, renderTarget);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255);

    for(int i=idxTopLine;i<idxBottomLine;i++) {
        if (i >= lines.size()) {
            break;
        }
        auto line = lines[i];
        auto nCharToPrint = line->Length()>rect.Width()?rect.Width():line->Length();
        DrawLineWithAttributesAt(0, i - idxTopLine, nCharToPrint, *line);
    }
}

// This is the more advanced drawing routine...
void SDLDrawContext::DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l) {
    DrawStringAt(x,y,l.Buffer().data());
}

