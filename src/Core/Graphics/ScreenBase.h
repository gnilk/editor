//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_SCREENBASE_H
#define EDITOR_SCREENBASE_H

#include <utility>
#include <vector>
#include "Core/Rect.h"
#include "Core/Graphics/WindowBase.h"
#include "Core/ColorRGBA.h"

namespace gedit {
    class ScreenBase {
    public:
        using Ref = std::shared_ptr<ScreenBase>;
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
    protected:
    };
}

#endif //EDITOR_SCREENBASE_H
