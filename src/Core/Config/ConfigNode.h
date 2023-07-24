//
// Created by gnilk on 22.07.23.
//

#ifndef EDITOR_CONFIGNODE_H
#define EDITOR_CONFIGNODE_H

#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>
#include <map>

namespace gedit {
    class ConfigNode {
    public:
        ConfigNode() = default;
        virtual ~ConfigNode() = default;

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

        void SetBool(const std::string &key, bool newValue) {
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
        auto GetSequence(const std::string &key) const {
            if (!HasKey(key)) {
                return T();
            }
            return dataNode[key].as<std::vector<T>>();
        }
        // I get a warning here about const but without it I get an error... lovely...
        auto GetSequenceOfStr(const std::string &key) const {
            if (!HasKey(key)) {
                return std::vector<std::string>();
            }
            return dataNode[key].as<std::vector<std::string>>();
        }

        auto GetMap(const std::string &key) const {
            if (HasKey(key)) {
                return dataNode[key].as<std::map<std::string, std::string>>();
            }
            return std::map<std::string, std::string>();
        }

        virtual bool LoadConfig(const std::string &filename);

        static std::optional<ConfigNode> FromString(const std::string yamlData) {
            ConfigNode cfgNode;
            cfgNode.dataNode = YAML::Load(yamlData);
            if (!cfgNode.dataNode.IsDefined()) {
                return {};
            }
            return cfgNode;
        }

        // This is more like a last resort kind of thing...
        const YAML::Node &GetDataNode() const {
            return dataNode;
        }


    protected:
        YAML::Node dataNode;
    };

}

#endif //EDITOR_CONFIGNODE_H
