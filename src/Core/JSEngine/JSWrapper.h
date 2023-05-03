//
// Created by gnilk on 27.03.23.
//

#ifndef GEDIT_EMBDUKTAPEJS_JSWRAPPER_H
#define GEDIT_EMBDUKTAPEJS_JSWRAPPER_H

#include <string>
#include <string_view>
#include <vector>
#include <memory>

#include "duktape.h"
#include "logger.h"

#include "Core/AssetLoaderBase.h"
#include "Core/Config/Config.h"

#include "Core/Plugins/PluginCommand.h"

#include "JSPluginCommand.h"


namespace gedit {
    class JSWrapper : PluginExecutor<JSPluginCommand> {
    public:
    public:
        bool Initialize();
        bool RunScriptOnce(const std::string &script, const std::vector<std::string> &args);
        bool RunScriptOnce(const std::string_view script, const std::vector<std::string> &args);
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
