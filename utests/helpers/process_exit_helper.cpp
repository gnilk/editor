//
// process-exit-helper - for test_process_stopall_atexit (open-bugs #32). A program of its own, not a fork of trun:
// a forked trun runs trun's exit too, which has threads the child doesn't.
//
// The static stands for Runloop's message queues: built at load time, destroyed at exit. main starts a long child and
// returns with it running; the exit callback reports whether the static was still alive when it ran. 'K' = it was,
// 'X' = it ran too late, nothing = it never ran (the process ended first - the bug).
//

#include <unistd.h>

#include <atomic>
#include <string>

#include "Core/Process.h"

using namespace gedit;

namespace {
    struct ExitSentinel {
        std::atomic<bool> isAlive = true;
        ~ExitSentinel() {
            isAlive = false;
        }
    };
    ExitSentinel glbSentinel;
}

namespace {
    void Report() {
        char report = glbSentinel.isAlive ? 'K' : 'X';
        (void)!write(STDOUT_FILENO, &report, 1);
    }
}

// 'chain': the exit callback starts another process, as GitBase does (a failed 'git show' asks 'git status') - that one
// must not outlive the exit either: one report per process, "KK".
int main(int argc, char **argv) {
    bool isChained = (argc > 1) && (std::string(argv[1]) == "chain");
    auto process = Process::Create("sleep", {"30"});
    process->SetOnExit([isChained](const Process::Result &result) {
        Report();
        if (!isChained) {
            return;
        }
        auto next = Process::Create("sleep", {"30"});
        next->SetOnExit([](const Process::Result &nextResult) {
            Report();
        });
        next->Start();
    });
    if (!process->Start()) {
        return 1;
    }
    return 0;
}
