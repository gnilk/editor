//
// Created by gnilk on 22.07.23.
//

#ifndef EDITOR_THEME_H
#define EDITOR_THEME_H

#include <memory>
#include <string>
#include "logger.h"

#include "Core/NamedColor.h"
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
        const NamedColor &GetGlobalColors() {
            return colorConfig["globals"];
        }

        const NamedColor &GetContentColors() {
            return colorConfig["content"];
        }

        const NamedColor &GetUIColors() {
            return colorConfig["ui"];
        }

    public:
        bool Initialize();
    private:
        bool LoadSublimeColorFile(std::string filename);

        template<typename T, typename E>
        void ParseVariablesInScript(const T &from, E &scriptEngine);

        template<typename T, typename E>
        void SetNamedColorsFromScript(NamedColor &dstColorConfig, const T &from, E &scriptEngine);

    private:
        gnilk::ILogger *logger = nullptr;
        std::string filename = {};
        std::string name = "noname";
        std::unordered_map<std::string, NamedColor> colorConfig;
    };
}


#endif //EDITOR_THEME_H
