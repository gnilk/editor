//
// Created by gnilk on 03.05.23.
//

#include "PluginCommand.h"

using namespace gedit;

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
