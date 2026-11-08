//
// ProcessUnix - posix_spawn, a pipe each for stdout and stderr, and a thread that reads them and waits for the exit.
//

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>

#include "ProcessUnix.h"

extern char **environ;

using namespace gedit;

namespace {
    // Close-on-exec, or every other child spawned meanwhile (the terminal's shell) would hold the write end and the
    // reader would never see the end of the output. Linux makes the pipe that way at once; macOS has no pipe2, so
    // there a spawn on another thread can slip in between - the reader stops at the exit instead of the end (RunLoop).
    bool MakePipe(int fds[2]) {
#if defined(__linux__)
        return pipe2(fds, O_CLOEXEC) == 0;
#else
        if (pipe(fds) != 0) {
            return false;
        }
        fcntl(fds[0], F_SETFD, FD_CLOEXEC);
        fcntl(fds[1], F_SETFD, FD_CLOEXEC);
        return true;
#endif
    }

    void CloseFd(int &fd) {
        if (fd >= 0) {
            close(fd);
        }
        fd = -1;
    }

    Process::Result ResultFromStatus(int status) {
        Process::Result result;
        if (WIFEXITED(status)) {
            result.state = Process::kState::kExited;
            result.exitCode = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            result.state = Process::kState::kSignalled;
            result.signal = WTERMSIG(status);
        } else {
            result.state = Process::kState::kSignalled;
        }
        return result;
    }

    // While the child runs, how long a read waits before asking if it has exited
    constexpr int POLL_TIMEOUT_MS = 100;
}

Process::Ref Process::Create(std::string command, std::vector<std::string> arguments) {
    return std::make_shared<ProcessUnix>(std::move(command), std::move(arguments));
}

ProcessUnix::ProcessUnix(std::string command, std::vector<std::string> arguments) :
    Process(std::move(command), std::move(arguments)) {
}

ProcessUnix::~ProcessUnix() {
    // The thread holds a Ref until its last callback, so it is either done or this is it, letting go of the last Ref
    if (thread.joinable()) {
        if (thread.get_id() == std::this_thread::get_id()) {
            thread.detach();
        } else {
            thread.join();
        }
    }
}

bool ProcessUnix::DoStart() {
    int pipeOut[2] = {-1, -1};
    int pipeErr[2] = {-1, -1};
    if (!MakePipe(pipeOut) || !MakePipe(pipeErr)) {
        auto error = errno;
        CloseFd(pipeOut[0]);
        CloseFd(pipeOut[1]);
        CloseFd(pipeErr[0]);
        CloseFd(pipeErr[1]);
        Finish(FailedToStart("pipe", strerror(error)));
        return false;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipeOut[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, pipeErr[1], STDERR_FILENO);
    // Nothing to read: a command that asks (a credentials prompt) gets end of input instead of waiting forever
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    if (!workingDirectory.empty()) {
        // macOS 26 has the POSIX name and deprecates this one; glibc has only this one so far, and older macOS too
#if defined(__APPLE__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
        posix_spawn_file_actions_addchdir_np(&actions, workingDirectory.c_str());
#if defined(__APPLE__)
#pragma clang diagnostic pop
#endif
    }

    // posix_spawn takes 'char *const argv[]' but doesn't write to them
    std::vector<char *> argv;
    argv.push_back(const_cast<char *>(command.c_str()));
    for (auto &argument : arguments) {
        argv.push_back(const_cast<char *>(argument.c_str()));
    }
    argv.push_back(nullptr);

    pid_t child = -1;
    int status = posix_spawnp(&child, command.c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    // The child has its own copies; with ours open the reader would never see the end
    CloseFd(pipeOut[1]);
    CloseFd(pipeErr[1]);

    if (status != 0) {
        CloseFd(pipeOut[0]);
        CloseFd(pipeErr[0]);
        Finish(FailedToStart(command, strerror(status)));
        return false;
    }

    {
        std::lock_guard<std::mutex> guard(lock);
        pid = child;
    }
    auto self = std::static_pointer_cast<ProcessUnix>(shared_from_this());
    int fdStdOut = pipeOut[0];
    int fdStdErr = pipeErr[0];
    thread = std::thread([self, fdStdOut, fdStdErr]() {
        self->RunLoop(fdStdOut, fdStdErr);
    });
    return true;
}

bool ProcessUnix::DoKill() {
    if (isReaped || (pid <= 0)) {
        return false;
    }
    return kill(pid, SIGKILL) == 0;
}

void ProcessUnix::RunLoop(int fdStdOut, int fdStdErr) {
    std::string buffer(4096, '\0');
    bool isExited = false;
    int status = 0;

    auto reap = [this, &isExited, &status]() {
        std::lock_guard<std::mutex> guard(lock);
        if (waitpid(pid, &status, WNOHANG) == pid) {
            isExited = true;
            isReaped = true;
        }
    };

    // To the end of both pipes - or, once the child has exited, to the end of what is in them: a pipe inherited by
    // another child (see MakePipe) never ends.
    while ((fdStdOut >= 0) || (fdStdErr >= 0)) {
        pollfd fds[2] = {};
        int *owners[2] = {};
        nfds_t numFds = 0;
        if (fdStdOut >= 0) {
            fds[numFds] = {fdStdOut, POLLIN, 0};
            owners[numFds++] = &fdStdOut;
        }
        if (fdStdErr >= 0) {
            fds[numFds] = {fdStdErr, POLLIN, 0};
            owners[numFds++] = &fdStdErr;
        }

        int numReady = poll(fds, numFds, isExited ? 0 : POLL_TIMEOUT_MS);
        if (numReady < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (numReady == 0) {
            if (isExited) {
                break;
            }
            reap();
            continue;
        }

        for (nfds_t i = 0; i < numFds; i++) {
            if ((fds[i].revents & (POLLIN | POLLHUP | POLLERR)) == 0) {
                continue;
            }
            ssize_t numRead = read(fds[i].fd, buffer.data(), buffer.size());
            if (numRead > 0) {
                std::string data(buffer.data(), static_cast<size_t>(numRead));
                if (owners[i] == &fdStdOut) {
                    DeliverStdOut(data);
                } else {
                    DeliverStdErr(data);
                }
            } else if ((numRead == 0) || ((errno != EINTR) && (errno != EAGAIN))) {
                CloseFd(*owners[i]);
            }
        }
    }
    CloseFd(fdStdOut);
    CloseFd(fdStdErr);

    // The output has ended, so the child is ending - wait without holding the lock for long, a Kill() may be waiting
    while (!isExited) {
        reap();
        if (!isExited) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    Finish(ResultFromStatus(status));
}
