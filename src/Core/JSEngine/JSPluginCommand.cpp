//
// Created by gnilk on 03.05.23.
//
#include <filesystem>

#include "Core/RuntimeConfig.h"

#include "JSPluginCommand.h"
#include "JSPluginEngine.h"

using namespace gedit;

JSPluginCommand::Ref JSPluginCommand::CreateFromConfig(const ConfigNode &config) {

    Ref cmd = std::make_shared<JSPluginCommand>();
    if (!cmd->InitializeFromConfig(config)) {
        return nullptr;
    }
    return cmd;
}

bool JSPluginCommand::InitializeFromConfig(const ConfigNode &config) {
    logger = gnilk::Logger::GetLogger("JSEngine");
    if (!PluginCommand::InitializeFromConfig(config)) {
        return false;
    }
    scriptFile = config.GetStr("script","");
    return true;
}

void JSPluginCommand::SetExecutor(JSPluginEngine *newJsEngine) {
    jsEngine = newJsEngine;
}

bool JSPluginCommand::Execute(const std::vector<std::string> &args) {
    if (!isLoaded) {
        return TryLoad();
    }
    std::string_view strData(scriptData->GetPtrAs<char *>());
    return jsEngine->RunScriptOnce(strData, args);
}

bool JSPluginCommand::TryLoad() {
    // Need some IOUtils..
    if (isLoaded) {
        // reloading???
        return true;
    }

    auto pluginRoot = Config::Instance()["main"].GetStr("plugin_directory","");
    auto pluginScriptFile =  std::filesystem::path(pluginRoot).append(scriptFile);

    logger->Debug("  Loading JS file from: %s", pluginScriptFile.c_str());
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    scriptData = assetLoader.LoadTextAsset(pluginScriptFile);
    if (scriptData == nullptr) {
        logger->Error("Unable to load file!");
        return false;
    }
    isLoaded = true;
    return true;
}

