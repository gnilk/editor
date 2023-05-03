//
// Created by gnilk on 03.05.23.
//

#ifndef EDITOR_JSPLUGINCOMMAND_H
#define EDITOR_JSPLUGINCOMMAND_H

#include <memory>
#include <string>


#include "logger.h"

#include "Core/Config/Config.h"
#include "Core/AssetLoaderBase.h"

namespace gedit {

    // This should go to it's own file...
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
        virtual bool Execute() {
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



    // Split this to another file...
    class JSPluginCommand : public PluginCommand {
    public:
        using Ref = std::shared_ptr<JSPluginCommand>;
    public:
        static JSPluginCommand::Ref CreateFromConfig(const ConfigNode &cfgNode);
        bool Execute() override;

        const std::string &GetScriptFile() const {
            return scriptFile;
        }
        // Extensions
    public:
        bool TryLoad();
    protected:
        bool InitializeFromConfig(const ConfigNode &cfgNode) override;

    protected:
        std::string scriptFile;                 // Consider moving this
        AssetLoaderBase::Asset::Ref scriptData;
    private:
        gnilk::ILogger *logger = nullptr;
    };

}
#endif //EDITOR_JSPLUGINCOMMAND_H
