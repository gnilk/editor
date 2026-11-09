//
// GlyphFont - see GlyphFont.h
//
#include "GlyphFont.h"

#include "stb_rect_pack.h"

using namespace gedit;

// A page's bitmap starts this size and doubles until its glyphs fit; past the largest the page has no glyphs
static constexpr int kMinTextureSize = 128;
static constexpr int kMaxTextureSize = 4096;

GlyphFont::Ref GlyphFont::Create(const uint8_t *data, size_t size, float pixelHeight) {
    auto font = std::make_shared<GlyphFont>();
    if (!font->Open(data, size, pixelHeight)) {
        return nullptr;
    }
    return font;
}

const GlyphFont::Glyph *GlyphFont::Lookup(char32_t codePoint, const Page **outPage) {
    auto idxPage = PageOf(codePoint);
    if (idxPage >= kNumPages) {
        return nullptr;
    }
    auto page = pages[idxPage].get();
    if (page == nullptr) {
        page = BuildPage(idxPage);
    }
    auto &glyph = page->glyphs[SlotOf(codePoint)];
    if (glyph.isMissing) {
        return nullptr;
    }
    if (outPage != nullptr) {
        *outPage = page;
    }
    return &glyph;
}

const GlyphFont::Page *GlyphFont::GetPage(int idxPage) const {
    if ((idxPage < 0) || (idxPage >= kNumPages)) {
        return nullptr;
    }
    return pages[idxPage].get();
}

bool GlyphFont::Open(const uint8_t *data, size_t size, float newPixelHeight) {
    if (!IsFontHeader(data, size)) {
        return false;
    }
    fontData.assign(data, data + size);
    int offset = stbtt_GetFontOffsetForIndex(fontData.data(), 0);
    if ((offset < 0) || !IsFontHeader(fontData.data() + offset, size - offset)) {
        return false;
    }
    if (stbtt_InitFont(&info, fontData.data(), offset) == 0) {
        return false;
    }
    pixelHeight = newPixelHeight;
    scale = stbtt_ScaleForPixelHeight(&info, pixelHeight);
    stbtt_GetFontVMetrics(&info, &ascent, nullptr, nullptr);
    baseline = (int)((float)ascent * scale);
    pages.resize(kNumPages);

    // Page 0 when the font opens, as before; the cell is as wide as a space
    BuildPage(0);
    auto space = Lookup(U' ');
    if (space == nullptr) {
        return false;
    }
    cellAdvance = space->xadvance;
    return true;
}

// stb_truetype reads whatever it is given without checking it fits: before it does, the bytes start like a font (a
// TrueType / OpenType tag, or a collection's) and the table directory is inside them. The tables themselves are trusted.
bool GlyphFont::IsFontHeader(const uint8_t *data, size_t size) {
    if ((data == nullptr) || (size < 12)) {
        return false;
    }
    auto read16 = [data](size_t at) { return (uint32_t)((data[at] << 8) | data[at + 1]); };
    uint32_t tag = (read16(0) << 16) | read16(2);
    if (tag == 0x74746366) {        // 'ttcf' - a collection, the font is at an offset
        return true;
    }
    if ((tag != 0x00010000) && (tag != 0x74727565) && (tag != 0x4f54544f) && (tag != 0x74797031)) {    // 1.0, 'true', 'OTTO', 'typ1'
        return false;
    }
    size_t numTables = read16(4);
    return (12 + numTables * 16) <= size;
}

// Every code point of the page the font has a glyph for, packed into one bitmap. Control characters (below 32) are
// never drawn. A code point the font lacks is left missing - stb would otherwise pack the font's 'no glyph' box for it.
GlyphFont::Page *GlyphFont::BuildPage(int idxPage) {
    auto page = std::make_unique<Page>();
    page->index = idxPage;

    std::vector<int> codePoints;
    for (int slot = 0; slot < kPageSize; slot++) {
        int codePoint = idxPage * kPageSize + slot;
        if (codePoint < 32) {
            continue;
        }
        if (stbtt_FindGlyphIndex(&info, codePoint) == 0) {
            continue;
        }
        codePoints.push_back(codePoint);
    }
    if (!codePoints.empty()) {
        PackPage(*page, codePoints);
    }

    numPagesBuilt++;
    pages[idxPage] = std::move(page);
    return pages[idxPage].get();
}

bool GlyphFont::PackPage(Page &page, std::vector<int> &codePoints) {
    std::vector<stbtt_packedchar> packed(codePoints.size());
    stbtt_pack_range range = {};
    range.font_size = pixelHeight;
    range.array_of_unicode_codepoints = codePoints.data();
    range.num_chars = (int)codePoints.size();
    range.chardata_for_range = packed.data();

    for (int textureSize = kMinTextureSize; textureSize <= kMaxTextureSize; textureSize *= 2) {
        std::vector<uint8_t> bitmap((size_t)textureSize * textureSize);
        stbtt_pack_context context;
        if (stbtt_PackBegin(&context, bitmap.data(), textureSize, textureSize, 0, 1, nullptr) == 0) {
            return false;
        }
        stbtt_PackSetOversampling(&context, 1, 1);
        int isPacked = stbtt_PackFontRanges(&context, fontData.data(), 0, &range, 1);
        stbtt_PackEnd(&context);
        if (isPacked == 0) {
            continue;
        }
        page.textureSize = textureSize;
        page.bitmap = std::move(bitmap);
        for (size_t i = 0; i < codePoints.size(); i++) {
            auto &from = packed[i];
            auto &glyph = page.glyphs[SlotOf((char32_t)codePoints[i])];
            glyph.isMissing = false;
            glyph.x0 = from.x0;
            glyph.y0 = from.y0;
            glyph.x1 = from.x1;
            glyph.y1 = from.y1;
            glyph.xoff = from.xoff;
            glyph.yoff = from.yoff;
            glyph.xadvance = from.xadvance;
        }
        return true;
    }
    return false;
}
