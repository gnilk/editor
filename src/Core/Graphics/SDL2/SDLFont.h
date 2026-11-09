//
// The SDL2 part of a font (docs/usability-round.md §6): GlyphFont's pages as textures, each uploaded the first time a
// glyph of it is drawn. The page table, the build on a miss and the missing glyphs are GlyphFont's.
//
// High density (docs/open-bugs.md #18): the glyphs are rasterised at the screen's pixel density - 18 px on a 2x display
// is 36 px of glyph - while everything this class takes and returns (pen positions, the baseline, widths) stays in
// logical units, the units the renderer's scale maps onto the screen. A glyph pixel then lands on one screen pixel
// instead of being magnified.
//
#ifndef GEDIT_SDL2_SDLFONT_H
#define GEDIT_SDL2_SDLFONT_H

#include <memory>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include "Core/Graphics/GlyphFont.h"

namespace gedit::SDL2 {

    class SDLFont {
    public:
        using Ptr = std::unique_ptr<SDLFont>;

        SDLFont(SDL_Renderer *renderer, GlyphFont::Ref glyphFont, float density);
        virtual ~SDLFont() = default;

        // A font from its file's bytes at a logical pixel height, rasterised at 'density' screen pixels per logical
        // pixel and drawn with 'renderer'; null when the bytes aren't a font
        static Ptr Create(SDL_Renderer *renderer, const uint8_t *data, size_t size, float pixelHeight, float density);

        // Draw with the renderer's draw colour, the pen's baseline at y. Every character takes one cell; one the font
        // has no glyph for leaves it empty.
        void RenderText(float x, float y, const std::u32string &text);
        // ASCII, at most 'maxChars' bytes - one cell per byte
        void RenderText(float x, float y, const char *text, size_t maxChars = SIZE_MAX);

        float MeasureText(const char *text) const;

        // Logical pixels from the top of a line to the baseline
        float Baseline() const {
            return (float)glyphFont->Baseline() / density;
        }

        // Screen pixels per logical pixel the glyphs were rasterised for
        float Density() const {
            return density;
        }

    private:
        using TexturePtr = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;

        void RenderGlyph(float &x, float y, char32_t codePoint);
        SDL_FRect GlyphDestination(float x, float y, const GlyphFont::Glyph &glyph, float w, float h) const;
        SDL_Texture *TextureFor(const GlyphFont::Page *page);

    private:
        SDL_Renderer *renderer = nullptr;
        GlyphFont::Ref glyphFont;
        float density = 1.0f;
        std::vector<TexturePtr> textures;           // by page, made the first time the page is drawn from
        Uint8 colorR = 0xff;
        Uint8 colorG = 0xff;
        Uint8 colorB = 0xff;
        Uint8 colorA = 0xff;
    };

}

#endif //GEDIT_SDL2_SDLFONT_H
