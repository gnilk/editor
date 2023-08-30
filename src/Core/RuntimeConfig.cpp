//
// Created by gnilk on 14.01.23.
//
#include "Core/RuntimeConfig.h"

#ifdef GEDIT_MACOS
#include "Core/macOS/MacOSFolderMonitor.h"
#endif

using namespace gedit;
RuntimeConfig &RuntimeConfig::Instance() {
    static RuntimeConfig config;
    return config;
}

FolderMonitorBase &RuntimeConfig::GetFolderMonitor() {
#ifdef GEDIT_MACOS
    return MacOSFolderMonitor::Instance();
#else
    static FolderMonitorBase dummy;
            return dummy;
#endif
}


void RuntimeConfig::RegisterPluginCommand(const PluginCommand::Ref pluginCommand) {
    // Insert twice - this allows lookup to find it...
    pluginCommands.insert({pluginCommand->GetName(),pluginCommand});
    pluginCommands.insert({pluginCommand->GetShortName(),pluginCommand});
}

bool RuntimeConfig::HasPluginCommand(const std::string &name) {
    // We insert the same plugin command twice, once with the short name and one with the full name...
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

std::vector<PluginCommand::Ref> RuntimeConfig::GetPluginCommands() {
    std::vector<PluginCommand::Ref> plugins;
    // Note: Commands are registered multiple times
    // 1) short command (like: sl)
    // 2) real/long command (like: setlanguage)
    // Thus if not checking we will end up with multiple same-help strings
    for(auto &[key, pCommandRef] : pluginCommands) {
        // Do we have this reference already...
        if (std::find(plugins.begin(), plugins.end(), pCommandRef) == std::end(plugins)) {
            plugins.push_back(pCommandRef);
        }
    }
    return plugins;
}

