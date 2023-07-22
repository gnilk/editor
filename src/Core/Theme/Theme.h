//
// Created by gnilk on 22.07.23.
//

#ifndef EDITOR_THEME_H
#define EDITOR_THEME_H

#include <memory>
#include <string>
#include "logger.h"

#include "Core/Config/NamedColorConfig.h"
#include "Core/Config/ConfigNode.h"

namespace gedit {
    class Theme : public ConfigNode {
    public:
        using Ref = std::shared_ptr<Theme>;
    public:
        Theme() = default;
        virtual ~Theme() = default;

        static Ref Create();

        const std::string &GetName() {
            return name;
        }

        bool Load(const std::string &pathname);
        bool Reload();

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

    public:
        bool Initialize();
    private:
        bool LoadSublimeColorFile(std::string filename);

        template<typename T, typename E>
        void ParseVariablesInScript(const T &from, E &scriptEngine);

        template<typename T, typename E>
        void SetNamedColorsFromScript(NamedColorConfig &dstColorConfig, const T &from, E &scriptEngine);

    private:
        gnilk::ILogger *logger = nullptr;
        std::string filename = {};
        std::string name = "noname";
        std::unordered_map<std::string, NamedColorConfig> colorConfig;
    };
}


#endif //EDITOR_THEME_H
