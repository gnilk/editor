//
// SystemFonts on Linux - fontconfig: the fonts sorted for 'monospace' once, then the first of them whose charset has
// the code point (what a terminal does). Without fontconfig at build time nothing is found and the configured
// fallback list is all there is. See Core/Graphics/SystemFonts.h.
//
#include "Core/Graphics/SystemFonts.h"

#ifdef GEDIT_HAVE_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

using namespace gedit;

#ifdef GEDIT_HAVE_FONTCONFIG

namespace {
    // Every font, best match for 'monospace' first. Sorted once - a sort per code point is milliseconds, and a page
    // the font lacks asks for up to 256 of them. Never destroyed.
    FcFontSet *SortedFonts() {
        static FcFontSet *fonts = [] () -> FcFontSet * {
            if (!FcInit()) {
                return nullptr;
            }
            auto pattern = FcNameParse((const FcChar8 *)"monospace");
            if (pattern == nullptr) {
                return nullptr;
            }
            FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
            FcDefaultSubstitute(pattern);
            FcResult result;
            auto sorted = FcFontSort(nullptr, pattern, FcFalse, nullptr, &result);
            FcPatternDestroy(pattern);
            return sorted;
        }();
        return fonts;
    }
}

std::optional<SystemFontFace> SystemFonts::FindFaceFor(char32_t codePoint) {
    auto fonts = SortedFonts();
    if (fonts == nullptr) {
        return {};
    }
    for (int i = 0; i < fonts->nfont; i++) {
        auto font = fonts->fonts[i];
        FcCharSet *charSet = nullptr;
        if ((FcPatternGetCharSet(font, FC_CHARSET, 0, &charSet) != FcResultMatch) ||
            !FcCharSetHasChar(charSet, (FcChar32)codePoint)) {
            continue;
        }
        FcChar8 *file = nullptr;
        if (FcPatternGetString(font, FC_FILE, 0, &file) != FcResultMatch) {
            continue;
        }
        int index = 0;
        FcPatternGetInteger(font, FC_INDEX, 0, &index);
        SystemFontFace face;
        face.file = (const char *)file;
        face.index = index & 0xffff;        // the high bits are a variable font's named instance
        return face;
    }
    return {};
}

#else

std::optional<SystemFontFace> SystemFonts::FindFaceFor(char32_t) {
    return {};
}

#endif
