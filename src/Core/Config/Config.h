//
// Created by gnilk on 21.01.23.
//

#ifndef EDITOR_CONFIG_H
#define EDITOR_CONFIG_H

#include <string>
#include <vector>
#include <assert.h>
#include "NamedColorConfig.h"
#include <yaml-cpp/yaml.h>

namespace gedit {
    class ConfigNode {
    public:
        ConfigNode() = default;

        explicit ConfigNode(const YAML::Node &node) : dataNode(node) {

        }

        ConfigNode operator[](const std::string &key) const {
            assert(dataNode.IsMap());
            return ConfigNode(dataNode[key]);
        }

        ConfigNode operator[](size_t idx) const {
            assert(dataNode.IsSequence());
            return ConfigNode(dataNode[idx]);
        }


        ConfigNode GetNode(const std::string &key) const {
            return ConfigNode(dataNode[key]);
        }

        bool HasKey(const std::string &key) const {
            if (!dataNode.IsDefined()) {
                return false;
            }

            return dataNode[key].IsDefined();
        }

        void SetStr(const std::string &key, const std::string &newValue) {
            dataNode[key] = newValue;
        }

        int GetInt(const std::string &key, const int defValue = 0) const {
            if (!HasKey(key)) {
                return defValue;
            }
            return dataNode[key].as<int>();
        }

        bool GetBool(const std::string &key, const bool defValue = false) const {
            if (!HasKey(key)) {
                return defValue;
            }
            return dataNode[key].as<bool>();
        }

        std::string GetStr(const std::string &key, const std::string &defValue = "") const {
            // If not defined, return default
            if (!dataNode.IsDefined()) {
                return {defValue};
            }
            if (!dataNode[key].IsDefined()) {
                return {defValue};
            }
            return (dataNode[key].as<std::string>());
        }
        char GetChar(const std::string &key, char defValue) const {
            if (!dataNode[key].IsDefined()) {
                return defValue;
            }
            return (dataNode[key].as<char>());
        }

        template<typename T>
        const auto GetSequence(const std::string &key) const {
            if (!HasKey(key)) {
                return T();
            }
            return dataNode[key].as<std::vector<T>>();
        }
        const auto GetSequenceOfStr(const std::string &key) const {
            if (!HasKey(key)) {
                return std::vector<std::string>();
            }
            return dataNode[key].as<std::vector<std::string>>();
        }

        const auto GetMap(const std::string &key) const {
            if (HasKey(key)) {
                return dataNode[key].as<std::map<std::string, std::string>>();
            }
            return std::map<std::string, std::string>();
        }

        virtual bool LoadConfig(const std::string &filename) {
            dataNode = YAML::LoadFile(filename);
            if (dataNode.IsDefined()) {
                return true;
            }
            return false;
        }

        // This is more like a last resort kind of thing...
        const YAML::Node &GetDataNode() const {
            return dataNode;
        }


    protected:
        YAML::Node dataNode;
    };

    class Config : public ConfigNode {
    public:
        static Config &Instance();


        // Load configuration include theme and color files
        bool LoadConfig(const std::string &filename) override;

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
    };
}


#endif //EDITOR_CONFIG_H
