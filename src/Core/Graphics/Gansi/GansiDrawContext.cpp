//
// gedit::Gansi draw context implementation.
//
#include "Core/Graphics/Gansi/GansiDrawContext.h"
#include "Core/Graphics/Gansi/GansiTranslate.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;
using namespace gedit::Gansi;

void GansiDrawContext::PutCell(int x, int y, char32_t ch, gnilk::ansi::Color fg, gnilk::ansi::Color bg,
                               gnilk::ansi::Attr attr) const {
    if (terminal == nullptr) {
        return;
    }
    // Clip to the window's local extent.
    if (x < 0 || y < 0 || x >= rect.Width() || y >= rect.Height()) {
        return;
    }
    int absCol = rect.TopLeft().x + x;
    int absRow = rect.TopLeft().y + y;
    // Clip to the actual grid.
    if (absCol < 0 || absRow < 0 || absCol >= terminal->Cols() || absRow >= terminal->Rows()) {
        return;
    }
    gnilk::ansi::Cell &cell = terminal->At(absCol, absRow);
    cell.ch = ch;
    cell.fg = fg;
    cell.bg = bg;
    cell.attr = attr;
}

void GansiDrawContext::Clear() const {
    gnilk::ansi::Color bg = ToAnsiColor(bgColor);
    for (int y = 0; y < rect.Height(); ++y) {
        for (int x = 0; x < rect.Width(); ++x) {
            PutCell(x, y, U' ', ToAnsiColor(fgColor), bg, gnilk::ansi::Attr::None);
        }
    }
}

void GansiDrawContext::ClearLine(int y) const {
    gnilk::ansi::Color bg = ToAnsiColor(bgColor);
    gnilk::ansi::Color fg = ToAnsiColor(fgColor);
    for (int x = 0; x < rect.Width(); ++x) {
        PutCell(x, y, U' ', fg, bg, gnilk::ansi::Attr::None);
    }
}

void GansiDrawContext::FillLine(int y, kTextAttributes attrib, char c) const {
    gnilk::ansi::Color fg, bg;
    gnilk::ansi::Attr attr;
    ResolvePen(attrib, fgColor, bgColor, fg, bg, attr);
    for (int x = 0; x < rect.Width(); ++x) {
        PutCell(x, y, static_cast<char32_t>(c), fg, bg, attr);
    }
}

void GansiDrawContext::Scroll(int /*nRows*/) const {
    // Parity with the SDL backends (no-op): the editor repaints the affected views rather than
    // relying on a hardware scroll. A scroll-region optimisation can be added later.
}

void GansiDrawContext::DrawHRule(int y) const {
    // Box-drawing horizontal line across the row, in the current fg color.
    gnilk::ansi::Color fg = ToAnsiColor(fgColor);
    gnilk::ansi::Color bg = ToAnsiColor(bgColor);
    for (int x = 0; x < rect.Width(); ++x) {
        PutCell(x, y, U'─', fg, bg, gnilk::ansi::Attr::None);
    }
}

void GansiDrawContext::DrawLineOverlays(int y) const {
    if (terminal == nullptr) {
        return;
    }

    // The overlay color is the application-set foreground: LineRender points it at the theme's
    // 'selection' color immediately before this call. A cell grid can't alpha-composite, so we
    // highlight by blending that color into each covered cell's BACKGROUND only — the glyph and its
    // fg are left intact so the text stays readable.
    //
    // NOTE: theme alphas are stored 0..255 (e.g. `alpha(224)`), NOT normalized 0..1 — see
    // SublimeConfigColorScript::ExecuteAlpha. Using the raw value as a blend factor overflows the
    // uint8 cast below (the `c * (1 - a)` term goes hugely negative and wraps), which is what turned
    // the selection green. Normalize first; a color that already uses a 0..1 alpha is left as-is.
    float a = fgColor.A();
    if (a > 1.0f) {
        a /= 255.0f;
    }
    // Invert for now - this gives better results - but we should get this under control
    a = 1.0 - a;
    const gnilk::ansi::Color ovl = ToAnsiColor(fgColor);

    auto blend = [a, &ovl](const gnilk::ansi::Color &c) -> gnilk::ansi::Color {
        return {
            static_cast<uint8_t>(c.r * (1.0f - a) + ovl.r * a),
            static_cast<uint8_t>(c.g * (1.0f - a) + ovl.g * a),
            static_cast<uint8_t>(c.b * (1.0f - a) + ovl.b * a),
        };
    };

    for (auto &overlay : overlays) {
        if (!overlay.isActive || !overlay.IsLinePartiallyCovered(y)) {
            continue;
        }
        int start = 0;
        int end = rect.Width();
        if (y == overlay.start.y) {
            start = overlay.start.x;
        }
        if (y == overlay.end.y) {
            end = overlay.end.x;
        }
        for (int x = start; x < end; ++x) {
            if (x < 0 || y < 0 || x >= rect.Width() || y >= rect.Height()) {
                continue;
            }
            int absCol = rect.TopLeft().x + x;
            int absRow = rect.TopLeft().y + y;
            if (absCol < 0 || absRow < 0 || absCol >= terminal->Cols() || absRow >= terminal->Rows()) {
                continue;
            }
            gnilk::ansi::Cell &cell = terminal->At(absCol, absRow);
            cell.bg = blend(cell.bg);
        }
    }
}

void GansiDrawContext::DrawString(int x, int y, kTextAttributes attrib, const std::u32string &str) const {
    gnilk::ansi::Color fg, bg;
    gnilk::ansi::Attr attr;
    ResolvePen(attrib, fgColor, bgColor, fg, bg, attr);
    int col = x;
    for (char32_t ch : str) {
        PutCell(col, y, ch, fg, bg, attr);
        ++col;
    }
}

void GansiDrawContext::DrawStringAt(int x, int y, const std::u32string &str) const {
    DrawString(x, y, kTextAttributes::kNormal, str);
}

void GansiDrawContext::DrawStringWithAttributesAt(int x, int y, kTextAttributes attrib, const std::u32string &str) const {
    DrawString(x, y, attrib, str);
}

void GansiDrawContext::DrawStringAt(int x, int y, const char *str) const {
    DrawString(x, y, kTextAttributes::kNormal, UnicodeHelper::utf8to32(str));
}

void GansiDrawContext::DrawStringWithAttributesAt(int x, int y, kTextAttributes attrib, const char *str) const {
    DrawString(x, y, attrib, UnicodeHelper::utf8to32(str));
}

void GansiDrawContext::DrawStringWithAttributesAndColAt(int x, int y, kTextAttributes attrib, int /*idxColor*/, const char *str) const {
    // Indexed-color path is unused (the editor works in truecolor); treat as the attribute path.
    DrawString(x, y, attrib, UnicodeHelper::utf8to32(str));
}
