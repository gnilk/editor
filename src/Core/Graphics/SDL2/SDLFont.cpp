//
// SDLFont (SDL2) - see SDLFont.h
//
#include "SDLFont.h"

#include <cmath>
#include <cstring>

using namespace gedit;
using namespace gedit::SDL2;

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

SDLFont::SDLFont(SDL_Renderer *newRenderer, GlyphFont::Ref newGlyphFont, float newDensity) :
    renderer(newRenderer), glyphFont(std::move(newGlyphFont)), density(newDensity) {
    textures.reserve(GlyphFont::kNumPages);
    for (int i = 0; i < GlyphFont::kNumPages; i++) {
        textures.emplace_back(nullptr, &SDL_DestroyTexture);
    }
}

void SDLFont::RenderText(float x, float y, const std::u32string &text, float slant) {
    SDL_GetRenderDrawColor(renderer, &colorR, &colorG, &colorB, &colorA);
    for (auto codePoint : text) {
        RenderGlyph(x, y, codePoint, slant);
    }
}

void SDLFont::RenderText(float x, float y, const char *text, size_t maxChars, float slant) {
    SDL_GetRenderDrawColor(renderer, &colorR, &colorG, &colorB, &colorA);
    for (size_t i = 0; (i < maxChars) && (text[i] != '\0'); i++) {
        // A byte past ASCII is a piece of a UTF-8 sequence, not a Latin-1 character: its cell stays empty
        auto byte = (uint8_t)text[i];
        RenderGlyph(x, y, (byte < 128) ? (char32_t)byte : U' ', slant);
    }
}

float SDLFont::MeasureText(const char *text) const {
    return glyphFont->CellAdvance() * (float)strlen(text) / density;
}

void SDLFont::RenderGlyph(float &x, float y, char32_t codePoint, float slant) {
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
            SDL_FRect dst = GlyphDestination(x, y, *glyph, w, h);
            if (slant != 0.0f) {
                SDL_FRect srcF = {(float)src.x, (float)src.y, w, h};
                RenderSlanted(texture, page, y, srcF, dst, slant);
            } else {
                SDL_RenderCopyF(renderer, texture, &src, &dst);
            }
        }
    }
    x += glyphFont->CellAdvance() / density;
}

// The glyph as a quad sheared about the baseline: a point 'h' above it moves right by slant * h (a descender left).
// The colour rides on the vertices with the texture's own mod at white - one colour whether or not this SDL applies
// the texture mod to geometry.
void SDLFont::RenderSlanted(SDL_Texture *texture, const GlyphFont::Page *page, float baseline, const SDL_FRect &src, const SDL_FRect &dst, float slant) {
    SDL_SetTextureColorMod(texture, 0xff, 0xff, 0xff);
    SDL_SetTextureAlphaMod(texture, 0xff);
    SDL_Color color = {colorR, colorG, colorB, colorA};
    float size = (float)page->textureSize;
    float u0 = src.x / size;
    float v0 = src.y / size;
    float u1 = (src.x + src.w) / size;
    float v1 = (src.y + src.h) / size;
    float shiftTop = slant * (baseline - dst.y);
    float shiftBottom = slant * (baseline - (dst.y + dst.h));
    SDL_Vertex vertices[4] = {
        {{dst.x + shiftTop, dst.y}, color, {u0, v0}},
        {{dst.x + dst.w + shiftTop, dst.y}, color, {u1, v0}},
        {{dst.x + shiftBottom, dst.y + dst.h}, color, {u0, v1}},
        {{dst.x + dst.w + shiftBottom, dst.y + dst.h}, color, {u1, v1}},
    };
    const int indices[6] = {0, 1, 2, 2, 1, 3};
    SDL_RenderGeometry(renderer, texture, vertices, 4, indices, 6);
}

// Where a glyph goes, in logical units: the pen and the glyph's offset meet in screen pixels, snapped to a whole one so
// the bitmap isn't resampled, and the glyph keeps its rasterised size - one bitmap pixel on one screen pixel
SDL_FRect SDLFont::GlyphDestination(float x, float y, const GlyphFont::Glyph &glyph, float w, float h) const {
    float screenX = std::round(x * density + glyph.xoff);
    float screenY = std::round(y * density + glyph.yoff);
    return {screenX / density, screenY / density, w / density, h / density};
}

// The page's alpha bitmap as white with that alpha (through the text weight curve), coloured at draw by the texture's
// colour mod
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
        pixels[i] = SDL_MapRGBA(format, 0xff, 0xff, 0xff, coverageCurve[page->bitmap[i]]);
    }
    SDL_FreeFormat(format);
    SDL_UpdateTexture(texture.get(), nullptr, pixels.data(), size * (int)sizeof(Uint32));
    return texture.get();
}
