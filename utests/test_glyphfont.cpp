//
// GlyphFont (docs/usability-round.md §6) - the glyph page table, without a screen, against the shipped Andale Mono:
// it has '│' (U+2502), it lacks '▎' (U+258E).
//
#include <filesystem>
#include <optional>
#include <testinterface.h>

#include "Core/Graphics/GlyphFont.h"
#include "Core/Graphics/SystemFonts.h"
#include "Core/RuntimeConfig.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_glyphfont(ITesting *t);
    DLL_EXPORT int test_glyphfont_pageslot(ITesting *t);
    DLL_EXPORT int test_glyphfont_page0(ITesting *t);
    DLL_EXPORT int test_glyphfont_buildonce(ITesting *t);
    DLL_EXPORT int test_glyphfont_missing(ITesting *t);
    DLL_EXPORT int test_glyphfont_bitmap(ITesting *t);
    DLL_EXPORT int test_glyphfont_advance(ITesting *t);
    DLL_EXPORT int test_glyphfont_notafont(ITesting *t);
    DLL_EXPORT int test_glyphfont_fallback(ITesting *t);
    DLL_EXPORT int test_glyphfont_fallback_colouremoji(ITesting *t);
    DLL_EXPORT int test_glyphfont_systemfallback(ITesting *t);
    DLL_EXPORT int test_glyphfont_systemfallback_order(ITesting *t);
    DLL_EXPORT int test_glyphfont_systemfallback_colouremoji(ITesting *t);
    DLL_EXPORT int test_glyphfont_systemlookup(ITesting *t);
}

namespace {
    GlyphFont::Ref OpenAndale(ITesting *t) {
        auto asset = RuntimeConfig::Instance().GetAssetLoader().LoadAsset("Andale Mono.ttf");
        TR_ASSERT(t, asset != nullptr);
        auto font = GlyphFont::Create(asset->GetPtrAs<const uint8_t *>(), asset->GetSize(), 18);
        TR_ASSERT(t, font != nullptr);
        return font;
    }

    // Any pixel drawn inside the glyph's rectangle of its page
    bool HasInk(const GlyphFont::Page *page, const GlyphFont::Glyph *glyph) {
        for (int y = glyph->y0; y < glyph->y1; y++) {
            for (int x = glyph->x0; x < glyph->x1; x++) {
                if (page->bitmap[(size_t)y * page->textureSize + x] != 0) {
                    return true;
                }
            }
        }
        return false;
    }
}

DLL_EXPORT int test_glyphfont(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_glyphfont_pageslot(ITesting *t) {
    TR_ASSERT(t, (GlyphFont::PageOf(U'A') == 0) && (GlyphFont::SlotOf(U'A') == 0x41));
    TR_ASSERT(t, (GlyphFont::PageOf(U'│') == 0x25) && (GlyphFont::SlotOf(U'│') == 0x02));
    TR_ASSERT(t, (GlyphFont::PageOf(0x10FFFF) == GlyphFont::kNumPages - 1) && (GlyphFont::SlotOf(0x10FFFF) == 0xff));
    return kTR_Pass;
}

DLL_EXPORT int test_glyphfont_page0(ITesting *t) {
    auto font = OpenAndale(t);
    // Built when the font opens: ASCII and Latin-1 cost no build
    TR_ASSERT(t, font->NumPagesBuilt() == 1);
    TR_ASSERT(t, font->GetPage(0) != nullptr);
    TR_ASSERT(t, font->Lookup(U'A') != nullptr);
    TR_ASSERT(t, font->Lookup(U'å') != nullptr);       // å
    TR_ASSERT(t, font->NumPagesBuilt() == 1);
    // Control characters are never drawn
    TR_ASSERT(t, font->Lookup(U'\t') == nullptr);
    TR_ASSERT(t, font->Baseline() > 0);
    TR_ASSERT(t, font->CellAdvance() > 0);
    return kTR_Pass;
}

DLL_EXPORT int test_glyphfont_buildonce(ITesting *t) {
    auto font = OpenAndale(t);
    TR_ASSERT(t, font->GetPage(0x25) == nullptr);
    const GlyphFont::Page *page = nullptr;
    auto bar = font->Lookup(U'│', &page);
    TR_ASSERT(t, bar != nullptr);
    TR_ASSERT(t, (page != nullptr) && (page->index == 0x25));
    TR_ASSERT(t, font->NumPagesBuilt() == 2);
    // The rest of the page comes with it
    TR_ASSERT(t, font->Lookup(U'─') != nullptr);      // ─
    TR_ASSERT(t, font->Lookup(U'│') == bar);
    TR_ASSERT(t, font->NumPagesBuilt() == 2);
    return kTR_Pass;
}

DLL_EXPORT int test_glyphfont_missing(ITesting *t) {
    auto font = OpenAndale(t);
    // Not in the font: missing, and asking again builds nothing
    TR_ASSERT(t, font->Lookup(U'▎') == nullptr);
    int numBuilt = font->NumPagesBuilt();
    TR_ASSERT(t, font->Lookup(U'▎') == nullptr);
    TR_ASSERT(t, font->NumPagesBuilt() == numBuilt);
    // A page the font has nothing on (CJK) is built once, empty
    TR_ASSERT(t, font->Lookup(U'中') == nullptr);
    TR_ASSERT(t, font->Lookup(U'丮') == nullptr);
    TR_ASSERT(t, font->NumPagesBuilt() == numBuilt + 1);
    TR_ASSERT(t, font->GetPage(0x4e)->bitmap.empty());
    // Past Unicode: nothing, and no page
    TR_ASSERT(t, font->Lookup((char32_t)0x110000) == nullptr);
    TR_ASSERT(t, font->NumPagesBuilt() == numBuilt + 1);
    return kTR_Pass;
}

DLL_EXPORT int test_glyphfont_bitmap(ITesting *t) {
    auto font = OpenAndale(t);
    for (char32_t codePoint : {U'A', U'│', U'─'}) {
        const GlyphFont::Page *page = nullptr;
        auto glyph = font->Lookup(codePoint, &page);
        TR_ASSERT(t, glyph != nullptr);
        TR_ASSERT(t, (glyph->x0 < glyph->x1) && (glyph->y0 < glyph->y1));
        TR_ASSERT(t, (glyph->x1 <= page->textureSize) && (glyph->y1 <= page->textureSize));
        TR_ASSERT(t, page->bitmap.size() == (size_t)page->textureSize * page->textureSize);
        TR_ASSERT(t, HasInk(page, glyph));
    }
    return kTR_Pass;
}

DLL_EXPORT int test_glyphfont_advance(ITesting *t) {
    auto font = OpenAndale(t);
    // Monospaced: every glyph the font has advances one cell
    TR_ASSERT(t, font->Lookup(U'W')->xadvance == font->CellAdvance());
    TR_ASSERT(t, font->Lookup(U'│')->xadvance == font->CellAdvance());
    // A missing glyph still takes its cell
    TR_ASSERT(t, font->MeasureText(U"A▎B") == 3 * font->CellAdvance());
    return kTR_Pass;
}

DLL_EXPORT int test_glyphfont_notafont(ITesting *t) {
    const uint8_t junk[] = {1, 2, 3, 4, 5, 6, 7, 8};
    TR_ASSERT(t, GlyphFont::Create(junk, sizeof(junk), 18) == nullptr);
    TR_ASSERT(t, GlyphFont::Create(nullptr, 0, 18) == nullptr);
    return kTR_Pass;
}

namespace {
    // The first of these the machine has - the tests below only run what they can
    std::filesystem::path FirstExisting(std::initializer_list<const char *> files) {
        for (auto file : files) {
            if (std::filesystem::exists(file)) {
                return file;
            }
        }
        return {};
    }
}

// A code point the font lacks comes from a fallback, into the same page; the fallback file is only read when a page
// needs it, and one that is not there is skipped
DLL_EXPORT int test_glyphfont_fallback(ITesting *t) {
    auto cjk = FirstExisting({"/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
                              "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
                              "/Library/Fonts/Arial Unicode.ttf"});
    auto font = OpenAndale(t);
    font->SetFallbackFiles({"/no/such/font.ttf", cjk});
    TR_ASSERT(t, font->Lookup(U'A') != nullptr);
    TR_ASSERT(t, font->NumFallbacksOpened() == 0);
    float advance = font->CellAdvance();
    if (cjk.empty()) {
        TR_ASSERT(t, font->Lookup(U'中') == nullptr);
        return kTR_Pass;
    }
    const GlyphFont::Page *page = nullptr;
    auto glyph = font->Lookup(U'中', &page);
    TR_ASSERT(t, glyph != nullptr);
    TR_ASSERT(t, page == font->GetPage(0x4e));
    TR_ASSERT(t, HasInk(page, glyph));
    TR_ASSERT(t, font->NumFallbacksOpened() == 1);
    // Wider than one cell (it is drawn across two), and the cell is still the font's own
    TR_ASSERT(t, (float)(glyph->x1 - glyph->x0) > advance);
    TR_ASSERT(t, font->CellAdvance() == advance);
    // The font's own glyphs are its own on a page shared with a fallback (Latin Extended-A: Andale + fallback)
    TR_ASSERT(t, font->Lookup(U'ő') != nullptr);
    return kTR_Pass;
}

// A colour emoji font's glyphs are bitmaps stb_truetype cannot draw: they are not taken (the cell stays empty
// instead of drawing nothing as if it were a glyph)
DLL_EXPORT int test_glyphfont_fallback_colouremoji(ITesting *t) {
    auto emoji = FirstExisting({"/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf"});
    if (emoji.empty()) {
        return kTR_Pass;
    }
    auto font = OpenAndale(t);
    font->SetFallbackFiles({emoji});
    TR_ASSERT(t, font->Lookup(U'😀') == nullptr);      // (stb_truetype does not open this one at all - no outlines)
    return kTR_Pass;
}

namespace {
    // A font with the block elements Andale Mono lacks ('▎'), and none of the /context grid ('⛁'): Menlo (a
    // collection, so the face is named) or DejaVu Sans Mono
    std::optional<SystemFontFace> BlockElementsFace() {
        if (std::filesystem::exists("/System/Library/Fonts/Menlo.ttc")) {
            return SystemFontFace{"/System/Library/Fonts/Menlo.ttc", -1, "Menlo-Regular"};
        }
        if (std::filesystem::exists("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf")) {
            return SystemFontFace{"/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 0, ""};
        }
        return {};
    }
}

// The system is asked only for what the font lacks, a face it names is read once for all the code points it draws,
// and a face without the glyph (or no file at all) draws nothing
DLL_EXPORT int test_glyphfont_systemfallback(ITesting *t) {
    auto blocks = BlockElementsFace();
    if (!blocks.has_value()) {
        return kTR_Pass;
    }
    auto font = OpenAndale(t);
    int numAsked = 0;
    font->SetSystemFallback([&](char32_t codePoint) -> std::optional<SystemFontFace> {
        numAsked++;
        if (codePoint == U'⛀') {
            return SystemFontFace{"/no/such/font.ttf", 0, ""};
        }
        return blocks;
    });
    TR_ASSERT(t, font->Lookup(U'A') != nullptr);
    TR_ASSERT(t, font->Lookup(U'ő') != nullptr);           // Andale's own: page 1 built, nothing asked for 'ő'
    int askedBefore = font->NumSystemLookups();

    const GlyphFont::Page *page = nullptr;
    auto glyph = font->Lookup(U'▎', &page);
    TR_ASSERT(t, glyph != nullptr);
    TR_ASSERT(t, HasInk(page, glyph));
    TR_ASSERT(t, font->Lookup(U'▏') != nullptr);
    TR_ASSERT(t, font->Lookup(U'│') != nullptr);           // Andale's own, on the same page
    TR_ASSERT(t, font->NumSystemFacesOpened() == 1);
    TR_ASSERT(t, font->NumSystemLookups() > askedBefore);
    TR_ASSERT(t, numAsked == font->NumSystemLookups());

    // Once per code point: looking again builds nothing and asks nothing
    int askedAfter = font->NumSystemLookups();
    TR_ASSERT(t, font->Lookup(U'▎') != nullptr);
    TR_ASSERT(t, font->NumSystemLookups() == askedAfter);

    // The face it names has no '⛁'; for '⛀' it names no file at all
    TR_ASSERT(t, font->Lookup(U'⛁') == nullptr);
    TR_ASSERT(t, font->Lookup(U'⛀') == nullptr);
    TR_ASSERT(t, font->NumSystemFacesOpened() == 1);
    return kTR_Pass;
}

// A configured fallback that has the glyph wins: the system is not asked for it
DLL_EXPORT int test_glyphfont_systemfallback_order(ITesting *t) {
    auto blocks = BlockElementsFace();
    if (!blocks.has_value()) {
        return kTR_Pass;
    }
    auto font = OpenAndale(t);
    font->SetFallbackFiles({blocks->file});
    bool isAskedForBlock = false;
    font->SetSystemFallback([&](char32_t codePoint) -> std::optional<SystemFontFace> {
        if (codePoint == U'▎') {
            isAskedForBlock = true;
        }
        return {};
    });
    TR_ASSERT(t, font->Lookup(U'▎') != nullptr);
    TR_ASSERT(t, !isAskedForBlock);
    TR_ASSERT(t, font->NumFallbacksOpened() == 1);
    TR_ASSERT(t, font->NumSystemFacesOpened() == 0);
    return kTR_Pass;
}

// A colour emoji font the system names is not read: its table directory says bitmaps ('sbix' / 'CBDT'), even when it
// carries a 'glyf' table of empty outlines too (Apple Color Emoji, ~190MB - reading it stalled a page build ~2s)
DLL_EXPORT int test_glyphfont_systemfallback_colouremoji(ITesting *t) {
    auto emoji = FirstExisting({"/System/Library/Fonts/Apple Color Emoji.ttc",
                                "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf"});
    if (emoji.empty()) {
        return kTR_Pass;
    }
    auto font = OpenAndale(t);
    font->SetSystemFallback([&](char32_t) -> std::optional<SystemFontFace> {
        return SystemFontFace{emoji, -1, "AppleColorEmoji"};
    });
    TR_ASSERT(t, font->Lookup(U'😀') == nullptr);
    TR_ASSERT(t, font->NumSystemLookups() > 0);
    TR_ASSERT(t, font->NumSystemFacesOpened() == 0);
    return kTR_Pass;
}

// The real lookup draws Claude Code's /context grid, which no configured font has (open-bugs #26). On macOS CoreText
// always finds one (STIX Two Math), never LastResort's box; on Linux it depends on what is installed.
DLL_EXPORT int test_glyphfont_systemlookup(ITesting *t) {
    auto face = SystemFonts::FindFaceFor(U'⛁');
#ifdef GEDIT_MACOS
    TR_ASSERT(t, face.has_value());
#endif
    if (!face.has_value()) {
        return kTR_Pass;
    }
    TR_ASSERT(t, std::filesystem::exists(face->file));
    TR_ASSERT(t, face->postScriptName != "LastResort");

    auto font = OpenAndale(t);
    font->SetSystemFallback(SystemFonts::FindFaceFor);
    for (auto codePoint : {U'⛀', U'⛁', U'⛶', U'⛝'}) {
        const GlyphFont::Page *page = nullptr;
        auto glyph = font->Lookup(codePoint, &page);
        TR_ASSERT(t, glyph != nullptr);
        TR_ASSERT(t, HasInk(page, glyph));
    }
    return kTR_Pass;
}
