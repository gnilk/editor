//
// Created by gnilk on 22.07.23.
//

#ifndef EDITOR_THEME_H
#define EDITOR_THEME_H

#include <memory>
#include <string>
#include "logger.h"

#include "Core/NamedColors.h"
#include "Core/Config/ConfigNode.h"

namespace gedit {
    class Theme : public ConfigNode {
    public:
        using Ref = std::shared_ptr<Theme>;
        // Note: When adding a new color class - make sure to enhance 'IsValidColorClass' function!!
        inline static const std::string clrClassGlobals = "globals";
        inline static const std::string clrClassUI = "ui";
        inline static const std::string clrClassContent = "content";
        inline static const std::string clrClassTerminal = "terminal";
    public:
        Theme() = default;
        virtual ~Theme() = default;

        static Ref Create();

        const std::string &GetName() {
            return name;
        }

        bool Load(const std::string &pathname);
        bool Reload();

        bool HasColorsForClass(const std::string &clrClass) {
            if (colorConfig.find(clrClass) == colorConfig.end()) return false;
            return true;
        }


        NamedColors::Ref GetColorsForClass(const std::string &clrClass) {
            if (!HasColorsForClass(clrClass)) {
                return nullptr;
            }
            return colorConfig[clrClass];
        }

        // Returns the current color configuration
        const NamedColors &GetGlobalColors() {
            return *colorConfig[clrClassGlobals];
        }

        const NamedColors &GetContentColors() {
            return *colorConfig[clrClassContent];
        }

        const NamedColors &GetUIColors() {
            return *colorConfig[clrClassUI];
        }
        const NamedColors &GetTerminalColor() {
            return *colorConfig[clrClassTerminal];
        }

    public:
        bool Initialize();
    private:
        bool IsValidColorClass(const std::string &name);
        bool LoadSublimeColorFile(std::string filename);

        template<typename T, typename E>
        void ParseVariablesInScript(const T &from, E &scriptEngine);

        template<typename T, typename E>
        void SetNamedColorsFromScript(NamedColors::Ref dstColorConfig, const T &from, E &scriptEngine);

    private:
        gnilk::ILogger *logger = nullptr;
        std::string filename = {};
        std::string name = "noname";
        std::unordered_map<std::string, NamedColors::Ref> colorConfig;
    };
}


#endif //EDITOR_THEME_H
