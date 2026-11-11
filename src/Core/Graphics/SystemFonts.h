//
// SystemFonts - ask the operating system which installed font draws a character (docs/system-font-fallback.md).
// The last step of GlyphFont's fallback, after the theme font and sdl.font_fallback. One implementation per platform:
// CoreText on macOS (src/Core/macOS/MacOSSystemFonts.cpp), fontconfig on Linux (src/Core/Linux/LinuxSystemFonts.cpp).
//
#ifndef GEDIT_SYSTEMFONTS_H
#define GEDIT_SYSTEMFONTS_H

#include <filesystem>
#include <optional>
#include <string>

namespace gedit {

    // A font file, and which face of it when it is a collection (.ttc): the index when the platform knows it, else
    // the face's PostScript name (index -1) for the reader to match against the collection's names
    struct SystemFontFace {
        std::filesystem::path file;
        int index = -1;
        std::string postScriptName;
    };

    namespace SystemFonts {
        // The font the system would draw the code point with; empty when there is none (or only the 'no glyph'
        // font). The face is the system's choice: whether it really has an outline is for the reader to check.
        std::optional<SystemFontFace> FindFaceFor(char32_t codePoint);
    }

}

#endif //GEDIT_SYSTEMFONTS_H
