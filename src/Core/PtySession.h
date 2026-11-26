//
// A shell on a pseudo terminal: what the embedded terminal (TerminalController) needs from the platform -
// start it, write to it, size it, ask what runs in it, end it (docs/windows-port.md §2.1, §4.5). The emulation of what it
// writes back (gvt) is the same everywhere; only this part differs.
//
// Create() is defined by the platform's source file - 'unix/UnixPty.cpp' (forkpty) for Linux and macOS, later
// 'windows/ConPty.cpp' (ConPTY) - so this header names no platform class and the build picks the implementation.
//
// Output arrives on the session's own thread, through the output delegate; a shell that ends by itself (exit, killed)
// is told once through the exit delegate, on that thread too.
//

#ifndef GOATEDIT_PTYSESSION_H
#define GOATEDIT_PTYSESSION_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace gedit {
    class PtySession {
    public:
        using Ref = std::shared_ptr<PtySession>;

        enum class State {
            kIdle,
            kRunning,
            kTerminate,
            kTerminated,
        };

        // All shell output (stdout + stderr) arrives through one callback since both share the pty
        using OutputDelegate = std::function<void(const uint8_t *buffer, size_t length)>;
        // The shell ended by itself (exit, killed) - not through Close(). Called once on the session's thread, in state
        // kTerminated, after the last output. Close() and then Begin() start a new one.
        using ExitDelegate = std::function<void()>;

    public:
        // The platform's
        static Ref Create();

        virtual ~PtySession() = default;

        virtual bool Begin(const std::string &shell, const std::string &args, const std::vector<std::string> &initScript) = 0;

        // Notify the pty of the visible terminal size so programs like vi and less know how many rows and columns they
        // have to work with
        virtual void SetWindowSize(int cols, int rows) = 0;

        virtual int SendCmd(std::u32string &cmd) = 0;
        virtual int Write(uint8_t chr) = 0;
        virtual int WriteBytes(const std::string &data) = 0;
        // A job other than the shell is in the foreground of the pty: "a program is running"
        virtual bool IsForegroundJobRunning() const = 0;
        // The shell process's current directory (what a relative path in a command's output is relative to); empty
        // when it can't be read
        virtual std::string GetWorkingDirectory() const = 0;

        // Ends the shell and returns once the session's reader is gone (state kTerminated) - after it, the delegates
        // are never called again and the session can be destroyed. Bounded: a shell that won't end is killed.
        virtual void Close() = 0;

        void SetOutputDelegate(OutputDelegate handler) {
            onOutput = std::move(handler);
        }
        void SetExitDelegate(ExitDelegate handler) {
            onExit = std::move(handler);
        }

        // Written by the session's thread, polled by the UI thread - atomic, or an optimised build may hoist the load
        // out of a wait loop and spin forever
        State GetState() const {
            return state.load();
        }

    protected:
        void SetState(State newState) {
            state = newState;
        }

    protected:
        OutputDelegate onOutput = nullptr;
        ExitDelegate onExit = nullptr;

    private:
        std::atomic<State> state { State::kIdle };
    };
}

#endif //GOATEDIT_PTYSESSION_H
