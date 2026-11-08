//
// Process on macOS and Linux: posix_spawn, a pipe each for stdout and stderr, and a thread that reads them and waits
// for the exit. Made by Process::Create - use it through Process.
//

#ifndef GOATEDIT_PROCESSUNIX_H
#define GOATEDIT_PROCESSUNIX_H

#include <sys/types.h>

#include <string>
#include <thread>
#include <vector>

#include "Core/Process.h"

namespace gedit {
    class ProcessUnix : public Process {
    public:
        ProcessUnix(std::string command, std::vector<std::string> arguments);
        virtual ~ProcessUnix();
    protected:
        bool DoStart() override;
        bool DoKill() override;
    private:
        void RunLoop(int fdStdOut, int fdStdErr);
    private:
        pid_t pid = -1;
        bool isReaped = false;          // a pid that has been waited for may belong to someone else - never kill it
        std::thread thread;
    };
}

#endif //GOATEDIT_PROCESSUNIX_H
