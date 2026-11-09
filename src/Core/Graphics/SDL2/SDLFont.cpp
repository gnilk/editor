//
// SDLFont (SDL2) - see SDLFont.h
//
#include "SDLFont.h"

#include <cstring>

using namespace gedit;
using namespace gedit::SDL2;

SDLFont::Ptr SDLFont::Create(SDL_Renderer *renderer, const uint8_t *data, size_t size, float pixelHeight) {
    auto glyphFont = GlyphFont::Create(data, size, pixelHeight);
    if (glyphFont == nullptr) {
        return nullptr;
    }
    return std::make_unique<SDLFont>(renderer, glyphFont);
}

SDLFont::SDLFont(SDL_Renderer *renderer, GlyphFont::Ref glyphFont) : renderer(renderer), glyphFont(std::move(glyphFont)) {
    textures.reserve(GlyphFont::kNumPages);
    for (int i = 0; i < GlyphFont::kNumPages; i++) {
        textures.emplace_back(nullptr, &SDL_DestroyTexture);
    }
}

void SDLFont::RenderText(float x, float y, const std::u32string &text) {
    SDL_GetRenderDrawColor(renderer, &colorR, &colorG, &colorB, &colorA);
    for (auto codePoint : text) {
        RenderGlyph(x, y, codePoint);
    }
}

void SDLFont::RenderText(float x, float y, const char *text, size_t maxChars) {
    SDL_GetRenderDrawColor(renderer, &colorR, &colorG, &colorB, &colorA);
    for (size_t i = 0; (i < maxChars) && (text[i] != '\0'); i++) {
        // A byte past ASCII is a piece of a UTF-8 sequence, not a Latin-1 character: its cell stays empty
        auto byte = (uint8_t)text[i];
        RenderGlyph(x, y, (byte < 128) ? (char32_t)byte : U' ');
    }
}

float SDLFont::MeasureText(const char *text) const {
    return glyphFont->CellAdvance() * (float)strlen(text);
}

void SDLFont::RenderGlyph(float &x, float y, char32_t codePoint) {
    const GlyphFont::Page *page = nullptr;
    auto glyph = glyphFont->Lookup(codePoint, &page);
    if (glyph != nullptr) {
        auto texture = TextureFor(page);
        if (texture != nullptr) {
            SDL_SetTextureColorMod(texture, colorR, colorG, colorB);
            SDL_SetTextureAlphaMod(texture, colorA);
            float w = (float)(glyph->x1 - glyph->x0);
            float h = (float)(glyph->y1 - glyph->y0);
            SDL_Rect src = {glyph->x0, glyph->y0, (int)w, (int)h};
            SDL_Rect dst = {(int)(x + glyph->xoff), (int)(y + glyph->yoff), (int)w, (int)h};
            SDL_RenderCopy(renderer, texture, &src, &dst);
        }
    }
    x += glyphFont->CellAdvance();
}

// The page's alpha bitmap as white with that alpha, coloured at draw by the texture's colour mod
SDL_Texture *SDLFont::TextureFor(const GlyphFont::Page *page) {
    auto &texture = textures[page->index];
    if (texture != nullptr) {
        return texture.get();
    }
    int size = page->textureSize;
    texture.reset(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, size, size));
    if (texture == nullptr) {
        return nullptr;
    }
    SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
    // Nearest is good for smaller fonts, linear would be better for larger ones
    SDL_SetTextureScaleMode(texture.get(), SDL_ScaleModeNearest);

    auto format = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA32);
    std::vector<Uint32> pixels((size_t)size * size);
    for (size_t i = 0; i < pixels.size(); i++) {
        pixels[i] = SDL_MapRGBA(format, 0xff, 0xff, 0xff, page->bitmap[i]);
    }
    SDL_FreeFormat(format);
    SDL_UpdateTexture(texture.get(), nullptr, pixels.data(), size * (int)sizeof(Uint32));
    return texture.get();
}
