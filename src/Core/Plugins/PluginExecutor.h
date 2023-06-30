//
// Created by gnilk on 30.06.23.
//

#ifndef GEDIT_PLUGINEXECUTOR_H
#define GEDIT_PLUGINEXECUTOR_H

#include <string>
namespace gedit {
    class PluginExecutor {
    public:
        static bool ParseAndExecuteWithCmdPrefix(const std::string &cmdline);
    };
}


#endif //EDITOR_PLUGINEXECUTOR_H
