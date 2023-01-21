//
// Created by gnilk on 21.01.23.
//

#include "Config.h"
#include <yaml-cpp/yaml.h>

Config::Config() : ConfigNode() {

}

Config &Config::Instance() {
    static Config glbConfig;
    return glbConfig;
}

bool Config::LoadConfig(const std::string &filename) {
    dataNode = YAML::LoadFile(filename);
    if (!dataNode.IsDefined()) {
        return false;
    }
    return true;
}

