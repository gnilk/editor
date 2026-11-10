//
// GlyphFont - see GlyphFont.h
//
#include "GlyphFont.h"

#include <fstream>
#include <iterator>

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

void GlyphFont::SetFallbackFiles(const std::vector<std::filesystem::path> &files) {
    fallbacks.clear();
    for (auto &file : files) {
        Fallback fallback;
        fallback.file = file;
        fallbacks.push_back(std::move(fallback));
    }
}

int GlyphFont::NumFallbacksOpened() const {
    int count = 0;
    for (auto &fallback : fallbacks) {
        if (fallback.face != nullptr) {
            count++;
        }
    }
    return count;
}

const GlyphFont::Page *GlyphFont::GetPage(int idxPage) const {
    if ((idxPage < 0) || (idxPage >= kNumPages)) {
        return nullptr;
    }
    return pages[idxPage].get();
}

bool GlyphFont::Open(const uint8_t *data, size_t size, float newPixelHeight) {
    if (!OpenFace(primary, data, size)) {
        return false;
    }
    pixelHeight = newPixelHeight;
    scale = stbtt_ScaleForPixelHeight(&primary.info, pixelHeight);
    stbtt_GetFontVMetrics(&primary.info, &ascent, nullptr, nullptr);
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

// The bytes are copied - stb_truetype reads them again for every page. A collection (.ttc) opens its first font.
bool GlyphFont::OpenFace(Face &face, const uint8_t *data, size_t size) {
    if (!IsFontHeader(data, size)) {
        return false;
    }
    face.data.assign(data, data + size);
    int offset = stbtt_GetFontOffsetForIndex(face.data.data(), 0);
    if ((offset < 0) || !IsFontHeader(face.data.data() + offset, size - offset)) {
        return false;
    }
    return stbtt_InitFont(&face.info, face.data.data(), offset) != 0;
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

// Every code point of the page the font - or else a fallback - has a glyph for, packed into one bitmap. Control
// characters (below 32) are never drawn. A code point no font has is left missing - stb would otherwise pack the
// font's 'no glyph' box for it.
GlyphFont::Page *GlyphFont::BuildPage(int idxPage) {
    auto page = std::make_unique<Page>();
    page->index = idxPage;

    std::vector<FaceRange> ranges;
    auto addTo = [&ranges](const Face *face, int codePoint) {
        for (auto &range : ranges) {
            if (range.face == face) {
                range.codePoints.push_back(codePoint);
                return;
            }
        }
        ranges.push_back({face, {codePoint}});
    };
    for (int slot = 0; slot < kPageSize; slot++) {
        int codePoint = idxPage * kPageSize + slot;
        if (codePoint < 32) {
            continue;
        }
        if (stbtt_FindGlyphIndex(&primary.info, codePoint) != 0) {
            addTo(&primary, codePoint);
            continue;
        }
        auto face = FallbackFaceFor(codePoint);
        if (face != nullptr) {
            addTo(face, codePoint);
        }
    }
    if (!ranges.empty()) {
        PackPage(*page, ranges);
    }

    numPagesBuilt++;
    pages[idxPage] = std::move(page);
    return pages[idxPage].get();
}

// The first fallback with a glyph that draws something for the code point; a fallback file is read when first asked.
// A glyph without an outline (a colour emoji font's bitmap glyphs, which stb_truetype cannot draw) does not count.
const GlyphFont::Face *GlyphFont::FallbackFaceFor(int codePoint) {
    for (auto &fallback : fallbacks) {
        if (!fallback.isTried) {
            fallback.isTried = true;
            std::ifstream file(fallback.file, std::ios::binary);
            if (file.is_open()) {
                std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                auto face = std::make_unique<Face>();
                if (OpenFace(*face, data.data(), data.size())) {
                    fallback.face = std::move(face);
                }
            }
        }
        if (fallback.face == nullptr) {
            continue;
        }
        int glyphIndex = stbtt_FindGlyphIndex(&fallback.face->info, codePoint);
        if ((glyphIndex != 0) && !stbtt_IsGlyphEmpty(&fallback.face->info, glyphIndex)) {
            return fallback.face.get();
        }
    }
    return nullptr;
}

// Each face's code points packed into the one bitmap, at the same pixel height - every face's glyphs sit on the
// same baseline
bool GlyphFont::PackPage(Page &page, std::vector<FaceRange> &ranges) {
    std::vector<std::vector<stbtt_packedchar>> packed(ranges.size());
    std::vector<stbtt_pack_range> packRanges(ranges.size());
    for (size_t i = 0; i < ranges.size(); i++) {
        packed[i].resize(ranges[i].codePoints.size());
        auto &packRange = packRanges[i];
        packRange = {};
        packRange.font_size = pixelHeight;
        packRange.array_of_unicode_codepoints = ranges[i].codePoints.data();
        packRange.num_chars = (int)ranges[i].codePoints.size();
        packRange.chardata_for_range = packed[i].data();
    }

    for (int textureSize = kMinTextureSize; textureSize <= kMaxTextureSize; textureSize *= 2) {
        std::vector<uint8_t> bitmap((size_t)textureSize * textureSize);
        stbtt_pack_context context;
        if (stbtt_PackBegin(&context, bitmap.data(), textureSize, textureSize, 0, 1, nullptr) == 0) {
            return false;
        }
        stbtt_PackSetOversampling(&context, 1, 1);
        bool isPacked = true;
        for (size_t i = 0; (i < ranges.size()) && isPacked; i++) {
            isPacked = stbtt_PackFontRanges(&context, ranges[i].face->data.data(), 0, &packRanges[i], 1) != 0;
        }
        stbtt_PackEnd(&context);
        if (!isPacked) {
            continue;
        }
        page.textureSize = textureSize;
        page.bitmap = std::move(bitmap);
        for (size_t r = 0; r < ranges.size(); r++) {
            StoreGlyphs(page, ranges[r].codePoints, packed[r]);
        }
        return true;
    }
    return false;
}

void GlyphFont::StoreGlyphs(Page &page, const std::vector<int> &codePoints, const std::vector<stbtt_packedchar> &packed) {
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
}
