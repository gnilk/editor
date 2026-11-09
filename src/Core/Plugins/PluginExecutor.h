//
// Created by gnilk on 30.06.23.
//

#ifndef GEDIT_PLUGINEXECUTOR_H
#define GEDIT_PLUGINEXECUTOR_H

#include <string>
#include <vector>

namespace gedit {
    class PluginExecutor {
    public:
        static bool ParseAndExecuteWithCmdPrefix(const std::u32string &cmdline);
        // A command line without its prefix: the command's name first, then its arguments. A '!' at the end of the
        // name - '.cb!', vi's ':q!' - is taken off and becomes the first argument, as '.cb !' gives it
        // (docs/usability-round.md §5). Empty when there is no name.
        static std::vector<std::string> SplitCommandLine(const std::string &cmdLineNoPrefix);
    };
}


#endif //EDITOR_PLUGINEXECUTOR_H
