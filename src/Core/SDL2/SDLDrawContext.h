//
// Created by gnilk on 29.03.23.
//

#ifndef STBMEETSDL_SDLDRAWCONTEXT_H
#define STBMEETSDL_SDLDRAWCONTEXT_H

#include <SDL2/SDL.h>

#include "Core/DrawContext.h"
#include "Core/ColorRGBA.h"
#include "SDLColor.h"
#include "SDLWindow.h"

namespace gedit {
    class SDLDrawContext : public DrawContext {
        friend SDLWindow;
    public:
        SDLDrawContext() = default;
        explicit SDLDrawContext(SDL_Renderer *sdlRenderer, SDL_Texture *sdlRenderTarget, Rect clientRect) :
            renderer(sdlRenderer),
            renderTarget(sdlRenderTarget),
            DrawContext((NativeWindow)sdlRenderTarget, clientRect) {        // Pass this down as the window - sometimes this pointer is checked for validity
        }
        virtual ~SDLDrawContext() = default;

        void Clear() const override;
        void Scroll(int nRows) const override;

        void ClearLine(int y) const override;
        void FillLine(int y, kTextAttributes attrib, char c) const override;
        void DrawLineOverlays(int y) const override;

        void DrawStringAt(int x, int y, const char *str) const override;
        void DrawStringWithAttributesAt(int x, int y, kTextAttributes attrib, const char *str) const override;
        void DrawStringWithAttributesAndColAt(int x, int y, kTextAttributes attrib, int idxColor, const char *str) const override;
    protected:
        void SetRenderColor() const;
        void SetRenderColor(kTextAttributes attrib) const;

    protected:
        void DrawLineOverlay(int y, const Overlay &overlay) const;

        // Fill Rect use current color
        void FillRect(float x, float y, float w, float h, bool isColorSet = false) const;
        // DrawLine use current color
        void DrawLine(float x1, float y1, float x2, float y2) const;

        // Draw a line with a specific pixel offset - applied after editor -> screen transformation has happened
        void DrawLineWithPixelOffset(float x1, float y1, float x2, float y2, float ofsX = 0.0f, float ofsY = 0.0f) const;

        std::pair<float, float>CoordsToScreen(float x, float y) const;

    private:
        SDL_Renderer *renderer;
        SDL_Texture *renderTarget;
    };
}


#endif //STBMEETSDL_SDLDRAWCONTEXT_H
