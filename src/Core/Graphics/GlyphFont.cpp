//
// GlyphFont - see GlyphFont.h
//
#include "GlyphFont.h"

#include <fstream>
#include <algorithm>

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

void GlyphFont::SetSystemFallback(SystemLookup lookup) {
    systemLookup = std::move(lookup);
}

int GlyphFont::NumSystemFacesOpened() const {
    int count = 0;
    for (auto &systemFace : systemFaces) {
        if (systemFace.face != nullptr) {
            count++;
        }
    }
    return count;
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

// The bytes are copied - stb_truetype reads them again for every page. A collection (.ttc) opens its 'index' font.
bool GlyphFont::OpenFace(Face &face, const uint8_t *data, size_t size, int index) {
    if (!IsFontHeader(data, size)) {
        return false;
    }
    face.data.assign(data, data + size);
    face.index = index;
    int offset = stbtt_GetFontOffsetForIndex(face.data.data(), index);
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
// After the fallback files, the system's pick (SystemFaceFor).
const GlyphFont::Face *GlyphFont::FallbackFaceFor(int codePoint) {
    for (auto &fallback : fallbacks) {
        if (!fallback.isTried) {
            fallback.isTried = true;
            std::vector<uint8_t> data;
            auto face = std::make_unique<Face>();
            if (ReadFile(fallback.file, data) && OpenFace(*face, data.data(), data.size())) {
                fallback.face = std::move(face);
            }
        }
        if ((fallback.face != nullptr) && HasOutline(*fallback.face, codePoint)) {
            return fallback.face.get();
        }
    }
    return SystemFaceFor(codePoint);
}

// The face the system draws the code point with - taken only when stb_truetype can draw the glyph from it
const GlyphFont::Face *GlyphFont::SystemFaceFor(int codePoint) {
    if (!systemLookup) {
        return nullptr;
    }
    numSystemLookups++;
    auto source = systemLookup((char32_t)codePoint);
    if (!source.has_value()) {
        return nullptr;
    }
    auto face = OpenSystemFace(source.value());
    if ((face == nullptr) || !HasOutline(*face, codePoint)) {
        return nullptr;
    }
    return face;
}

// A face the system named, read the first time and kept - also when it can't be read, so it is tried once
const GlyphFont::Face *GlyphFont::OpenSystemFace(const SystemFontFace &source) {
    for (auto &systemFace : systemFaces) {
        if ((systemFace.source.file == source.file) && (systemFace.source.index == source.index) &&
            (systemFace.source.postScriptName == source.postScriptName)) {
            return systemFace.face.get();
        }
    }
    SystemFace systemFace;
    systemFace.source = source;
    std::vector<uint8_t> data;
    if (HasOutlineTables(source.file, std::max(source.index, 0)) && ReadFile(source.file, data)) {
        int index = (source.index >= 0) ? source.index : FaceIndexByName(data, source.postScriptName);
        auto face = std::make_unique<Face>();
        if (OpenFace(*face, data.data(), data.size(), index)) {
            systemFace.face = std::move(face);
        }
    }
    systemFaces.push_back(std::move(systemFace));
    return systemFaces.back().face.get();
}

// Whether a font file has outlines ('glyf' or 'CFF ') stb_truetype can draw and is not a colour bitmap font ('sbix',
// 'CBDT'), from its table directory alone. A colour emoji font may carry a 'glyf' table of empty outlines beside its
// bitmaps; the system's (Apple Color Emoji) is ~190MB that would be read for nothing. For a collection, the 'index'
// font's directory.
bool GlyphFont::HasOutlineTables(const std::filesystem::path &file, int index) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream.is_open()) {
        return false;
    }
    auto read32 = [&stream](uint32_t &outValue) {
        uint8_t bytes[4];
        if (!stream.read((char *)bytes, 4)) {
            return false;
        }
        outValue = ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | bytes[3];
        return true;
    };
    uint32_t tag = 0;
    if (!read32(tag)) {
        return false;
    }
    if (tag == 0x74746366) {        // 'ttcf': version, number of fonts, then each font's offset
        uint32_t version = 0;
        uint32_t numFonts = 0;
        if (!read32(version) || !read32(numFonts) || ((uint32_t)index >= numFonts)) {
            return false;
        }
        uint32_t offset = 0;
        stream.seekg(12 + 4 * index);
        if (!read32(offset)) {
            return false;
        }
        stream.seekg(offset);
        if (!read32(tag)) {
            return false;
        }
    }
    uint32_t numTablesAndRange = 0;
    if (!read32(numTablesAndRange)) {
        return false;
    }
    auto numTables = numTablesAndRange >> 16;
    stream.seekg(4, std::ios::cur);             // entrySelector, rangeShift - the rest of the offset table
    bool hasOutlines = false;
    for (uint32_t i = 0; i < numTables; i++) {
        uint32_t tableTag = 0;
        if (!read32(tableTag)) {
            return false;
        }
        if ((tableTag == 0x73626978) || (tableTag == 0x43424454)) {     // 'sbix' 'CBDT'
            return false;
        }
        if ((tableTag == 0x676c7966) || (tableTag == 0x43464620) || (tableTag == 0x43464632)) {   // 'glyf' 'CFF ' 'CFF2'
            hasOutlines = true;
        }
        stream.seekg(12, std::ios::cur);        // checksum, offset, length
    }
    return hasOutlines;
}

// The whole file, in one read - stb_truetype needs all of it
bool GlyphFont::ReadFile(const std::filesystem::path &file, std::vector<uint8_t> &outData) {
    std::ifstream stream(file, std::ios::binary | std::ios::ate);
    if (!stream.is_open()) {
        return false;
    }
    auto size = stream.tellg();
    if (size <= 0) {
        return false;
    }
    outData.resize((size_t)size);
    stream.seekg(0);
    return (bool)stream.read((char *)outData.data(), size);
}

// Which font of a collection has this PostScript name (name table ID 6); 0 when none does, or it isn't a collection
int GlyphFont::FaceIndexByName(const std::vector<uint8_t> &data, const std::string &postScriptName) {
    if (postScriptName.empty() || !IsFontHeader(data.data(), data.size())) {
        return 0;
    }
    int numFonts = stbtt_GetNumberOfFonts(data.data());
    for (int index = 0; index < numFonts; index++) {
        int offset = stbtt_GetFontOffsetForIndex(data.data(), index);
        if ((offset < 0) || !IsFontHeader(data.data() + offset, data.size() - offset)) {
            continue;
        }
        stbtt_fontinfo info = {};
        if (stbtt_InitFont(&info, data.data(), offset) == 0) {
            continue;
        }
        // Windows platform: UTF-16BE; Macintosh platform: Roman, one byte per character
        int length = 0;
        auto name = stbtt_GetFontNameString(&info, &length, STBTT_PLATFORM_ID_MICROSOFT, STBTT_MS_EID_UNICODE_BMP,
                                            STBTT_MS_LANG_ENGLISH, 6);
        if (name != nullptr) {
            std::string ascii;
            for (int i = 1; i < length; i += 2) {
                ascii.push_back(name[i]);
            }
            if (ascii == postScriptName) {
                return index;
            }
        }
        name = stbtt_GetFontNameString(&info, &length, STBTT_PLATFORM_ID_MAC, STBTT_MAC_EID_ROMAN,
                                       STBTT_MAC_LANG_ENGLISH, 6);
        if ((name != nullptr) && (std::string(name, (size_t)length) == postScriptName)) {
            return index;
        }
    }
    return 0;
}

// The face has a glyph for the code point that draws something. A glyph without an outline (a colour emoji font's
// bitmap glyphs, which stb_truetype cannot draw) does not count.
bool GlyphFont::HasOutline(const Face &face, int codePoint) {
    int glyphIndex = stbtt_FindGlyphIndex(&face.info, codePoint);
    return (glyphIndex != 0) && !stbtt_IsGlyphEmpty(&face.info, glyphIndex);
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
            isPacked = stbtt_PackFontRanges(&context, ranges[i].face->data.data(), ranges[i].face->index,
                                            &packRanges[i], 1) != 0;
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
