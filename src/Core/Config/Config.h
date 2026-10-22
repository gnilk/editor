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

#include "Core/Config/ConfigNode.h"

namespace gedit {

    class Config : public ConfigNode {
    public:
        static Config &Instance();

        // Load configuration and alos the theme file
        bool LoadSystemConfig(const std::string &filename);
        void TryMergeUserConfig(const std::string &filename, bool replace);
        // Merge '<root>/.goatedit/<filename>' (AssetLoader kProject) on top of system+user. A project
        // config travels with the repo, so keys that run commands/code or write files are stripped first.
        void TryMergeProjectConfig(const std::string &filename);
        // Removes the keys a project config may not set; returns them as "section.key" (for logging/tests).
        static std::vector<std::string> StripProjectUnsafeKeys(ConfigNode &projectConfig);


    private:
        Config();   // Hide CTOR...
        gnilk::Logger::ILogger *logger = nullptr;
        void SetDefaultsIfMissing();
    };
}


#endif //EDITOR_CONFIG_H
