//
// gedit::Gansi draw context — every draw virtual writes into the shared gansi cell grid. Coordinates
// are local to the owning window; the context offsets by the window origin and clips to the window
// rect (and the grid bounds). No pixels, no scaling.
//
#ifndef GEDIT_GANSI_GANSIDRAWCONTEXT_H
#define GEDIT_GANSI_GANSIDRAWCONTEXT_H

#include "Core/UI/Graphics/DrawContext.h"
#include "gansi/Terminal.h"
#include "gansi/Types.h"

namespace gedit::Gansi {

    class GansiDrawContext : public DrawContext {
    public:
        GansiDrawContext() = default;
        GansiDrawContext(gnilk::ansi::Terminal *_terminal, const Rect &clientRect)
            : DrawContext(clientRect), terminal(_terminal) {
        }
        ~GansiDrawContext() override = default;

        // Re-target after a window move/resize (windows are clip rects over one shared grid).
        void SetClientRect(const Rect &newRect) { rect = newRect; }

        void Clear() const override;
        void Scroll(int nRows) const override;
        void ClearLine(int y) const override;
        void FillLine(int y, kTextAttributes attrib, char c) const override;
        void DrawLineOverlays(int y) const override;
        void DrawHRule(int y) const override;

        void DrawStringAt(int x, int y, const char *str) const override;
        void DrawStringWithAttributesAt(int x, int y, kTextAttributes attrib, const char *str) const override;
        void DrawStringWithAttributesAndColAt(int x, int y, kTextAttributes attrib, int idxColor, const char *str) const override;

        void DrawStringAt(int x, int y, const std::u32string &str) const override;
        void DrawStringWithAttributesAt(int x, int y, kTextAttributes attrib, const std::u32string &str) const override;

    private:
        // Write one glyph cell at window-local (x,y) with an explicit pen. Clipped to window + grid.
        void PutCell(int x, int y, char32_t ch, gnilk::ansi::Color fg, gnilk::ansi::Color bg,
                     gnilk::ansi::Attr attr) const;
        void DrawString(int x, int y, kTextAttributes attrib, const std::u32string &str) const;

    private:
        gnilk::ansi::Terminal *terminal = nullptr;
    };

}

#endif // GEDIT_GANSI_GANSIDRAWCONTEXT_H
