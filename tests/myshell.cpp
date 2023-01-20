//
// Created by gnilk on 19.01.23.
//

#include <stdint.h>
#include <string>
#include <array>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

class Command
{
public:
    using OutputDelegate = std::function<void(std::string &)>;
public:
    int             ExitStatus = 0;
    std::string     Command;
    std::string     StdIn;
    std::string     StdOut;
    std::string     StdErr;


    void SendCmd(std::string &cmd);
    void Execute();
    void ConsumePipes();
    void CleanUp();
    bool IsDone() { return done; }
    void SetStdoutDelegate(OutputDelegate handler) {
        onStdout = handler;
    }

private:
    const int READ_END = 0;
    const int WRITE_END = 1;
    OutputDelegate onStdout = nullptr;
    bool done = false;
    pid_t pid;
    int infd[2] = {0, 0};
    int outfd[2] = {0, 0};
    int errfd[2] = {0, 0};
};

void Command::SendCmd(std::string &cmd) {
    int res = write(infd[WRITE_END], cmd.c_str(), cmd.size());

    printf("Sent: %d\n", res);
}

void Command::ConsumePipes() {
    // PARENT
    if(pid < 0)
    {
        CleanUp();
        throw std::runtime_error("Failed to fork");
    }

    printf("Consuming pipes\n");

    // FIXME: Should be with select..

    std::array<char, 256> buffer;

    char *res;
    auto fd = fdopen(outfd[READ_END],"r");
    ssize_t bytes = 0;
    while(true) {
        do {
            //bytes = ::read(outfd[READ_END], buffer.data(), buffer.size());
            res = fgets(buffer.data(), buffer.size(), fd);
            if (res != nullptr) {
                printf("got: %s\n", res);
                if (onStdout != nullptr) {
//                    std::string str(buffer.data());
//                    onStdout(str);
                }
//                StdOut.append(buffer.data());
            }
        } while (res != nullptr);
    }

    int status = 0;
    ::waitpid(pid, &status, 0);

    do
    {
        bytes = ::read(errfd[READ_END], buffer.data(), buffer.size());
        StdErr.append(buffer.data(), bytes);
    }
    while(bytes > 0);

    if(WIFEXITED(status))
    {
        ExitStatus = WEXITSTATUS(status);
    }

    CleanUp();
}

void Command::Execute() {

    auto rc = ::pipe(infd);
    if(rc < 0)
    {
        throw std::runtime_error(std::strerror(errno));
    }

    rc = ::pipe(outfd);
    if(rc < 0)
    {
        ::close(infd[READ_END]);
        ::close(infd[WRITE_END]);
        throw std::runtime_error(std::strerror(errno));
    }

    rc = ::pipe(errfd);
    if(rc < 0)
    {
        ::close(infd[READ_END]);
        ::close(infd[WRITE_END]);

        ::close(outfd[READ_END]);
        ::close(outfd[WRITE_END]);
        throw std::runtime_error(std::strerror(errno));
    }

    pid = fork();
    if(pid > 0) // PARENT
    {
        ::close(infd[READ_END]);    // Parent does not read from stdin
        ::close(outfd[WRITE_END]);  // Parent does not write to stdout
        ::close(errfd[WRITE_END]);  // Parent does not write to stderr

        if(::write(infd[WRITE_END], StdIn.data(), StdIn.size()) < 0)
        {
            throw std::runtime_error(std::strerror(errno));
        }
    }
    else if(pid == 0) // CHILD
    {
        ::dup2(infd[READ_END], STDIN_FILENO);
        ::dup2(outfd[WRITE_END], STDOUT_FILENO);
        ::dup2(errfd[WRITE_END], STDERR_FILENO);

        ::close(infd[WRITE_END]);   // Child does not write to stdin
        ::close(outfd[READ_END]);   // Child does not read from stdout
        ::close(errfd[READ_END]);   // Child does not read from stderr

        ::execl("/bin/zsh", "/bin/zsh" "-is", nullptr, nullptr);
        printf("execl-done\n");
        ::exit(EXIT_SUCCESS);
    }
    done = false;
    std::thread(&Command::ConsumePipes, this).detach();
}
void Command::CleanUp() {
    ::close(infd[READ_END]);
    ::close(infd[WRITE_END]);

    ::close(outfd[READ_END]);
    ::close(outfd[WRITE_END]);

    ::close(errfd[READ_END]);
    ::close(errfd[WRITE_END]);

    done = true;
};



int main(int argc, char **argv) {
    Command cmd;
    cmd.Command = "ls -laF";
    static int lc = 0;
    cmd.SetStdoutDelegate([](std::string &str) {
        // FIXME: needs to trim (should perhaps be done in the consumer)
        printf("%d: %s\n", lc, str.c_str());
        lc++;
    });
    char buffer[256];
    cmd.Execute();
    while(!cmd.IsDone()) {
        if (fgets(buffer, 256, stdin) != nullptr) {
            std::string cmdString(buffer);
            cmd.SendCmd(cmdString);
        }
        std::this_thread::yield();
    }
}