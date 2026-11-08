//
// Process - the platform-independent part. The spawning and reading: ProcessUnix (src/Core/unix/).
//

#include "Process.h"

using namespace gedit;

Process::Process(std::string command, std::vector<std::string> arguments) :
    command(std::move(command)),
    arguments(std::move(arguments)) {
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
    return DoStart();
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
    }
    if (onExit != nullptr) {
        onExit(newResult);
    }
    {
        std::lock_guard<std::mutex> guard(lock);
        isFinished = true;
    }
    finished.notify_all();
}

Process::Result Process::FailedToStart(const std::string &what, const std::string &reason) {
    Result failed;
    failed.state = kState::kFailedToStart;
    failed.error = what + ": " + reason;
    return failed;
}
