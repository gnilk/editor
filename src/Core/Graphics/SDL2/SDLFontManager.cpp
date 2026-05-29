//
// Created by gnilk on 29.03.23.
//

#include "SDLFontManager.h"

using namespace gedit;
using namespace gedit::SDL2;

SDLFontManager &SDLFontManager::Instance() {
    static SDLFontManager glbInstance;
    return glbInstance;
}


