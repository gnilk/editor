//
// Created by gnilk on 21.01.23.
//

#ifndef EDITOR_CONFIG_H
#define EDITOR_CONFIG_H

#include <string>
#include <vector>
#include "Core/Language/LanguageBase.h"
#include "NamedColorConfig.h"
#include <yaml-cpp/yaml.h>

namespace gedit {
    class ConfigNode {
    public:
        ConfigNode() = default;

        explicit ConfigNode(const YAML::Node &node) : dataNode(node) {

        }

        ConfigNode operator[](const std::string &key) const {
            return ConfigNode(dataNode[key]);
        }

        ConfigNode GetNode(const std::string &key) const {
            return ConfigNode(dataNode[key]);
        }

        bool HasKey(const std::string &key) {
            if (!dataNode.IsDefined()) {
                return false;
            }
            return dataNode[key].IsDefined();
        }

        void SetStr(const std::string &key, const std::string &newValue) {
            dataNode[key] = newValue;
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
            if (!dataNode.IsDefined()) {
                return {defValue};
            }
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

        auto GetMap(const std::string &key) {
            if (HasKey(key)) {
                return dataNode[key].as<std::map<std::string, std::string>>();
            }
            return std::map<std::string, std::string>();
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
        const NamedColorConfig &GetGlobalColors() {
            return colorConfig["globals"];
        }

        const NamedColorConfig &GetContentColors() {
            return colorConfig["content"];
        }

        const NamedColorConfig &GetUIColors() {
            return colorConfig["ui"];
        }


    protected:
        bool LoadSublimeColorFile(const std::string &filename);

        // Using templates here to avoid including the header files of the currently only supported script engine
        // I simply want to hide the implementation details from the header file..
        // This could also have been done with simple static C-functions (i.e. not bound to object)
        // But I wanted to test another method...
        template<typename T, typename E>
        void ParseVariablesInScript(const T &from, E &scriptEngine);

        template<typename T, typename E>
        void SetNamedColorsFromScript(NamedColorConfig &dstColorConfig, const T &from, E &scriptEngine);

    private:
        Config();   // Hide CTOR...

        std::unordered_map<std::string, NamedColorConfig> colorConfig;

        void SetDefaultsIfMissing();

        std::unordered_map<std::string, LanguageBase *> extToLanguages;

    };
}


#endif //EDITOR_CONFIG_H
