//
// Created by gnilk on 03.05.23.
//
#include <filesystem>

#include "Core/RuntimeConfig.h"

#include "JSPluginCommand.h"

using namespace gedit;


// MOVE THESE AWAY FROM HERE
PluginCommand::Ref PluginCommand::CreateFromConfig(const ConfigNode &config) {
    Ref cmd = std::make_shared<PluginCommand>();
    cmd->InitializeFromConfig(config);
    return cmd;
}
bool PluginCommand::InitializeFromConfig(const ConfigNode &config) {
    logger = gnilk::Logger::GetLogger("JSPluginCommand");

    name = config.GetStr("name","");
    shortName = config.GetStr("short","");
    description = config.GetStr("description","");

    logger->Debug("Init JS Plug '%s', Short: '%s', Desc: %s", name.c_str(), shortName.c_str(), description.c_str());


    auto &strArgs = config.GetSequenceOfStr("arguments");
    for(auto &arg : strArgs) {
        std::vector<std::string> argvec;
        strutil::split(argvec, arg.c_str(), ',');
        assert(argvec.size() == 2);
        if (argvec.size() != 2) {
            // log this error
            return false;
        }
        Argument argument = {argvec[0], argvec[1]};
        argumentList.push_back(argument);
    }
    return true;
}


JSPluginCommand::Ref JSPluginCommand::CreateFromConfig(const ConfigNode &config) {

    Ref cmd = std::make_shared<JSPluginCommand>();
    if (!cmd->InitializeFromConfig(config)) {
        return nullptr;
    }
    return cmd;
}

bool JSPluginCommand::InitializeFromConfig(const ConfigNode &config) {
    logger = gnilk::Logger::GetLogger("JSPluginCommand");
    if (!PluginCommand::InitializeFromConfig(config)) {
        return false;
    }
    scriptFile = config.GetStr("script","");
    return true;
}

bool JSPluginCommand::Execute() {
    if (!isLoaded) {
        TryLoad();
    }
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
    auto assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    scriptData = assetLoader.LoadTextAsset(pluginScriptFile);
    if (scriptData == nullptr) {
        logger->Error("Unable to load file!");
        return false;
    }
    isLoaded = true;
    return true;
}

