//
// A child process with its output delivered as it arrives (docs/usability-round.md §0). Generic: it runs a
// command and reports what it writes and how it ended - anything that knows what the command means (git) is built
// on top of it, holding a Process, not deriving from one.
//
// Async by default: Start() spawns the child and returns, and a thread of the process's own reads stdout and
// stderr and waits for the exit. The callbacks run on that thread - a caller that touches the UI posts to the main
// thread itself (Runloop::PostMessage); Process knows nothing of the run loop. WaitFor() blocks until the end.
//
// A started process keeps itself alive until it has exited, so dropping the last Ref doesn't stop it: Kill() does.
//
// This is the platform-independent part: state, callbacks, waiting. The spawning and reading is per platform, as
// FolderMonitor's is: 'ProcessUnix' (src/Core/unix/) for macOS and Linux. Create() is defined by the platform's
// source file, so this header names no platform class and the build picks the implementation.
//

#ifndef GOATEDIT_PROCESS_H
#define GOATEDIT_PROCESS_H

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace gedit {
    class Process : public std::enable_shared_from_this<Process> {
    public:
        using Ref = std::shared_ptr<Process>;

        enum class kState : uint8_t {
            kNotStarted,
            kRunning,
            kExited,            // ended by itself, 'exitCode' holds the code
            kSignalled,         // ended by a signal, 'signal' holds it - Kill() ends here
            kFailedToStart,     // no such command, or the pipes couldn't be made - 'error' says which
        };

        struct Result {
            kState state = kState::kNotStarted;
            int exitCode = -1;
            int signal = 0;
            std::string error;

            bool IsSuccess() const {
                return (state == kState::kExited) && (exitCode == 0);
            }
        };

        // Chunks as they are read, not lines: a line can arrive in two
        using OutputDelegate = std::function<void(const std::string &data)>;
        using ExitDelegate = std::function<void(const Result &result)>;
    public:
        // The platform's implementation. 'command' is looked up on PATH; 'arguments' don't include it.
        static Ref Create(std::string command, std::vector<std::string> arguments = {});
        virtual ~Process() = default;

        // Set before Start()
        void SetWorkingDirectory(const std::filesystem::path &newWorkingDirectory);
        void SetOnStdOut(OutputDelegate newOnStdOut);
        void SetOnStdErr(OutputDelegate newOnStdErr);
        // Last: all output has been delivered before it is called
        void SetOnExit(ExitDelegate newOnExit);

        // False if the child couldn't be started: the exit callback still runs, with kFailedToStart, but here, on the
        // caller's thread. Once only.
        bool Start();
        // Blocks until the child has exited and every callback has run. Not from inside a callback.
        Result WaitFor();
        // Start() and WaitFor() - for a caller that is already off the main thread
        Result Run();
        bool Kill();

        bool IsRunning();
        Result GetResult();
    protected:
        Process(std::string command, std::vector<std::string> arguments);

        // The platform's part. DoStart spawns the child and whatever reads it, and ends with Finish() either way - at
        // once if it fails. DoKill is called with 'lock' held, only while running.
        virtual bool DoStart() = 0;
        virtual bool DoKill() = 0;

        // From the platform's reader
        void DeliverStdOut(const std::string &data);
        void DeliverStdErr(const std::string &data);
        void Finish(const Result &result);
        static Result FailedToStart(const std::string &what, const std::string &reason);
    protected:
        const std::string command;
        const std::vector<std::string> arguments;
        std::filesystem::path workingDirectory;
        std::mutex lock;
    private:
        OutputDelegate onStdOut = nullptr;
        OutputDelegate onStdErr = nullptr;
        ExitDelegate onExit = nullptr;

        std::condition_variable finished;
        Result result;
        bool isFinished = false;
    };
}

#endif //GOATEDIT_PROCESS_H
