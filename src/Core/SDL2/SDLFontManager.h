//
// Created by gnilk on 29.03.23.
//

#ifndef STBMEETSDL_SDLFONTMANAGER_H
#define STBMEETSDL_SDLFONTMANAGER_H

#include "ext/stbttf.h"


namespace gedit {
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


#endif //STBMEETSDL_SDLFONTMANAGER_H
