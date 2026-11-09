//
// The SDL3 part of a font (docs/usability-round.md §6): GlyphFont's pages as textures, each uploaded the first time a
// glyph of it is drawn. The page table, the build on a miss and the missing glyphs are GlyphFont's.
//
#ifndef GEDIT_SDL3_SDLFONT_H
#define GEDIT_SDL3_SDLFONT_H

#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "Core/Graphics/GlyphFont.h"

namespace gedit::SDL3 {

    class SDLFont {
    public:
        using Ptr = std::unique_ptr<SDLFont>;

        SDLFont(SDL_Renderer *renderer, GlyphFont::Ref glyphFont);
        virtual ~SDLFont() = default;

        // A font from its file's bytes at a pixel height, drawn with 'renderer'; null when the bytes aren't a font
        static Ptr Create(SDL_Renderer *renderer, const uint8_t *data, size_t size, float pixelHeight);

        // Draw with the renderer's draw colour, the pen's baseline at y. Every character takes one cell; one the font
        // has no glyph for leaves it empty.
        void RenderText(float x, float y, const std::u32string &text);
        // ASCII, at most 'maxChars' bytes - one cell per byte
        void RenderText(float x, float y, const char *text, size_t maxChars = SIZE_MAX);

        float MeasureText(const char *text) const;

        int Baseline() const {
            return glyphFont->Baseline();
        }

    private:
        using TexturePtr = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;

        void RenderGlyph(float &x, float y, char32_t codePoint);
        SDL_Texture *TextureFor(const GlyphFont::Page *page);

    private:
        SDL_Renderer *renderer = nullptr;
        GlyphFont::Ref glyphFont;
        std::vector<TexturePtr> textures;           // by page, made the first time the page is drawn from
        Uint8 colorR = 0xff;
        Uint8 colorG = 0xff;
        Uint8 colorB = 0xff;
        Uint8 colorA = 0xff;
    };

}

#endif //GEDIT_SDL3_SDLFONT_H
