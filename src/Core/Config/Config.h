//
// Created by gnilk on 21.01.23.
//

#ifndef EDITOR_CONFIG_H
#define EDITOR_CONFIG_H

#include <string>
#include <vector>
#include "Core/Language/LanguageBase.h"
#include "ColorConfig.h"
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
    char GetChar(const std::string &key, char defValue) {
        if (!dataNode[key].IsDefined()) {
            return defValue;
        }
        return (dataNode[key].as<char>());
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

    void RegisterLanguage(const std::string &extension, LanguageBase *languageBase);
    LanguageBase *GetLanguageForFilename(const std::string &extension);

    // Load configuration include theme and color files
    bool LoadConfig(const std::string &filename);

    // Returns the current color configuration
    const ColorConfig &ColorConfiguration() {
        return colorConfig;
    }

protected:
    bool LoadSublimeColorFile(const std::string &filename);

private:
    Config();   // Hide CTOR...
    ColorConfig colorConfig;
    void SetDefaultsIfMissing();

    std::unordered_map<std::string, LanguageBase *> extToLanguages;

};


#endif //EDITOR_CONFIG_H
