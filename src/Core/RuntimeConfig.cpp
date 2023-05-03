//
// Created by gnilk on 14.01.23.
//

#include "Core/RuntimeConfig.h"

using namespace gedit;
RuntimeConfig &RuntimeConfig::Instance() {
    static RuntimeConfig config;
    return config;
}

void RuntimeConfig::RegisterPluginCommand(const PluginCommand::Ref pluginCommand) {
    pluginCommands.insert({pluginCommand->GetName(),pluginCommand});
    pluginCommands.insert({pluginCommand->GetShortName(),pluginCommand});
}

bool RuntimeConfig::HasPluginCommand(const std::string &name) {
    if (pluginCommands.find(name) != pluginCommands.end()) {
        return true;
    }
    return false;
}

PluginCommand::Ref RuntimeConfig::GetPluginCommand(const std::string &name) {
    if (!HasPluginCommand(name)) {
        return nullptr;
    }
    return pluginCommands[name];
}
