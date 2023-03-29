//
// Created by gnilk on 29.03.23.
//

#ifndef STBMEETSDL_SDLDRAWCONTEXT_H
#define STBMEETSDL_SDLDRAWCONTEXT_H

#include "Core/DrawContext.h"
#include <SDL3/SDL.h>

namespace gedit {
    class SDLDrawContext : public DrawContext {
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


    private:
        SDL_Renderer *renderer;
        SDL_Texture *renderTarget;
    };
}


#endif //STBMEETSDL_SDLDRAWCONTEXT_H
