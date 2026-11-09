//
// SDLFont (SDL3) - see SDLFont.h
//
#include "SDLFont.h"

#include <cmath>
#include <cstring>

using namespace gedit;
using namespace gedit::SDL3;

SDLFont::Ptr SDLFont::Create(SDL_Renderer *renderer, const uint8_t *data, size_t size, float pixelHeight, float density) {
    if (density <= 0.0f) {
        density = 1.0f;
    }
    auto glyphFont = GlyphFont::Create(data, size, pixelHeight * density);
    if (glyphFont == nullptr) {
        return nullptr;
    }
    return std::make_unique<SDLFont>(renderer, glyphFont, density);
}

SDLFont::SDLFont(SDL_Renderer *renderer, GlyphFont::Ref glyphFont, float density) :
    renderer(renderer), glyphFont(std::move(glyphFont)), density(density) {
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
    return glyphFont->CellAdvance() * (float)strlen(text) / density;
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
            SDL_FRect src = {(float)glyph->x0, (float)glyph->y0, w, h};
            SDL_FRect dst = GlyphDestination(x, y, *glyph, w, h);
            SDL_RenderTexture(renderer, texture, &src, &dst);
        }
    }
    x += glyphFont->CellAdvance() / density;
}

// Where a glyph goes, in logical units: the pen and the glyph's offset meet in screen pixels, snapped to a whole one so
// the bitmap isn't resampled, and the glyph keeps its rasterised size - one bitmap pixel on one screen pixel
SDL_FRect SDLFont::GlyphDestination(float x, float y, const GlyphFont::Glyph &glyph, float w, float h) const {
    float screenX = std::round(x * density + glyph.xoff);
    float screenY = std::round(y * density + glyph.yoff);
    return {screenX / density, screenY / density, w / density, h / density};
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
    SDL_SetTextureScaleMode(texture.get(), SDL_SCALEMODE_NEAREST);

    auto format = SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_RGBA32);
    std::vector<Uint32> pixels((size_t)size * size);
    for (size_t i = 0; i < pixels.size(); i++) {
        pixels[i] = SDL_MapRGBA(format, nullptr, 0xff, 0xff, 0xff, page->bitmap[i]);
    }
    SDL_UpdateTexture(texture.get(), nullptr, pixels.data(), size * (int)sizeof(Uint32));
    return texture.get();
}
