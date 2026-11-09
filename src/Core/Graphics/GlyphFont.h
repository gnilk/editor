//
// GlyphFont - a TrueType font's glyphs, rasterised a page of 256 code points at a time, the first time one of them is
// asked for (docs/usability-round.md §6). Knows stb_truetype, not SDL: a page is an alpha bitmap and where each glyph
// sits in it - the backend uploads the bitmap to a texture and draws from it.
//
// The lookup is two array indexes, pages[cp >> 8]->glyphs[cp & 0xff]; a page that isn't there yet is the only miss.
// A code point the font has no glyph for is marked missing when its page is built, so it costs a lookup, not a retry.
//
#ifndef GEDIT_GLYPHFONT_H
#define GEDIT_GLYPHFONT_H

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "stb_truetype.h"

namespace gedit {

    class GlyphFont {
    public:
        using Ref = std::shared_ptr<GlyphFont>;

        static constexpr int kPageSize = 256;
        static constexpr int kNumPages = 0x110000 / kPageSize;     // all of Unicode

        // Where a glyph sits in its page's bitmap, and how it is placed against the pen (stbtt_packedchar)
        struct Glyph {
            bool isMissing = true;
            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
            float xoff = 0;
            float yoff = 0;
            float xadvance = 0;
        };

        // 256 code points' glyphs in one square alpha bitmap
        struct Page {
            int index = 0;
            int textureSize = 0;
            std::vector<uint8_t> bitmap;            // textureSize * textureSize, one alpha byte per pixel
            std::array<Glyph, kPageSize> glyphs = {};
        };

    public:
        GlyphFont() = default;
        virtual ~GlyphFont() = default;

        // A font from its file's bytes (copied - stb_truetype reads them again for every page), at a pixel height.
        // Null when the bytes aren't a font. Page 0 is built here.
        static Ref Create(const uint8_t *data, size_t size, float pixelHeight);

        static int PageOf(char32_t codePoint) {
            return (int)(codePoint >> 8);
        }
        static int SlotOf(char32_t codePoint) {
            return (int)(codePoint & 0xff);
        }

        // The glyph for a code point, building its page on a miss; null when the font has none. 'outPage' (optional)
        // is the page the glyph's rectangle is in.
        const Glyph *Lookup(char32_t codePoint, const Page **outPage = nullptr);

        // A page, built or not - for the backend to know whether there is a bitmap to upload
        const Page *GetPage(int idxPage) const;

        // The width of one character cell: every character advances the pen by this, drawn or not, so the grid stays
        // monospaced - a missing glyph leaves its cell empty instead of pulling the rest of the line left
        float CellAdvance() const {
            return cellAdvance;
        }
        float MeasureText(const std::u32string &text) const {
            return cellAdvance * (float)text.length();
        }

        float Size() const {
            return pixelHeight;
        }
        float Scale() const {
            return scale;
        }
        int Ascent() const {
            return ascent;
        }
        // Pixels from the top of a line to the baseline
        int Baseline() const {
            return baseline;
        }

        // How many pages have been built - for the tests: a page is built once
        int NumPagesBuilt() const {
            return numPagesBuilt;
        }

    private:
        static bool IsFontHeader(const uint8_t *data, size_t size);
        bool Open(const uint8_t *data, size_t size, float newPixelHeight);
        Page *BuildPage(int idxPage);
        bool PackPage(Page &page, std::vector<int> &codePoints);

    private:
        std::vector<uint8_t> fontData;
        stbtt_fontinfo info = {};
        float pixelHeight = 0;
        float scale = 0;
        int ascent = 0;
        int baseline = 0;
        float cellAdvance = 0;

        // Built pages; a page tried once is here, even when the font has none of its glyphs
        std::vector<std::unique_ptr<Page>> pages;
        int numPagesBuilt = 0;
    };

}

#endif //GEDIT_GLYPHFONT_H
