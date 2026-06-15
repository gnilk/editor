//
// Created by gnilk on 15.06.26.
//
// Shared window-geometry helpers for the rendering backends (docs/session-cache.md §4.2). Pure graphics:
// turn a requested geometry (supplied by the glue layer) into a sane on-screen window using the display
// bounds, and report live geometry back through a callback. NO session/app dependency lives here — the
// glue (Editor) owns reading/writing the session and only exchanges plain data with the backend.
//

#include "Core/Graphics/ScreenBase.h"

using namespace gedit;

// Fraction of the display the window fills when no geometry was requested.
static constexpr float kDefaultFraction = 0.8f;
// Last-resort size when even the display bounds are unavailable (headless / odd driver).
static constexpr int kFallbackWidth = 1280;
static constexpr int kFallbackHeight = 800;

// Keep a window fully inside the display work area: shrink if larger, then nudge the origin so the far
// edge doesn't fall off-screen, then pin the origin to the top-left of the bounds.
static void ClampToBounds(int &x, int &y, int &width, int &height, int dx, int dy, int dw, int dh) {
    if (width > dw) {
        width = dw;
    }
    if (height > dh) {
        height = dh;
    }
    if ((x + width) > (dx + dw)) {
        x = (dx + dw) - width;
    }
    if ((y + height) > (dy + dh)) {
        y = (dy + dh) - height;
    }
    if (x < dx) {
        x = dx;
    }
    if (y < dy) {
        y = dy;
    }
}

void ScreenBase::ResolveStartupGeometry(int &x, int &y, int &width, int &height) {
    int dx = 0, dy = 0, dw = 0, dh = 0;
    bool haveBounds = GetPrimaryDisplayBounds(dx, dy, dw, dh);

    if (hasRequestedGeometry) {
        x = requestedX;
        y = requestedY;
        width = requestedWidth;
        height = requestedHeight;
        if (haveBounds) {
            ClampToBounds(x, y, width, height, dx, dy, dw, dh);
        }
        return;
    }

    // No request: centre a fraction of the display (or a fixed fallback if the display is unknown).
    if (haveBounds) {
        width = static_cast<int>(dw * kDefaultFraction);
        height = static_cast<int>(dh * kDefaultFraction);
        x = dx + (dw - width) / 2;
        y = dy + (dh - height) / 2;
    } else {
        width = kFallbackWidth;
        height = kFallbackHeight;
        x = 0;
        y = 0;
    }
}

void ScreenBase::NotifyWindowGeometryChanged(int x, int y, int width, int height) {
    if (geometryChangedHandler != nullptr) {
        geometryChangedHandler(x, y, width, height);
    }
}
