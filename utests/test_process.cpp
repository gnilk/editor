//
// Process (docs/usability-round.md §0), through Process::Create - so on macOS and Linux, ProcessUnix. Real children -
// 'sh', 'true', 'false', 'sleep' - since what is tested is the spawning, the pipes and the waiting.
//
#include <testinterface.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

#include "Core/Process.h"

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
