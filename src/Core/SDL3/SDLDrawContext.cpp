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

void SDLDrawContext::Clear() {

}

void SDLDrawContext::DrawLine(Line *line, int idxLine) {
    // REMOVE ME!
}

void SDLDrawContext::ClearLine(int y) {

}

void SDLDrawContext::FillLine(int y, kTextAttributes attrib, char c) {
    auto [fg, bg] = SDLColorRepository::Instance().GetColor(0);
    if (attrib & kTextAttributes::kInverted) {
        fg.Use(renderer);
    } else {
        bg.Use(renderer);
    }
    FillRect(0,y,rect.Width(),1);
}

void SDLDrawContext::Scroll(int nRows) {

}

std::pair<float, float> SDLDrawContext::CoordsToScreen(float x, float y) {
    auto pixWinOfs = SDLTranslate::RowColToPixel(rect.TopLeft());

    float screenXPos = SDLTranslate::ColToXPos(x) + pixWinOfs.x;
    float screenYPos = SDLTranslate::RowToYPos(y) + pixWinOfs.y;

    return {screenXPos, screenYPos};
}

void SDLDrawContext::FillRect(float x, float y, float w, float h) {
    auto [pixXStart, pixYStart] = CoordsToScreen(x, y);
    //auto [pixWidth, pixHeight] = CoordsToScreen(w, h);

    auto pixWidth = SDLTranslate::ColToXPos(w);
    auto pixHeight = SDLTranslate::RowToYPos(h);

    SDL_FRect rect = {pixXStart, pixYStart, pixWidth, pixHeight};
    SDL_RenderFillRect(renderer, &rect);
}

void SDLDrawContext::DrawLine(float x1, float y1, float x2, float y2) {
    auto [px1, py1] = CoordsToScreen(x1, y1);
    auto [px2, py2] = CoordsToScreen(x2, y2);

    SDL_RenderLine(renderer, px1, py1, px2, py2);
}
void SDLDrawContext::DrawLineWithPixelOffset(float x1, float y1, float x2, float y2, float ofsX, float ofsY) {
    auto [px1, py1] = CoordsToScreen(x1, y1);
    auto [px2, py2] = CoordsToScreen(x2, y2);

    SDL_RenderLine(renderer, px1 + ofsX, py1 + ofsY, px2 + ofsX, py2 + ofsY);
}


//
// ALL STRINGS ARE DRAWN BOTTOM UP
// YPOS means the LOWER scanline of the text-texture
//
void SDLDrawContext::DrawStringAt(int x, int y, const char *str) {
    auto font = SDLFontManager::Instance().GetActiveFont();

    SDL_SetRenderTarget(renderer, renderTarget);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    auto [px, py] = CoordsToScreen(x, y);

    STBTTF_RenderText(renderer, font, px, py + font->baseline, str);
}

// Note: 'Blink' is NOT supported
void SDLDrawContext::DrawStringWithAttributesAndColAt(int x, int y, kTextAttributes attrib, int idxColor, const char *str) {
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

void SDLDrawContext::DrawStringWithAttributesAt(int x, int y, kTextAttributes attrib, const char *str) {
    DrawStringWithAttributesAndColAt(x,y, attrib, 0, str);
}

// This assumes X = 0
void SDLDrawContext::DrawLines(const std::vector<Line *> &lines, int idxTopLine, int idxBottomLine) {
    SDL_SetRenderTarget(renderer, renderTarget);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    for (int i = idxTopLine; i < idxBottomLine; i++) {
        if (i >= lines.size()) {
            break;
        }
        auto line = lines[i];
        auto nCharToPrint = line->Length() > rect.Width() ? rect.Width() : line->Length();
        DrawLineWithAttributesAt(0, i - idxTopLine, nCharToPrint, *line);
    }
}

// This is the more advanced drawing routine...
void SDLDrawContext::DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l) {
    auto font = SDLFontManager::Instance().GetActiveFont();
    SDL_SetRenderTarget(renderer, renderTarget);

    // No attributes?  Just dump the string...
    auto &attribs = l.Attributes();
    if (attribs.size() == 0) {
        DrawStringAt(x, y, l.Buffer().data());
        return;
    }

    // While the NCurses backend draw a char at the time this one will draw chunks
    auto itAttrib = attribs.begin();
    int xp = x;

    while (itAttrib != attribs.end()) {
        auto next = itAttrib + 1;
        size_t len = std::string::npos;
        // Not at the end - replace with length of this attribute
        if (next != attribs.end()) {
            // Some kind of assert!
            if (itAttrib->idxOrigString > next->idxOrigString) {
                auto logger = gnilk::Logger::GetLogger("SDLDrawContext");
                logger->Error("DrawLineWithAttributesAt, attribute index is wrong for line: '%s'", l.Buffer().data());
                return;
            }
            len = next->idxOrigString - itAttrib->idxOrigString;
        }

        // Grab the substring for this attribute range
        std::string strOut = std::string(l.Buffer().data(), itAttrib->idxOrigString, len);

        // Draw string with the correct color...

        DrawStringWithAttributesAndColAt(xp,y, itAttrib->textAttributes, itAttrib->idxColor, strOut.c_str());

//        auto [fgColor, bgColor] = SDLColorRepository::Instance().GetColor(itAttrib->idxColor);
//        auto [px, py] = CoordsToScreen(xp,y);
//        fgColor.Use(renderer);
//        STBTTF_RenderText(renderer, font, px, py + font->baseline, strOut.c_str());

        xp += len;
        itAttrib = next;
    }
}

