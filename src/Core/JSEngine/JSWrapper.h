//
// Created by gnilk on 27.03.23.
//

#ifndef GEDIT_EMBDUKTAPEJS_JSWRAPPER_H
#define GEDIT_EMBDUKTAPEJS_JSWRAPPER_H

#include <string>
#include <vector>
#include <memory>

#include "duktape.h"

#include "Core/Config/Config.h"


namespace gedit {
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
    };

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
    };


    class JSWrapper {
    public:
    public:
        bool Initialize();
        bool RunScriptOnce(const std::string &script, std::vector<std::string> &args);
        duk_context *GetContext() {
            return ctx;
        }
    protected:
        bool ConfigureNodeModuleSupport();
        void RegisterBuiltIns();
    private:
        std::vector<PluginCommand::Ref> plugins;
        duk_context *ctx;
    };
}

#endif //GEDIT_EMBDUKTAPEJS_JSWRAPPER_H
