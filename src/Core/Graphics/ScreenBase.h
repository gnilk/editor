//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_SCREENBASE_H
#define EDITOR_SCREENBASE_H

#include <utility>
#include <vector>
#include <functional>
#include "Core/Rect.h"
#include "Core/Graphics/WindowBase.h"
#include "Core/ColorRGBA.h"

namespace gedit {
    class ScreenBase {
    public:
        using Ref = std::shared_ptr<ScreenBase>;
        // Invoked (with live pixel geometry) whenever the window is moved/resized. The glue layer
        // (Editor) registers this to persist geometry into the session; the backend stays free of any
        // app/session dependency — geometry flows OUT through this callback, never the other way.
        using WindowGeometryChangedHandler = std::function<void(int x, int y, int width, int height)>;
    public:
        ScreenBase() = default;
        virtual ~ScreenBase() = default;
        virtual bool Open() { return false; }
        virtual void Close() {}
        virtual void Clear() {}
        virtual void Update() {}
        virtual bool UpdateClipboardData() { return false; }
        // Copy current screen to a texture (internally)
        virtual void CopyToTexture() {}
        // Clear screen with the copied texture
        virtual void ClearWithTexture() {}

        virtual void RegisterColor(int appIndex, const ColorRGBA &foreground, const ColorRGBA &background) {}


        // These two are not used..
        virtual void BeginRefreshCycle() {}
        virtual void EndRefreshCycle() {}


        virtual Rect Dimensions() {
            return {};
        };
        virtual WindowBase *CreateWindow(const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) { return nullptr; }
        virtual WindowBase *UpdateWindow(WindowBase *window, const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) { return nullptr; }

        virtual void OnSizeChanged() { }
        virtual void OnMoved() { }

        // Pump platform events and dispatch keyboard/window/clipboard events.
        // Must be called from the main thread. Blocks up to ~250 ms waiting for events.
        virtual void PollEvents() {}

        // --- Window geometry: plain-data API, no session/app dependency (docs/session-cache.md §4.2) ---
        // The glue layer supplies the geometry to open with (sourced from the session) and a callback to
        // receive live geometry. The backend turns the request into a sane on-screen window (default +
        // clamp, which need the display) and reports moves/resizes back through the callback.

        // Requested startup geometry in pixels (from the glue / session). A zero width/height means
        // "no preference" → the backend centres a default on the display. Call before Open().
        void SetRequestedWindowGeometry(int x, int y, int width, int height) {
            requestedX = x;
            requestedY = y;
            requestedWidth = width;
            requestedHeight = height;
            hasRequestedGeometry = (width > 0) && (height > 0);
        }
        void SetWindowGeometryChangedHandler(WindowGeometryChangedHandler handler) {
            geometryChangedHandler = std::move(handler);
        }

        // Pixel bounds (origin + size) of the primary display's usable area — a graphics concern.
        // Returns false if unavailable (then ResolveStartupGeometry falls back to a fixed size).
        virtual bool GetPrimaryDisplayBounds(int &x, int &y, int &width, int &height) { return false; }
    protected:
        // Resolve the geometry to create the window with: the requested geometry if one was supplied
        // (clamped to the display), otherwise a centred fraction of the display (never a hard-coded
        // 1920x1080 @ 0,0). Backends call this in Open(). No session/config knowledge here.
        void ResolveStartupGeometry(int &x, int &y, int &width, int &height);

        // Report live geometry to the registered handler (a no-op if none was set). Backends call this
        // on create + move/resize.
        void NotifyWindowGeometryChanged(int x, int y, int width, int height);
    private:
        bool hasRequestedGeometry = false;
        int requestedX = 0;
        int requestedY = 0;
        int requestedWidth = 0;
        int requestedHeight = 0;
        WindowGeometryChangedHandler geometryChangedHandler = nullptr;
    };
}

#endif //EDITOR_SCREENBASE_H
