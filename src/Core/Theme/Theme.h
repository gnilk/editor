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
        // Shipped with the editor - what a theme that can't be loaded falls back to
        static constexpr const char *DEFAULT_THEME = "default.theme.yml";
    public:
        Theme() = default;
        virtual ~Theme() = default;

        static Ref Create();
        // 'themeFile', or the shipped default when it can't be loaded - 'outProblem' then says why (empty when the
        // theme loaded). nullptr only when the default can't be loaded either.
        static Ref LoadOrDefault(const std::string &themeFile, std::string &outProblem);

        const std::string &GetName() {
            return name;
        }

        // False when the theme or its colour file is missing or can't be parsed - GetLoadError() says which. A failed
        // load leaves the theme without colours; the caller picks another (Editor::ConfigureTheme falls back to the
        // shipped default).
        bool Load(const std::string &pathname);
        // Load the same file again. On failure the theme keeps the colours it had.
        bool Reload();
        const std::string &GetLoadError() const {
            return loadError;
        }

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

        // Returns the current color configuration - an empty table for a class the theme doesn't have (never a null
        // dereference: open-bugs #29 was operator[] inserting a null Ref here)
        const NamedColors &GetGlobalColors() {
            return ColorsOrEmpty(clrClassGlobals);
        }

        const NamedColors &GetContentColors() {
            return ColorsOrEmpty(clrClassContent);
        }

        const NamedColors &GetUIColors() {
            return ColorsOrEmpty(clrClassUI);
        }
        const NamedColors &GetTerminalColor() {
            return ColorsOrEmpty(clrClassTerminal);
        }

    public:
        bool Initialize();
    private:
        const NamedColors &ColorsOrEmpty(const std::string &clrClass) const;
        bool LoadThemeFile(const std::string &pathname);
        bool IsValidColorClass(const std::string &name);
        bool LoadSublimeColorFile(std::string filename);
        void InheritGlobalColors();

        template<typename T, typename E>
        void ParseVariablesInScript(const T &from, E &scriptEngine);

        template<typename T, typename E>
        void SetNamedColorsFromScript(NamedColors::Ref dstColorConfig, const T &from, E &scriptEngine);

    private:
        gnilk::ILogger *logger = nullptr;
        std::string filename = {};
        std::string name = "noname";
        std::string loadError;
        std::unordered_map<std::string, NamedColors::Ref> colorConfig;
    };
}


#endif //EDITOR_THEME_H
