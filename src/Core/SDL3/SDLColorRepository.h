//
// Created by gnilk on 30.03.23.
//

#ifndef EDITOR_SDLCOLORREPOSITORY_H
#define EDITOR_SDLCOLORREPOSITORY_H

#include <map>
#include <utility>
#include <SDL3/SDL.h>
#include "Core/ColorRGBA.h"
#include "SDLColor.h"


namespace gedit {
    // Singleton used to manage colors within SDL
    class SDLColorRepository {
    public:
        virtual ~SDLColorRepository() = default;
        static SDLColorRepository &Instance() {
            static SDLColorRepository glbColorRepository;
            return glbColorRepository;
        }

        void RegisterColor(int appIndex, const ColorRGBA &fg, const ColorRGBA &bg) {
            colors[appIndex]=std::make_pair(fg,bg);
            sdlcolors[appIndex] = std::make_pair(SDLColor(fg), SDLColor(bg));
        }

        std::pair<ColorRGBA, ColorRGBA> GetOriginalColor(int appIndex) {
            return colors[appIndex];
        }

        std::pair<SDLColor, SDLColor> GetColor(int appIndex) {
            return sdlcolors[appIndex];
        }

        void UseFG(SDL_Renderer *renderer, int appIndex) {
            sdlcolors[appIndex].first.Use(renderer);
        }

        void UseBG(SDL_Renderer *renderer, int appIndex) {
            sdlcolors[appIndex].second.Use(renderer);
        }


        void SetBackgroundColor(const SDLColor &newBackgroundColor) {
            backgroundColor = newBackgroundColor;
        }

        void UseBackgroundColor(SDL_Renderer *renderer) {
            backgroundColor.Use(renderer);
        }

    private:
        SDLColorRepository() = default;
        SDLColor backgroundColor;   // Special for background
        // These are the original colors
        std::map<int, std::pair<ColorRGBA, ColorRGBA> > colors;
        // We also pre convert directly here
        std::map<int, std::pair<SDLColor, SDLColor> > sdlcolors;
    };
}


#endif //EDITOR_SDLCOLORREPOSITORY_H
