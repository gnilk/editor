//
// Created by gnilk on 21.01.23.
//

#ifndef EDITOR_CONFIG_H
#define EDITOR_CONFIG_H

#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

class ConfigNode {
public:
    ConfigNode() = default;

    explicit ConfigNode(const YAML::Node &node) : dataNode(node) {

    }

    ConfigNode operator [](const std::string &key) const {
        return ConfigNode(dataNode[key]);
    }

    bool HasKey(const std::string &key) {
        return dataNode[key].IsDefined();
    }

    int GetInt(const std::string &key, const int defValue = 0) {
        if (!HasKey(key)) {
            return defValue;
        }
        return dataNode[key].as<int>();
    }

    bool GetBool(const std::string &key, const bool defValue = false) {
        if (!HasKey(key)) {
            return defValue;
        }
        return dataNode[key].as<bool>();
    }

    std::string GetStr(const std::string &key, const std::string &defValue = "") {
        // If not defined, return default
        if (!dataNode[key].IsDefined()) {
            return {defValue};
        }
        return (dataNode[key].as<std::string>());
    }

    auto GetSequenceOfStr(const std::string &key) {
        if (!HasKey(key)) {
            return std::vector<std::string>();
        }
        return dataNode[key].as<std::vector<std::string>>();
    }

protected:
    YAML::Node dataNode;
};


class Config : public ConfigNode {
public:

    static Config &Instance();
    bool LoadConfig(const std::string &filename);

private:
    Config();   // Hide CTOR...
};


#endif //EDITOR_CONFIG_H
