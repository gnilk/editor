//
// Created by gnilk on 29.03.23.
//

#ifndef STBMEETSDL_SDL3_SDLFONTMANAGER_H
#define STBMEETSDL_SDL3_SDLFONTMANAGER_H

#include "ext/stbttf.h"


namespace gedit::SDL3 {
    class SDLFontManager {
    public:
        virtual ~SDLFontManager() = default;
        static SDLFontManager &Instance();

        STBTTF_Font *GetActiveFont() {
            return font;
        }
        void SetActiveFont(STBTTF_Font *newFont) {
            font = newFont;
        }

    private:
        SDLFontManager() = default;
    private:
        // Font must be public for now..
        STBTTF_Font* font = nullptr;

    };
}


#endif //STBMEETSDL_SDL3_SDLFONTMANAGER_H
