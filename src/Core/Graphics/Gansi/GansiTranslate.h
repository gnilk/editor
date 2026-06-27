//
// gedit::Gansi translation helpers — bridge editor value types to gansi library types.
//
#ifndef GEDIT_GANSI_GANSITRANSLATE_H
#define GEDIT_GANSI_GANSITRANSLATE_H

#include <cstdint>

#include "Core/ColorRGBA.h"
#include "Core/TextAttributes.h"
#include "gansi/Types.h"

namespace gedit::Gansi {

    // ColorRGBA (float 0..1) -> gansi truecolor (0..255).
    inline gnilk::ansi::Color ToAnsiColor(const ColorRGBA &c) {
        return {
            static_cast<uint8_t>(c.RedAsInt()),
            static_cast<uint8_t>(c.GreenAsInt()),
            static_cast<uint8_t>(c.BlueAsInt()),
        };
    }

    // Resolve an editor text-attribute set into a gansi pen: cell attributes plus the (possibly
    // swapped, when inverted) fg/bg the cell should carry. Mirrors the SDL backend's inverse/underline
    // handling but in cell terms (no sub-cell pixels).
    inline void ResolvePen(kTextAttributes attrib, const ColorRGBA &fg, const ColorRGBA &bg,
                           gnilk::ansi::Color &outFg, gnilk::ansi::Color &outBg, gnilk::ansi::Attr &outAttr) {
        outFg = ToAnsiColor(fg);
        outBg = ToAnsiColor(bg);
        outAttr = gnilk::ansi::Attr::None;
        if (attrib & kTextAttributes::kBold) {
            outAttr |= gnilk::ansi::Attr::Bold;
        }
        if (attrib & kTextAttributes::kItalic) {
            outAttr |= gnilk::ansi::Attr::Italic;
        }
        if (attrib & kTextAttributes::kUnderline) {
            outAttr |= gnilk::ansi::Attr::Underline;
        }
        if (attrib & kTextAttributes::kInverted) {
            std::swap(outFg, outBg);
        }
    }

}

#endif // GEDIT_GANSI_GANSITRANSLATE_H
