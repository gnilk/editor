//
// Created by gnilk on 24.07.23.
//

#include "Core/Config/Config.h"
#include "Core/NamedColors.h"
#include "Core/Theme/Theme.h"
#include "ThemeAPI.h"

using namespace gedit;

bool ThemeAPI::Reload() {
    if (theme == nullptr) {
        return false;
    }

    return theme->Reload();
}

NamedColorsAPI::Ref ThemeAPI::GetColors(const std::string &clsColors) {
    if (theme == nullptr) {
        return nullptr;
    }
    if (!theme->HasColorsForClass(clsColors)) {
        return nullptr;
    }

    auto refColors = theme->GetColorsForClass(clsColors);

    return std::make_shared<NamedColorsAPI>(refColors);


}