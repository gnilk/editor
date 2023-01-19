//
// Created by gnilk on 19.01.23.
//

#include <thread>
#include <array>
#include "ShellCommand.h"

void ShellCommand::Execute(std::string &cmdString) {

    Command = cmdString;
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
        ::close(infd[WRITE_END]); // Done writing
    }
    else if(pid == 0) // CHILD
    {
        ::dup2(infd[READ_END], STDIN_FILENO);
        ::dup2(outfd[WRITE_END], STDOUT_FILENO);
        ::dup2(errfd[WRITE_END], STDERR_FILENO);

        ::close(infd[WRITE_END]);   // Child does not write to stdin
        ::close(outfd[READ_END]);   // Child does not read from stdout
        ::close(errfd[READ_END]);   // Child does not read from stderr

        ::execl("/bin/zsh", "zsh", "-lc", Command.c_str(), nullptr);
        ::exit(EXIT_SUCCESS);
    }
    done = false;
    std::thread(&ShellCommand::ConsumePipes, this).detach();
}

void ShellCommand::CleanUp() {
    ::close(infd[READ_END]);
    ::close(infd[WRITE_END]);

    ::close(outfd[READ_END]);
    ::close(outfd[WRITE_END]);

    ::close(errfd[READ_END]);
    ::close(errfd[WRITE_END]);

    done = true;
};

void ShellCommand::ConsumePipes() {
    // PARENT
    if(pid < 0) {
        CleanUp();
        throw std::runtime_error("Failed to fork");
    }


    std::array<char, 256> buffer;

    StdOut.clear();
    StdErr.clear();
    char *res;
    auto fd = fdopen(outfd[READ_END],"r");
    ssize_t bytes = 0;
    do {
        //bytes = ::read(outfd[READ_END], buffer.data(), buffer.size());
        res = fgets(buffer.data(), buffer.size(), fd);
        if ((onStdout != nullptr) && (res != NULL)) {
            std::string str(buffer.data());
            onStdout(str);
        }
        if (res != NULL) {
            StdOut.append(buffer.data());
        }
    } while(res != nullptr);

    do {
        bytes = ::read(errfd[READ_END], buffer.data(), buffer.size());
        StdErr.append(buffer.data(), bytes);
    }
    while(bytes > 0);

    int status = 0;
    ::waitpid(pid, &status, 0);

    if(WIFEXITED(status)) {
        ExitStatus = WEXITSTATUS(status);
    }
    CleanUp();
}
