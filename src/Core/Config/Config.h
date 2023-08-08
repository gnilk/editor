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
        bool MergeUserConfig(const std::string &filename, bool replace);



    private:
        Config();   // Hide CTOR...
        gnilk::Logger::ILogger *logger = nullptr;
        void SetDefaultsIfMissing();
    };
}


#endif //EDITOR_CONFIG_H
