//
// SystemFonts on macOS - CoreText's font cascade: the font CTFontCreateForString picks for the one character, the way
// any Cocoa app (CLion, Ghostty) finds a glyph its own font lacks. See Core/Graphics/SystemFonts.h.
//
#include "Core/Graphics/SystemFonts.h"

#include <CoreText/CoreText.h>

using namespace gedit;

namespace {
    // The font the cascade starts from - a monospaced one, so where several fonts have the glyph the pick leans the
    // same way. Created once, never released.
    CTFontRef BaseFont() {
        static CTFontRef base = CTFontCreateWithName(CFSTR("Menlo-Regular"), 12.0, nullptr);
        return base;
    }

    std::string ToStdString(CFStringRef str) {
        if (str == nullptr) {
            return {};
        }
        auto length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8) + 1;
        std::string result((size_t)length, '\0');
        if (!CFStringGetCString(str, result.data(), length, kCFStringEncodingUTF8)) {
            return {};
        }
        result.resize(strlen(result.c_str()));
        return result;
    }

    std::filesystem::path FileOf(CTFontRef font) {
        auto url = (CFURLRef)CTFontCopyAttribute(font, kCTFontURLAttribute);
        if (url == nullptr) {
            return {};
        }
        char path[PATH_MAX] = {};
        bool isOk = CFURLGetFileSystemRepresentation(url, true, (UInt8 *)path, sizeof(path));
        CFRelease(url);
        return isOk ? std::filesystem::path(path) : std::filesystem::path();
    }
}

std::optional<SystemFontFace> SystemFonts::FindFaceFor(char32_t codePoint) {
    if (BaseFont() == nullptr) {
        return {};
    }
    // The code point as UTF-16 - one unit, or a surrogate pair past the BMP
    UniChar units[2];
    CFIndex numUnits = 1;
    if (codePoint > 0xffff) {
        auto value = (uint32_t)codePoint - 0x10000;
        units[0] = (UniChar)(0xd800 + (value >> 10));
        units[1] = (UniChar)(0xdc00 + (value & 0x3ff));
        numUnits = 2;
    } else {
        units[0] = (UniChar)codePoint;
    }
    auto str = CFStringCreateWithCharacters(kCFAllocatorDefault, units, numUnits);
    if (str == nullptr) {
        return {};
    }
    auto font = CTFontCreateForString(BaseFont(), str, CFRangeMake(0, numUnits));
    CFRelease(str);
    if (font == nullptr) {
        return {};
    }

    SystemFontFace face;
    auto psName = CTFontCopyPostScriptName(font);
    face.postScriptName = ToStdString(psName);
    if (psName != nullptr) {
        CFRelease(psName);
    }
    face.file = FileOf(font);
    CFRelease(font);

    // LastResort has an outline for every code point - the 'no glyph' box
    if (face.file.empty() || (face.postScriptName == "LastResort")) {
        return {};
    }
    return face;
}
