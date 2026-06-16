//
// Created by gnilk on 24.07.23.
//

#include "Core/Config/Config.h"
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

    // AI-6 (docs/ui-refactor.md §8): the UI toolkit reads colors through UIHost, never Theme
    // directly, so a runtime reload (this is JS-triggerable) must re-push fresh copies - mirrors the
    // initial push in Editor::ConfigureTheme.
    UIHost::Instance().SetUIColors(theme->GetUIColors());
    UIHost::Instance().SetGlobalColors(theme->GetGlobalColors());
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