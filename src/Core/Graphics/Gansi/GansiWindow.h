//
// gedit::Gansi window — a logical clip rect over the one shared gansi cell grid (no per-window
// buffer/texture). Its content DrawContext writes absolute cells = window origin + local, clipped.
//
#ifndef GEDIT_GANSI_GANSIWINDOW_H
#define GEDIT_GANSI_GANSIWINDOW_H

#include "Core/Graphics/Gansi/GansiDrawContext.h"
#include "Core/UI/Graphics/WindowBase.h"
#include "gansi/Terminal.h"

namespace gedit::Gansi {

    class GansiScreen;

    class GansiWindow : public WindowBase {
    public:
        GansiWindow(const Rect &rect, gnilk::ansi::Terminal *terminal, GansiScreen *screen);
        ~GansiWindow() override = default;

        DrawContext &GetContentDC() override { return clientContext; }

        void SetCursor(const Cursor &cursor) override;

        // Reposition / resize the clip rect (windows are just rects over the shared grid).
        void UpdateRect(const Rect &newRect);

    private:
        GansiScreen *screen = nullptr;
        GansiDrawContext clientContext;
    };

}

#endif // GEDIT_GANSI_GANSIWINDOW_H
