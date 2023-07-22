//
// Created by gnilk on 21.01.23.
//

#ifndef EDITOR_CONFIG_H
#define EDITOR_CONFIG_H

#include <string>
#include <vector>
#include <assert.h>
#include <optional>
#include "NamedColorConfig.h"
#include "Core/Theme/Theme.h"
#include "Core/Config/ConfigNode.h"

namespace gedit {

    class Config : public ConfigNode {
    public:
        static Config &Instance();

        // Load configuration and alos the theme file
        bool LoadConfig(const std::string &filename) override;

        const Theme::Ref GetTheme() {
            return theme;
        }

    protected:
        bool LoadTheme(const std::string &themeFile);

    private:
        Config();   // Hide CTOR...

        Theme::Ref theme = nullptr;
        void SetDefaultsIfMissing();
    };
}


#endif //EDITOR_CONFIG_H
