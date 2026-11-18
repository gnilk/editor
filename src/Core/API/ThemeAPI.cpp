//
// Created by gnilk on 24.07.23.
//

#include "Core/Config/Config.h"
#include "Core/Editor.h"
#include "Core/NamedColors.h"
#include "Core/Theme/Theme.h"
#include "Core/UI/UIHost.h"
#include "ThemeAPI.h"

using namespace gedit;

bool ThemeAPI::Reload() {
    if (theme == nullptr) {
        return false;
    }

    if (!theme->Reload()) {
        return false;
    }

    // The colours everywhere follow - the UI and syntax colours, the terminal's palette (it re-pushed only the UI's,
    // so the syntax colours kept the old ones)
    Editor::Instance().ApplyTheme();
    return true;
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