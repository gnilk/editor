//
// Created by gnilk on 30.06.23.
//

#include "PluginExecutor.h"
#include "Core/Config/Config.h"
#include "Core/RuntimeConfig.h"
#include "Core/StrUtil.h"

#include "logger.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

bool PluginExecutor::ParseAndExecuteWithCmdPrefix(const std::u32string &cmdline32) {
    // Below is all UTF8 - as we are leaving the editor
    // Plugin's all operate on UTF8
    auto prefix = Config::Instance()["main"].GetStr("cmdlet_prefix");
    auto cmdline = UnicodeHelper::utf32to8(cmdline32);

    if (!strutil::startsWith(cmdline, prefix)) {
        return false;
    }

    auto commandList = SplitCommandLine(cmdline.substr(prefix.length()));
    if (commandList.empty()) {
        return false;
    }

    if (!RuntimeConfig::Instance().HasPluginCommand(commandList[0])) {
        auto logger = gnilk::Logger::GetLogger("PluginExecutor");
        logger->Error("Plugin '%s' not found", commandList[0].c_str());
        return false;
    }

    auto cmd = RuntimeConfig::Instance().GetPluginCommand(commandList[0]);
    auto argList = std::vector<std::string>(commandList.begin() + 1, commandList.end());
    cmd->Execute(argList);

    return true;
}

std::vector<std::string> PluginExecutor::SplitCommandLine(const std::string &cmdLineNoPrefix) {
    // We should have a 'smarter' that keeps strings and so forth
    std::vector<std::string> words;
    strutil::split(words, cmdLineNoPrefix, ' ');
    std::vector<std::string> commandList;
    for (auto &word : words) {
        if (!word.empty()) {
            commandList.push_back(word);
        }
    }
    if (commandList.empty()) {
        return {};
    }
    auto &name = commandList[0];
    if ((name.length() > 1) && (name.back() == '!')) {
        name.pop_back();
        commandList.insert(commandList.begin() + 1, "!");
    }
    return commandList;
}
