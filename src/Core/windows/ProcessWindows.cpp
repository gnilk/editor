//
// Process on Windows - for now a stub: nothing starts, the exit callback says why (docs/windows-port.md §4.6, phase 3:
// CreateProcessW with a pipe each for stdout and stderr).
//

#include "Core/Process.h"

namespace gedit {
    class ProcessWindows : public Process {
    public:
        ProcessWindows(std::string newCommand, std::vector<std::string> newArguments) :
            Process(std::move(newCommand), std::move(newArguments)) {
        }
    protected:
        bool DoStart() override {
            Finish(FailedToStart(command, "processes are not started on Windows yet"));
            return false;
        }
        bool DoKill() override {
            return false;
        }
    };
}

using namespace gedit;

Process::Ref Process::Create(std::string command, std::vector<std::string> arguments) {
    return std::make_shared<ProcessWindows>(std::move(command), std::move(arguments));
}
