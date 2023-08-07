//
// Created by gnilk on 21.01.23.
//

#ifndef EDITOR_CONFIG_H
#define EDITOR_CONFIG_H

#include <string>
#include <vector>
#include <assert.h>
#include <optional>
#include <logger.h>

#include "Core/NamedColors.h"
#include "Core/Theme/Theme.h"
#include "Core/Config/ConfigNode.h"

namespace gedit {

    class Config : public ConfigNode {
    public:
        static Config &Instance();

        // Load configuration and alos the theme file
        bool LoadSystemConfig(const std::string &filename);
        bool MergeUserConfig(const std::string &filename, bool replace);

        const Theme::Ref GetTheme() {
            return theme;
        }

    protected:
        bool LoadTheme(const std::string &themeFile);

    private:
        Config();   // Hide CTOR...
        gnilk::Logger::ILogger *logger = nullptr;
        Theme::Ref theme = nullptr;
        void SetDefaultsIfMissing();
    };
}


#endif //EDITOR_CONFIG_H
