//
// Created by gnilk on 24.07.23.
//

#include "Core/Config/Config.h"
#include "Core/Theme/Theme.h"
#include "ThemeAPI.h"

using namespace gedit;

bool ThemeAPI::Reload() {
    if (theme == nullptr) {
        return false;
    }

    return theme->Reload();
}
