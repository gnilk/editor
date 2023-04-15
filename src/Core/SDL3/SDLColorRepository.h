//
// Created by gnilk on 30.03.23.
//

#ifndef EDITOR_SDLCOLORREPOSITORY_H
#define EDITOR_SDLCOLORREPOSITORY_H

#include <map>
#include <utility>
#include <SDL3/SDL.h>
#include "Core/ColorRGBA.h"
#include "Core/Config/Config.h"
#include "SDLColor.h"

#include "logger.h"


namespace gedit {
    // Singleton used to manage colors within SDL
    class SDLColorRepository {
    public:
        virtual ~SDLColorRepository() = default;
        static SDLColorRepository &Instance() {
            static SDLColorRepository glbColorRepository;
            glbColorRepository.Initialize();
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
        void UseBackgroundColor(SDL_Renderer *renderer, int otherAlpha) {
            backgroundColor.Use(renderer, otherAlpha);
        }

        void SetCursorColor(const SDLColor &newCursorColor) {
            cursorColor = newCursorColor;
        }

        void UseCursorColor(SDL_Renderer *renderer) {
            cursorColor.Use(renderer);
        }
    protected:
        void Initialize() {
            if (isInitialized) {
                return;
            }
            auto logger = gnilk::Logger::GetLogger("SDLColorRepo");
            if (!Config::Instance().ColorConfiguration().HasColor("background")) {
                logger->Warning("No background color found - assigning default");
                // Auto assign background here..
                backgroundColor = SDLColor(ColorRGBA::FromRGBA(46, 54, 62, 255));
            } else {
                backgroundColor = SDLColor(Config::Instance().ColorConfiguration().GetColor("background"));
            }


            if (!Config::Instance().ColorConfiguration().HasColor("caret")) {
                logger->Warning("No cursor color found - assigning default");
                // Auto assign background here..
                cursorColor = SDLColor(ColorRGBA::FromRGBA(46, 54, 62, 255));
            } else {
                cursorColor = SDLColor(Config::Instance().ColorConfiguration().GetColor("caret"));
            }
        }

    private:
        SDLColorRepository() = default;
    private:
        bool isInitialized = false;
        SDLColor backgroundColor;   // Special for background
        SDLColor cursorColor;       // Special for cursor

        // These are the original colors
        std::map<int, std::pair<ColorRGBA, ColorRGBA> > colors;
        // We also pre convert directly here
        std::map<int, std::pair<SDLColor, SDLColor> > sdlcolors;
    };
}


#endif //EDITOR_SDLCOLORREPOSITORY_H
