//
// Process (docs/usability-round.md §0), through Process::Create - so on macOS and Linux, ProcessUnix. Real children -
// 'sh', 'true', 'false', 'sleep' - since what is tested is the spawning, the pipes and the waiting.
//
#include <testinterface.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <signal.h>


#include "Core/Process.h"
#include "Core/unix/Shell.h"

using namespace gedit;
namespace fs = std::filesystem;
using kState = Process::kState;

extern "C" {
    DLL_EXPORT int test_process(ITesting *t);
    DLL_EXPORT int test_process_exitcode(ITesting *t);
    DLL_EXPORT int test_process_stdout(ITesting *t);
    DLL_EXPORT int test_process_stderr(ITesting *t);
    DLL_EXPORT int test_process_largeoutput(ITesting *t);
    DLL_EXPORT int test_process_workingdir(ITesting *t);
    DLL_EXPORT int test_process_nosuchcommand(ITesting *t);
    DLL_EXPORT int test_process_kill(ITesting *t);
    DLL_EXPORT int test_process_async(ITesting *t);
    DLL_EXPORT int test_process_dropref(ITesting *t);
    DLL_EXPORT int test_process_stdin(ITesting *t);
    DLL_EXPORT int test_process_inheritedpipe(ITesting *t);
    DLL_EXPORT int test_process_startonce(ITesting *t);
    DLL_EXPORT int test_process_shellsurvives(ITesting *t);
    DLL_EXPORT int test_process_shellclose(ITesting *t);
    DLL_EXPORT int test_process_shellexit(ITesting *t);
    DLL_EXPORT int test_process_shellclose_ignoreshup(ITesting *t);
    DLL_EXPORT int test_process_shellclose_output_on_hup(ITesting *t);
    DLL_EXPORT int test_process_shellclose_second_shell(ITesting *t);
    DLL_EXPORT int test_process_stopall(ITesting *t);
    DLL_EXPORT int test_process_stopall_atexit(ITesting *t);
    DLL_EXPORT int test_process_stopall_chained(ITesting *t);
}

namespace {
    struct Captured {
        std::mutex lock;
        std::string out;
        std::string err;
    };

    Process::Ref ShellScript(const std::string &script, Captured &captured) {
        auto process = Process::Create("sh", {"-c", script});
        process->SetOnStdOut([&captured](const std::string &data) {
            std::lock_guard<std::mutex> guard(captured.lock);
            captured.out += data;
        });
        process->SetOnStdErr([&captured](const std::string &data) {
            std::lock_guard<std::mutex> guard(captured.lock);
            captured.err += data;
        });
        return process;
    }
}

DLL_EXPORT int test_process(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_process_exitcode(ITesting *t) {
    auto ok = Process::Create("true")->Run();
    TR_ASSERT(t, ok.state == kState::kExited);
    TR_ASSERT(t, ok.exitCode == 0);
    TR_ASSERT(t, ok.IsSuccess());

    auto failed = Process::Create("false")->Run();
    TR_ASSERT(t, failed.state == kState::kExited);
    TR_ASSERT(t, failed.exitCode == 1);
    TR_ASSERT(t, !failed.IsSuccess());

    auto code = Process::Create("sh", {"-c", "exit 42"})->Run();
    TR_ASSERT(t, code.exitCode == 42);
    return kTR_Pass;
}

DLL_EXPORT int test_process_stdout(ITesting *t) {
    Captured captured;
    // The arguments reach the child as they are, one each - no shell between
    auto process = Process::Create("printf", {"%s|%s", "a b", "c"});
    process->SetOnStdOut([&captured](const std::string &data) {
        captured.out += data;
    });
    auto result = process->Run();
    TR_ASSERT(t, result.IsSuccess());
    TR_ASSERT(t, captured.out == "a b|c");
    return kTR_Pass;
}

DLL_EXPORT int test_process_stderr(ITesting *t) {
    Captured captured;
    auto result = ShellScript("echo out; echo err 1>&2; exit 3", captured)->Run();
    TR_ASSERT(t, result.exitCode == 3);
    TR_ASSERT(t, captured.out == "out\n");
    TR_ASSERT(t, captured.err == "err\n");
    return kTR_Pass;
}

DLL_EXPORT int test_process_largeoutput(ITesting *t) {
    // More than a pipe holds (64 KB) and more than one read: a reader that waited for the exit first would deadlock
    Captured captured;
    auto result = ShellScript("i=0; while [ $i -lt 20000 ]; do echo 0123456789; i=$((i+1)); done", captured)->Run();
    TR_ASSERT(t, result.IsSuccess());
    TR_ASSERT(t, captured.out.size() == 20000 * 11);
    return kTR_Pass;
}

DLL_EXPORT int test_process_workingdir(ITesting *t) {
    auto dir = fs::canonical(fs::temp_directory_path());
    Captured captured;
    auto process = ShellScript("pwd", captured);
    process->SetWorkingDirectory(dir);
    TR_ASSERT(t, process->Run().IsSuccess());
    TR_ASSERT(t, fs::canonical(captured.out.substr(0, captured.out.find('\n'))) == dir);
    return kTR_Pass;
}

DLL_EXPORT int test_process_nosuchcommand(ITesting *t) {
    bool isExitCalled = false;
    auto process = Process::Create("goatedit-no-such-command");
    process->SetOnExit([&isExitCalled](const Process::Result &result) {
        isExitCalled = (result.state == kState::kFailedToStart);
    });
    TR_ASSERT(t, !process->Start());
    TR_ASSERT(t, isExitCalled);
    auto result = process->WaitFor();
    TR_ASSERT(t, result.state == kState::kFailedToStart);
    TR_ASSERT(t, !result.error.empty());
    TR_ASSERT(t, !process->IsRunning());
    return kTR_Pass;
}

DLL_EXPORT int test_process_kill(ITesting *t) {
    auto process = Process::Create("sleep", {"30"});
    auto timeStart = std::chrono::steady_clock::now();
    TR_ASSERT(t, process->Start());
    TR_ASSERT(t, process->IsRunning());
    TR_ASSERT(t, process->Kill());
    auto result = process->WaitFor();
    TR_ASSERT(t, result.state == kState::kSignalled);
    TR_ASSERT(t, std::chrono::steady_clock::now() - timeStart < std::chrono::seconds(5));
    // Reaped: its pid may be someone else's now
    TR_ASSERT(t, !process->Kill());
    return kTR_Pass;
}

DLL_EXPORT int test_process_async(ITesting *t) {
    // Start() returns while the child runs; the callbacks come from another thread, output before the exit
    Captured captured;
    std::atomic<bool> isExited = false;
    std::atomic<bool> isOutputFirst = false;
    std::thread::id idCallback;
    auto process = ShellScript("sleep 0.2; echo done", captured);
    process->SetOnExit([&](const Process::Result &result) {
        std::lock_guard<std::mutex> guard(captured.lock);
        isOutputFirst = (captured.out == "done\n");
        idCallback = std::this_thread::get_id();
        isExited = true;
    });
    TR_ASSERT(t, process->Start());
    TR_ASSERT(t, !isExited);
    TR_ASSERT(t, process->WaitFor().IsSuccess());
    TR_ASSERT(t, isExited);
    TR_ASSERT(t, isOutputFirst);
    TR_ASSERT(t, idCallback != std::this_thread::get_id());
    return kTR_Pass;
}

DLL_EXPORT int test_process_dropref(ITesting *t) {
    // Dropping the last Ref doesn't stop it: the exit still arrives
    std::atomic<bool> isExited = false;
    {
        auto process = Process::Create("sh", {"-c", "sleep 0.1"});
        process->SetOnExit([&isExited](const Process::Result &result) {
            isExited = result.IsSuccess();
        });
        TR_ASSERT(t, process->Start());
    }
    for (int i = 0; (i < 100) && !isExited; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    TR_ASSERT(t, isExited);
    return kTR_Pass;
}

DLL_EXPORT int test_process_stdin(ITesting *t) {
    // Nothing on stdin: a command that reads gets the end at once instead of waiting for the editor's input
    Captured captured;
    auto result = ShellScript("cat; echo end", captured)->Run();
    TR_ASSERT(t, result.IsSuccess());
    TR_ASSERT(t, captured.out == "end\n");
    return kTR_Pass;
}

DLL_EXPORT int test_process_inheritedpipe(ITesting *t) {
    // A grandchild that outlives the child holds the pipe open: the reader stops at the child's exit, not at a pipe
    // that ends 3 seconds later. The same holds for a pipe another spawn inherited (Process.cpp, MakePipe).
    Captured captured;
    auto timeStart = std::chrono::steady_clock::now();
    auto result = ShellScript("echo start; sleep 3 & exit 0", captured)->Run();
    TR_ASSERT(t, result.IsSuccess());
    TR_ASSERT(t, captured.out == "start\n");
    TR_ASSERT(t, std::chrono::steady_clock::now() - timeStart < std::chrono::seconds(2));
    return kTR_Pass;
}

DLL_EXPORT int test_process_startonce(ITesting *t) {
    auto process = Process::Create("true");
    TR_ASSERT(t, process->Start());
    TR_ASSERT(t, !process->Start());
    TR_ASSERT(t, process->WaitFor().IsSuccess());
    return kTR_Pass;
}

// The embedded terminal's shell keeps delivering output after one of OUR children exits. A SIGCHLD handler
// (Linux only) set the shell to kTerminate on any child's exit - the first 'git rev-parse' after opening a folder
// stopped the pty reader, and the terminal showed nothing from then on while the shell still ran the commands.
DLL_EXPORT int test_process_shellsurvives(ITesting *t) {
    // Static, so the delegate's locals are gone before it is - the shell has exited by then (see the end)
    static Shell shell;
    std::mutex lock;
    std::string output;
    shell.SetOutputDelegate([&lock, &output](const uint8_t *buffer, size_t length) {
        std::lock_guard<std::mutex> guard(lock);
        output.append(reinterpret_cast<const char *>(buffer), length);
    });
    TR_ASSERT(t, shell.Begin("/bin/sh", "-i", {}));
    auto tEnd = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while ((shell.GetState() != Shell::State::kRunning) && (std::chrono::steady_clock::now() < tEnd)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    TR_ASSERT(t, shell.GetState() == Shell::State::kRunning);

    // One of the editor's own children comes and goes
    TR_ASSERT(t, Process::Create("true", {})->Run().exitCode == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    TR_ASSERT(t, shell.GetState() == Shell::State::kRunning);

    std::u32string cmd = U"echo goat_$((40+2))\n";
    shell.SendCmd(cmd);
    bool isSeen = false;
    tEnd = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!isSeen && (std::chrono::steady_clock::now() < tEnd)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        std::lock_guard<std::mutex> guard(lock);
        isSeen = (output.find("goat_42") != std::string::npos);
    }

    // Done with it: the shell exits, the reader sees EIO and stops (and stops calling into our locals)
    std::u32string exitCmd = U"exit\n";
    shell.SendCmd(exitCmd);
    tEnd = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while ((shell.GetState() != Shell::State::kTerminated) && (std::chrono::steady_clock::now() < tEnd)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    shell.SetOutputDelegate(nullptr);

    TR_ASSERT(t, isSeen);
    return kTR_Pass;
}


namespace {
    bool WaitForShellState(const Shell &shell, Shell::State state, int timeoutMs) {
        auto tEnd = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while ((shell.GetState() != state) && (std::chrono::steady_clock::now() < tEnd)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return shell.GetState() == state;
    }
}

// The crash on quit (2026-11-17, Mac): the pty reader was a detached thread holding the Shell's 'this', and
// nothing waited for it - the TerminalController (and its Shell) went away while the reader was still in poll().
// Close() must return only when the reader is gone, so the Shell can be destroyed right after.
DLL_EXPORT int test_process_shellclose(ITesting *t) {
    auto shell = std::make_unique<Shell>();
    std::atomic<size_t> nBytes { 0 };
    shell->SetOutputDelegate([&nBytes](const uint8_t *, size_t length) {
        nBytes += length;
    });
    TR_ASSERT(t, shell->Begin("/bin/sh", "-i", {}));
    TR_ASSERT(t, WaitForShellState(*shell, Shell::State::kRunning, 3000));

    auto tStart = std::chrono::steady_clock::now();
    shell->Close();
    auto msClose = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tStart).count();
    bool isStopped = (shell->GetState() == Shell::State::kTerminated);
    if (!isStopped) {
        // The reader is still running and holds 'this' - destroying the Shell now would be the crash itself
        shell.release();
    }
    TR_ASSERT(t, isStopped);
    TR_ASSERT(t, msClose < 1000);
    shell.reset();
    return kTR_Pass;
}

// A shell that ends by itself is told about, once, and a new one can be started in its place (hitlist #82); one that
// Close() ended is not
DLL_EXPORT int test_process_shellexit(ITesting *t) {
    Shell shell;
    std::atomic<int> nExits { 0 };
    std::atomic<bool> isTerminatedAtExit { false };
    shell.SetOutputDelegate([](const uint8_t *, size_t) {});
    shell.SetExitDelegate([&shell, &nExits, &isTerminatedAtExit]() {
        isTerminatedAtExit = (shell.GetState() == Shell::State::kTerminated);
        nExits++;
    });
    std::u32string cmd = U"exit\n";
    for (int round = 1; round <= 2; round++) {
        TR_ASSERT(t, shell.Begin("/bin/sh", "-i", {}));
        TR_ASSERT(t, WaitForShellState(shell, Shell::State::kRunning, 3000));
        shell.SendCmd(cmd);
        TR_ASSERT(t, WaitForShellState(shell, Shell::State::kTerminated, 3000));
        // Joins the reader: the delegate has returned
        shell.Close();
        TR_ASSERT(t, nExits == round);
        TR_ASSERT(t, isTerminatedAtExit);
    }
    TR_ASSERT(t, shell.Begin("/bin/sh", "-i", {}));
    TR_ASSERT(t, WaitForShellState(shell, Shell::State::kRunning, 3000));
    shell.Close();
    TR_ASSERT(t, nExits == 2);
    return kTR_Pass;
}

// A shell (or a job) that ignores the hangup must not hang the quit: Close() ends it anyway, in bounded time
DLL_EXPORT int test_process_shellclose_ignoreshup(ITesting *t) {
    auto shell = std::make_unique<Shell>();
    std::mutex lock;
    std::string output;
    shell->SetOutputDelegate([&lock, &output](const uint8_t *buffer, size_t length) {
        std::lock_guard<std::mutex> guard(lock);
        output.append(reinterpret_cast<const char *>(buffer), length);
    });
    TR_ASSERT(t, shell->Begin("/bin/sh", "-i", {}));
    TR_ASSERT(t, WaitForShellState(*shell, Shell::State::kRunning, 3000));
    std::u32string cmd = U"trap '' HUP; echo trap_$((1+1))\n";
    shell->SendCmd(cmd);
    bool isTrapped = false;
    auto tEnd = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!isTrapped && (std::chrono::steady_clock::now() < tEnd)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        std::lock_guard<std::mutex> guard(lock);
        isTrapped = (output.find("trap_2") != std::string::npos);
    }
    TR_ASSERT(t, isTrapped);

    auto tStart = std::chrono::steady_clock::now();
    shell->Close();
    auto msClose = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tStart).count();
    bool isStopped = (shell->GetState() == Shell::State::kTerminated);
    if (!isStopped) {
        shell.release();
    }
    TR_ASSERT(t, isStopped);
    TR_ASSERT(t, msClose < 2000);
    shell.reset();
    return kTR_Pass;
}

// The hang on quit with bash 5 (2026-11-19, Mac): a shell that writes on the hangup and exits, its output still
// queued for us. The reader has stopped reading by then (it waits for the shell) - and on macOS the exiting shell
// waits in the kernel for that output to drain, SIGKILL or not. Our end of the pty closes before the wait.
DLL_EXPORT int test_process_shellclose_output_on_hup(ITesting *t) {
    auto shell = std::make_unique<Shell>();
    std::mutex lock;
    std::string output;
    shell->SetOutputDelegate([&lock, &output](const uint8_t *buffer, size_t length) {
        std::lock_guard<std::mutex> guard(lock);
        output.append(reinterpret_cast<const char *>(buffer), length);
    });
    TR_ASSERT(t, shell->Begin("/bin/sh", "-i", {}));
    TR_ASSERT(t, WaitForShellState(*shell, Shell::State::kRunning, 3000));
    std::u32string cmd = U"trap 'head -c 4000 /dev/zero | tr \"\\0\" x; exit' HUP; echo trap_$((1+1))\n";
    shell->SendCmd(cmd);
    bool isTrapped = false;
    auto tEnd = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!isTrapped && (std::chrono::steady_clock::now() < tEnd)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        std::lock_guard<std::mutex> guard(lock);
        isTrapped = (output.find("trap_2") != std::string::npos);
    }
    TR_ASSERT(t, isTrapped);

    // On a thread: before the fix Close() never returned
    std::atomic<bool> isClosed = false;
    std::thread closer([&shell, &isClosed]() {
        shell->Close();
        isClosed = true;
    });
    tEnd = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!isClosed && (std::chrono::steady_clock::now() < tEnd)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (!isClosed) {
        // Stuck for good: leave the thread and the Shell behind rather than hang the run
        closer.detach();
        shell.release();
    }
    TR_ASSERT(t, isClosed);
    closer.join();
    TR_ASSERT(t, shell->GetState() == Shell::State::kTerminated);
    shell.reset();
    return kTR_Pass;
}

// The hang on quit with agent tabs (2026-11-24, Mac, open-bugs #36): the pty master wasn't close-on-exec, so a shell
// started later held an earlier one's master open. Closing our end then didn't close the pty: a job in the first shell
// that outlives the hangup (an agent) kept writing into a pty nobody reads, the dying shell waited in the kernel, and
// Close() never returned. With a second shell running, closing the first must end in bounded time - and its job with it.
DLL_EXPORT int test_process_shellclose_second_shell(ITesting *t) {
    auto first = std::make_unique<Shell>();
    std::mutex lock;
    std::string output;
    first->SetOutputDelegate([&lock, &output](const uint8_t *buffer, size_t length) {
        std::lock_guard<std::mutex> guard(lock);
        if (output.size() < 4096) {
            output.append(reinterpret_cast<const char *>(buffer), length);
        }
    });
    TR_ASSERT(t, first->Begin("/bin/sh", "-i", {}));
    TR_ASSERT(t, WaitForShellState(*first, Shell::State::kRunning, 3000));
    // The job ignores SIGHUP (kept across exec) and writes for ever
    std::u32string cmd = U"sh -c 'echo job_pid=$$; trap \"\" HUP; exec yes'\n";
    first->SendCmd(cmd);
    pid_t jobPid = 0;
    auto tEnd = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while ((jobPid == 0) && (std::chrono::steady_clock::now() < tEnd)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        std::lock_guard<std::mutex> guard(lock);
        // The output, not the echo of the command ('job_pid=$$')
        for (auto at = output.find("job_pid="); at != std::string::npos; at = output.find("job_pid=", at + 1)) {
            if (std::isdigit((unsigned char)output[at + 8]) && (output.find_first_of("\r\n", at) != std::string::npos)) {
                jobPid = (pid_t)std::atoi(output.c_str() + at + 8);
                break;
            }
        }
    }
    TR_ASSERT(t, jobPid > 0);

    // The second shell - forked while the first's master is open in this process
    auto second = std::make_unique<Shell>();
    second->SetOutputDelegate([](const uint8_t *, size_t) {});
    TR_ASSERT(t, second->Begin("/bin/sh", "-i", {}));
    TR_ASSERT(t, WaitForShellState(*second, Shell::State::kRunning, 3000));

    std::atomic<bool> isClosed = false;
    std::thread closer([&first, &isClosed]() {
        first->Close();
        isClosed = true;
    });
    tEnd = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (!isClosed && (std::chrono::steady_clock::now() < tEnd)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    // The pty really closed: the job's writes fail, it ends - given a moment
    bool isJobGone = false;
    tEnd = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (isClosed && !isJobGone && (std::chrono::steady_clock::now() < tEnd)) {
        isJobGone = (kill(jobPid, 0) != 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    // Whatever happened: no job left behind, then the second shell
    kill(jobPid, SIGKILL);
    second->Close();
    if (!isClosed) {
        // Stuck for good: leave the thread and the Shell behind rather than hang the run
        closer.detach();
        first.release();
        TR_ASSERT(t, isClosed);
    }
    closer.join();
    TR_ASSERT(t, first->GetState() == Shell::State::kTerminated);
    TR_ASSERT(t, isJobGone);
    return kTR_Pass;
}

DLL_EXPORT int test_process_stopall(ITesting *t) {
    // Two running, their Refs dropped: StopAll kills both and returns after both exit callbacks have run
    std::atomic<int> numExited = 0;
    for (int i = 0; i < 2; i++) {
        auto process = Process::Create("sleep", {"30"});
        process->SetOnExit([&numExited](const Process::Result &result) {
            numExited++;
        });
        TR_ASSERT(t, process->Start());
    }
    auto timeStart = std::chrono::steady_clock::now();
    TR_ASSERT(t, Process::StopAll() == 2);
    TR_ASSERT(t, numExited == 2);
    TR_ASSERT(t, std::chrono::steady_clock::now() - timeStart < std::chrono::seconds(2));
    // Nothing left
    TR_ASSERT(t, Process::StopAll() == 0);
    return kTR_Pass;
}

// open-bugs #32: a process still running at exit() must finish (killed, its exit callback run) before the statics its
// callback reaches are destroyed. process-exit-helper (built next to libutests, run from the build dir) returns from
// main with a 'sleep 30' running and reports from the callback: 'K' = the statics were still there.
DLL_EXPORT int test_process_stopall_atexit(ITesting *t) {
    auto helper = std::filesystem::current_path() / "process-exit-helper";
    TR_ASSERT(t, std::filesystem::exists(helper));
    std::string out;
    auto process = Process::Create(helper.string());
    process->SetOnStdOut([&out](const std::string &data) {
        out += data;
    });
    auto timeStart = std::chrono::steady_clock::now();
    auto result = process->Run();
    TR_ASSERT(t, result.IsSuccess());
    // Before the fix nothing ran the callback before the helper was gone: no report at all
    TR_ASSERT(t, out == "K");
    TR_ASSERT(t, std::chrono::steady_clock::now() - timeStart < std::chrono::seconds(5));
    return kTR_Pass;
}

// The Linux ASan run of #32 after the first fix: StopAll killed 'git show', whose exit callback started 'git status' -
// not in StopAll's list, still running when the statics went. A process started from a callback during the stop must
// end inside it too: here and in the helper's real exit.
DLL_EXPORT int test_process_stopall_chained(ITesting *t) {
    std::atomic<bool> isNextDone = false;
    Process::Ref next;
    auto first = Process::Create("sleep", {"30"});
    first->SetOnExit([&](const Process::Result &result) {
        next = Process::Create("sleep", {"30"});
        next->SetOnExit([&isNextDone](const Process::Result &nextResult) {
            isNextDone = true;
        });
        next->Start();
    });
    TR_ASSERT(t, first->Start());
    auto timeStart = std::chrono::steady_clock::now();
    Process::StopAll();
    TR_ASSERT(t, isNextDone);
    TR_ASSERT(t, (next != nullptr) && !next->IsRunning());
    TR_ASSERT(t, std::chrono::steady_clock::now() - timeStart < std::chrono::seconds(2));
    // A plain StopAll doesn't keep refusing: a process starts afterwards
    auto after = Process::Create("sh", {"-c", "exit 0"});
    TR_ASSERT(t, after->Run().IsSuccess());

    auto helper = std::filesystem::current_path() / "process-exit-helper";
    std::string out;
    auto process = Process::Create(helper.string(), {"chain"});
    process->SetOnStdOut([&out](const std::string &data) {
        out += data;
    });
    TR_ASSERT(t, process->Run().IsSuccess());
    TR_ASSERT(t, out == "KK");
    return kTR_Pass;
}
