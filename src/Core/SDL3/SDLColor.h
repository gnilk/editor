//
// Created by gnilk on 30.03.23.
//

#ifndef EDITOR_SDLCOLOR_H
#define EDITOR_SDLCOLOR_H

#include <SDL3/SDL.h>
#include "Core/ColorRGBA.h"

namespace gedit {
    class SDLColor {
    public:
        SDLColor() = default;
        SDLColor(const ColorRGBA &col) {
            r = col.RedAsInt();
            g = col.GreenAsInt();
            b = col.BlueAsInt();
            a = 255 - col.AlphaAsInt();
        }
        void Use(SDL_Renderer *renderer) {
            SDL_SetRenderDrawColor(renderer, r,g,b,a);
        }
    private:
        int r = 255;
        int g = 255;
        int b = 255;
        int a = 255;
    };
}

#endif //EDITOR_SDLCOLOR_H
