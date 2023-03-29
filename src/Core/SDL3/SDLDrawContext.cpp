//
// Created by gnilk on 29.03.23.
//

#include "SDLDrawContext.h"
#include "SDLTranslate.h"
#include "SDLFontManager.h"

using namespace gedit;

void SDLDrawContext::Clear() {

}

void SDLDrawContext::ClearLine(int y) {

}

void SDLDrawContext::FillLine(int y, kTextAttributes attrib, char c) {
    Rect pixRect = SDLTranslate::RowColToPixel(rect);
    float pixYStart = SDLTranslate::RowToYPos(y);
    float pixYEnd = SDLTranslate::RowToYPos(y+1);
    SDL_FRect rect = {(float)pixRect.TopLeft().x, pixYStart, (float)pixRect.Width(),pixYEnd - pixYStart};
    SDL_RenderRect(renderer, &rect);

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

void SDLDrawContext::DrawStringWithAttributesAt(int x, int y, kTextAttributes attrib, const char *str) {
    auto font = SDLFontManager::Instance().GetActiveFont();

    auto pixWinOfs = SDLTranslate::RowColToPixel(rect.TopLeft());


    float pixXpos = SDLTranslate::ColToXPos(x);
    float pixYpos = SDLTranslate::RowToYPos(y);

    SDL_SetRenderTarget(renderer, renderTarget);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255);

    STBTTF_RenderText(renderer, font, pixXpos + pixWinOfs.x, pixYpos + pixWinOfs.y + font->baseline, str);

}

void SDLDrawContext::DrawLine(Line *line, int idxLine) {
    // REMOVE ME!
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

void SDLDrawContext::DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l) {
    DrawStringAt(x,y,l.Buffer().data());
}

