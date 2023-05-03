//
// Created by gnilk on 03.05.23.
//

#ifndef GEDIT_PLUGINCOMMAND_H
#define GEDIT_PLUGINCOMMAND_H

#include <memory>
#include <string>
#include <vector>

#include "logger.h"

#include "Core/Config/Config.h"

namespace gedit {
    class PluginCommand;

    template<typename T>
    class PluginExecutor {
    public:
        PluginExecutor() = default;
        virtual ~PluginExecutor() = default;
    };

    class PluginCommand {
    public:
        using Ref = std::shared_ptr<PluginCommand>;
        struct Argument {
            std::string typeName;
            std::string description;
        };
    public:
        PluginCommand() = default;
        static Ref CreateFromConfig(const ConfigNode &cfgNode);

        // You should override this one...
        virtual bool Execute(const std::vector<std::string> &args) {
            return false;
        }

        // Accessors
        const std::string &GetName() const {
            return name;
        }
        const std::string &GetShortName() const {
            return shortName;
        }
        const std::string &GetDescription() const {
            return description;
        }
        const std::vector<Argument> &GetArgumentList() const {
            return argumentList;
        }

        bool IsLoaded() const {
            return isLoaded;
        }

        bool IsValid() const {
            return isValid;
        }
    protected:
        virtual bool InitializeFromConfig(const ConfigNode &cfgNode);
    protected:
        std::string name;
        std::string shortName;
        std::string description;
        std::vector<Argument> argumentList;
    protected:
        bool isLoaded = false;
        bool isValid = false;
    private:
        gnilk::ILogger *logger = nullptr;
    };
}
#endif //EDITOR_PLUGINCOMMAND_H
