//
// Created by gnilk on 03.05.23.
//

#ifndef EDITOR_JSPLUGINCOMMAND_H
#define EDITOR_JSPLUGINCOMMAND_H

#include <memory>
#include <string>
#include <vector>


#include "logger.h"

#include "Core/Config/Config.h"
#include "Core/AssetLoaderBase.h"
#include "Core/Plugins/PluginCommand.h"

namespace gedit {

    class JSWrapper;
    class JSPluginCommand : public PluginCommand {
    public:
        using Ref = std::shared_ptr<JSPluginCommand>;
    public:
        static JSPluginCommand::Ref CreateFromConfig(const ConfigNode &cfgNode);
        bool Execute(const std::vector<std::string> &args) override;

        void SetExecutor(JSWrapper *newJsEngine);

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
        JSWrapper *jsEngine = nullptr;
        gnilk::ILogger *logger = nullptr;
    };

}
#endif //EDITOR_JSPLUGINCOMMAND_H
