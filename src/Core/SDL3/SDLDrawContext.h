//
// Created by gnilk on 29.03.23.
//

#ifndef STBMEETSDL_SDLDRAWCONTEXT_H
#define STBMEETSDL_SDLDRAWCONTEXT_H

#include "Core/DrawContext.h"
#include "Core/ColorRGBA.h"
#include "SDLColor.h"
#include "SDLWindow.h"
#include <SDL3/SDL.h>

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

        void Clear() override;
        void DrawStringAt(int x, int y, const char *str) override;
        void DrawStringWithAttributesAt(int x, int y, kTextAttributes attrib, const char *str) override;

        void DrawLine(Line *line, int idxLine) override;
        void DrawLines(const std::vector<Line *> &lines, int idxTopLine, int idxBottomLine) override;
        void DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l) override;

        void ClearLine(int y) override;
        void FillLine(int y, kTextAttributes attrib, char c) override;
        void Scroll(int nRows) override;
    protected:

        void DrawStringWithAttributesAndColAt(int x, int y, kTextAttributes attrib, int idxColor, const char *str);


        // Fill Rect use current color
        void FillRect(float x, float y, float w, float h);
        // DrawLine use current color
        void DrawLine(float x1, float y1, float x2, float y2);

        // Draw a line with a specific pixel offset - applied after editor -> screen transformation has happened
        void DrawLineWithPixelOffset(float x1, float y1, float x2, float y2, float ofsX = 0.0f, float ofsY = 0.0f);

        std::pair<float, float>CoordsToScreen(float x, float y);

    private:
        SDL_Renderer *renderer;
        SDL_Texture *renderTarget;
    };
}


#endif //STBMEETSDL_SDLDRAWCONTEXT_H
