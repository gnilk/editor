//
// Created by gnilk on 30.06.23.
//

#include "PluginExecutor.h"
#include "Core/Config/Config.h"
#include "Core/RuntimeConfig.h"
#include "Core/StrUtil.h"

#include "logger.h"

using namespace gedit;

bool PluginExecutor::ParseAndExecuteWithCmdPrefix(const std::string &cmdline) {
    auto prefix = Config::Instance()["commandmode"].GetStr("cmdlet_prefix");
    if (!strutil::startsWith(cmdline, prefix)) {
        return false;
    }

    std::string cmdLineNoPrefix = cmdline.substr(prefix.length());
    std::vector<std::string> commandList;
    // We should have a 'smarter' that keeps strings and so forth
    strutil::split(commandList, cmdLineNoPrefix.data(), ' ');

    // There is more to come...
    if (!RuntimeConfig::Instance().HasPluginCommand(commandList[0])) {
        auto logger = gnilk::Logger::GetLogger("PluginExecutor");
        logger->Error("Plugin '%s' not found", commandList[0].c_str());
        return false;
    }

    auto cmd = RuntimeConfig::Instance().GetPluginCommand(commandList[0]);
    auto argStart = commandList.begin()+1;
    auto argEnd = commandList.end();
    auto argList = std::vector<std::string>(argStart, argEnd);
    cmd->Execute(argList);

    return true;

}