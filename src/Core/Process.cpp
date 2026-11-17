//
// Process - the platform-independent part. The spawning and reading: ProcessUnix (src/Core/unix/).
//

#include "Process.h"

#include <cstdlib>
#include <thread>

using namespace gedit;

namespace {
    // The running processes, for StopAll. Never destroyed: a process thread may still finish while statics go.
    // The raw pointer is for Unregister: locking another entry's weak_ptr under the lock could drop its last Ref
    // here, and its destructor joins a thread that may be waiting for this lock.
    struct Entry {
        const Process *process;
        std::weak_ptr<Process> weak;
    };
    struct Running {
        std::mutex lock;
        std::vector<Entry> processes;
        // While StopAll runs - for good once the exit's has begun - Start() refuses: an exit callback that starts the
        // next process (GitBase: a failed 'git show' asks 'git status') would start one nothing waits for
        bool isRefusing = false;
        bool isExiting = false;
    };

    Running &GetRunning() {
        static auto *running = new Running();
        return *running;
    }

    // Once, on the first Start: after Runloop's statics (and the Editor) were built, so it runs before they go
    void StopAllAtExit() {
        {
            auto &running = GetRunning();
            std::lock_guard<std::mutex> guard(running.lock);
            running.isExiting = true;
        }
        Process::StopAll();
    }

    // False while stopping: the process must not start
    bool Register(const Process::Ref &process) {
        static std::once_flag atExitOnce;
        auto &running = GetRunning();
        std::call_once(atExitOnce, []() {
            std::atexit(StopAllAtExit);
        });
        std::lock_guard<std::mutex> guard(running.lock);
        if (running.isRefusing || running.isExiting) {
            return false;
        }
        std::erase_if(running.processes, [](const Entry &entry) {
            return entry.weak.expired();
        });
        running.processes.push_back({process.get(), process});
        return true;
    }

    void Unregister(const Process *process) {
        auto &running = GetRunning();
        std::lock_guard<std::mutex> guard(running.lock);
        std::erase_if(running.processes, [process](const Entry &entry) {
            return entry.process == process;
        });
    }
}

Process::Process(std::string newCommand, std::vector<std::string> newArguments) :
    command(std::move(newCommand)),
    arguments(std::move(newArguments)) {
}

void Process::SetWorkingDirectory(const std::filesystem::path &newWorkingDirectory) {
    workingDirectory = newWorkingDirectory;
}

void Process::SetOnStdOut(OutputDelegate newOnStdOut) {
    onStdOut = std::move(newOnStdOut);
}

void Process::SetOnStdErr(OutputDelegate newOnStdErr) {
    onStdErr = std::move(newOnStdErr);
}

void Process::SetOnExit(ExitDelegate newOnExit) {
    onExit = std::move(newOnExit);
}

Process::Result Process::Run() {
    if (!Start()) {
        return GetResult();
    }
    return WaitFor();
}

bool Process::Start() {
    {
        std::lock_guard<std::mutex> guard(lock);
        if (result.state != kState::kNotStarted) {
            return false;
        }
        result.state = kState::kRunning;
    }
    if (!Register(shared_from_this())) {
        // As any failed start: the exit callback runs here, at once
        Finish(FailedToStart(command, "processes are being stopped (exit)"));
        return false;
    }
    return DoStart();
}

size_t Process::StopAll(std::chrono::milliseconds timeout) {
    auto &running = GetRunning();
    {
        std::lock_guard<std::mutex> guard(running.lock);
        running.isRefusing = true;
    }
    auto deadline = std::chrono::steady_clock::now() + timeout;
    size_t numStopped = 0;
    // A process can register between our snapshot and the refusal - so round again until none is left to wait for
    while (std::chrono::steady_clock::now() < deadline) {
        std::vector<Ref> processes;
        {
            std::lock_guard<std::mutex> guard(running.lock);
            for (auto &entry : running.processes) {
                auto process = entry.weak.lock();
                if (process == nullptr) {
                    continue;
                }
                // A callback that calls exit() runs on that process's thread - waiting for it would wait for ourselves
                std::lock_guard<std::mutex> processGuard(process->lock);
                if (process->callbackThread != std::this_thread::get_id()) {
                    processes.push_back(process);
                }
            }
        }
        if (processes.empty()) {
            break;
        }
        for (auto &process : processes) {
            if (process->Kill()) {
                numStopped++;
            }
        }
        for (auto &process : processes) {
            std::unique_lock<std::mutex> guard(process->lock);
            process->finished.wait_until(guard, deadline, [&process]() {
                return process->isFinished;
            });
        }
    }
    {
        std::lock_guard<std::mutex> guard(running.lock);
        running.isRefusing = false;
    }
    return numStopped;
}

Process::Result Process::WaitFor() {
    std::unique_lock<std::mutex> guard(lock);
    if (result.state == kState::kNotStarted) {
        return result;
    }
    finished.wait(guard, [this]() {
        return isFinished;
    });
    return result;
}

bool Process::Kill() {
    std::lock_guard<std::mutex> guard(lock);
    if ((result.state != kState::kRunning) || isFinished) {
        return false;
    }
    return DoKill();
}

bool Process::IsRunning() {
    std::lock_guard<std::mutex> guard(lock);
    return (result.state == kState::kRunning) && !isFinished;
}

Process::Result Process::GetResult() {
    std::lock_guard<std::mutex> guard(lock);
    return result;
}

void Process::DeliverStdOut(const std::string &data) {
    if (onStdOut != nullptr) {
        onStdOut(data);
    }
}

void Process::DeliverStdErr(const std::string &data) {
    if (onStdErr != nullptr) {
        onStdErr(data);
    }
}

void Process::Finish(const Result &newResult) {
    {
        std::lock_guard<std::mutex> guard(lock);
        result = newResult;
        callbackThread = std::this_thread::get_id();
    }
    if (onExit != nullptr) {
        onExit(newResult);
    }
    {
        std::lock_guard<std::mutex> guard(lock);
        isFinished = true;
    }
    Unregister(this);
    finished.notify_all();
}

Process::Result Process::FailedToStart(const std::string &what, const std::string &reason) {
    Result failed;
    failed.state = kState::kFailedToStart;
    failed.error = what + ": " + reason;
    return failed;
}
