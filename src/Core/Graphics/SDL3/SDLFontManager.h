//
// Created by gnilk on 29.03.23.
//

#ifndef STBMEETSDL_SDL3_SDLFONTMANAGER_H
#define STBMEETSDL_SDL3_SDLFONTMANAGER_H

#include "SDLFont.h"


namespace gedit::SDL3 {
    class SDLFontManager {
    public:
        virtual ~SDLFontManager() = default;
        static SDLFontManager &Instance();

        SDLFont *GetActiveFont() {
            return font.get();
        }
        // Null closes the font - before the renderer goes, its textures are the renderer's
        void SetActiveFont(SDLFont::Ptr newFont) {
            font = std::move(newFont);
        }

    private:
        SDLFontManager() = default;
    private:
        SDLFont::Ptr font;

    };
}


#endif //STBMEETSDL_SDL3_SDLFONTMANAGER_H
