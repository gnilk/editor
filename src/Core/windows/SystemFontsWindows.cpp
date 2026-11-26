//
// SystemFonts on Windows - for now nothing: only the configured fonts and sdl.font_fallback (docs/windows-port.md §4.8,
// phase 3: DirectWrite's font fallback).
//

#include "Core/Graphics/SystemFonts.h"

using namespace gedit;

std::optional<SystemFontFace> SystemFonts::FindFaceFor(char32_t codePoint) {
    return std::nullopt;
}
