//
// Created by gnilk on 24.07.23.
//

#ifndef EDITOR_THEMEAPI_H
#define EDITOR_THEMEAPI_H

#include <memory>
#include "Core/Theme/Theme.h"
#include "NamedColorsAPI.h"

namespace gedit {
    class ThemeAPI {
    public:
        using Ref = std::shared_ptr<ThemeAPI>;
    public:
        ThemeAPI() = default;
        ThemeAPI(Theme::Ref refTheme) : theme(refTheme) {
        }

        virtual ~ThemeAPI() = default;

        bool Reload();
        NamedColorsAPI::Ref GetColors(const std::string &clsColors);

    private:
        Theme::Ref theme;
    };
}


#endif //EDITOR_THEMEAPI_H
