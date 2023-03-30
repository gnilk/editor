//
// Created by gnilk on 29.03.23.
//

#include "SDLDrawContext.h"
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
//
// ALL STRINGS ARE DRAWN BOTTOM UP
// YPOS means the LOWER scanline of the text-texture
//

void SDLDrawContext::DrawStringAt(int x, int y, const char *str) {
    auto font = SDLFontManager::Instance().GetActiveFont();

    auto pixWinOfs = SDLTranslate::RowColToPixel(rect.TopLeft());

    float pixXpos = SDLTranslate::ColToXPos(x);
    float pixYpos = SDLTranslate::RowToYPos(y);


    SDL_SetRenderTarget(renderer, renderTarget);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255);


    STBTTF_RenderText(renderer, font, pixWinOfs.x +  pixXpos, pixWinOfs.y + pixYpos + font->baseline , str);
}

// TODO: Refactor this - rect should be encapsulated, etc..
// Note: 'Blink' is NOT supported
void SDLDrawContext::DrawStringWithAttributesAt(int x, int y, kTextAttributes attrib, const char *str) {
    auto font = SDLFontManager::Instance().GetActiveFont();

    auto pixWinOfs = SDLTranslate::RowColToPixel(rect.TopLeft());


    float pixXpos = SDLTranslate::ColToXPos(x);
    float pixYpos = SDLTranslate::RowToYPos(y);

    SDL_SetRenderTarget(renderer, renderTarget);

    // Get colors
    auto [fg, bg] = SDLColorRepository::Instance().GetColor(0);

    // Compute text rect
    float textWidth = SDLTranslate::ColToXPos(strlen(str));

    float pixXStart = SDLTranslate::ColToXPos(x) + pixWinOfs.x;
    float pixYStart = SDLTranslate::RowToYPos(y) + pixWinOfs.x;
    float pixYEnd = SDLTranslate::RowToYPos(y+1) + pixWinOfs.y;
    SDL_FRect rect = {(float)pixXStart, pixYStart, textWidth,pixYEnd - pixYStart};

    // If we are inverted, flip the useage of color (Note: I know this can be a oneliner - perhaps in this case it would ease readability)
    if (attrib & kTextAttributes::kInverted) {
        fg.Use(renderer);
    } else {
        bg.Use(renderer);
    }
    // Now fill the rect (Regardless if we are inverted or not - someone might have filled the line)
    SDL_RenderFillRect(renderer, &rect);

    // Change color to use depending on inverted or not
    if (attrib & kTextAttributes::kInverted) {
        bg.Use(renderer);
    } else {
        fg.Use(renderer);
    }


    STBTTF_RenderText(renderer, font, pixXpos + pixWinOfs.x, pixYpos + pixWinOfs.y + font->baseline, str);

    // underlined???  draw a line under the text
    if (attrib & kTextAttributes::kUnderline) {
        SDL_RenderLine(renderer, pixXStart, pixYEnd, pixXStart+textWidth, pixYEnd);
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

