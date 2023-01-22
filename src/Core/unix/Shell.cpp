//
// Created by gnilk on 19.01.23.
//

#include <util.h>
#include <termios.h>
#include <thread>
#include <array>
#include <sys/stat.h>
#include "Core/StrUtil.h"
#include "Core/Config/Config.h"
#include "Shell.h"


bool Shell::Begin() {
    if (!StartShellProc()) {
        return false;
    }
    SendInitScript();
    return true;
}
bool Shell::StartShellProc() {
    auto shell = Config::Instance()["terminal"].GetStr("shell","/bin/bash");
    auto shellInitStr = Config::Instance()["terminal"].GetStr("init", "-ils");

    struct stat shellstat;
    // Verify if shell exists...
    if (stat(shell.c_str(),&shellstat)) {
        printf("[ERR] can't stat shell '%s' - please verify path\n", shell.c_str());
        return false;
    }
    // FIXME: We could make sure it is an executeable and so forth...

    auto rc = ::pipe(infd);
    if(rc < 0) {
        //throw std::runtime_error(std::strerror(errno));
        return false;
    }

    rc = ::pipe(outfd);
    if(rc < 0) {
        ::close(infd[READ_END]);
        ::close(infd[WRITE_END]);
        //throw std::runtime_error(std::strerror(errno));
        return false;
    }

    rc = ::pipe(errfd);
    if(rc < 0)
    {
        ::close(infd[READ_END]);
        ::close(infd[WRITE_END]);

        ::close(outfd[READ_END]);
        ::close(outfd[WRITE_END]);
        //throw std::runtime_error(std::strerror(errno));
        return false;
    }

    // This is needed for 'zsh' but not for bash (fork is enough)
    struct termios tio;
    int err = tcgetattr(STDIN_FILENO, &tio);
    if (err) {
        CleanUp();
        return false;
        //exit(1);
    }
    int amaster = 0;
    pid = forkpty(&amaster, NULL, &tio, NULL);


//    pid = fork();
    if(pid > 0) {
        // PARENT
        ::close(infd[READ_END]);    // Parent does not read from stdin
        ::close(outfd[WRITE_END]);  // Parent does not write to stdout
        ::close(errfd[WRITE_END]);  // Parent does not write to stderr
    }
    else if(pid == 0) {
        // CHILD
        ::dup2(infd[READ_END], STDIN_FILENO);
        ::dup2(outfd[WRITE_END], STDOUT_FILENO);
        ::dup2(errfd[WRITE_END], STDERR_FILENO);

        ::close(infd[WRITE_END]);   // Child does not write to stdin
        ::close(outfd[READ_END]);   // Child does not read from stdout
        ::close(errfd[READ_END]);   // Child does not read from stderr

        // TODO: Need to specify configuration in the settings regarding shell to use and the prompt!

        //::execl("/bin/bash", "/bin/bash", "-ils", nullptr);       // this works for bash

        // zsh - Can't have -i ??
        //::execl("/bin/zsh", "/bin/zsh", "-is", nullptr);
        ::execl(shell.c_str(), shell.c_str(), shellInitStr.c_str(), nullptr);
        ::exit(EXIT_SUCCESS);
    }
    std::thread(&Shell::ConsumePipes, this).detach();
    return true;
}

void Shell::SendInitScript() {
    auto initScript = Config::Instance()["terminal"].GetSequenceOfStr("bootstrap");
    for(auto &s : initScript) {
        std::string strCmd(s);
        strCmd += "\n";
        SendCmd(strCmd);
    }
}

void Shell::CleanUp() {
    ::close(infd[READ_END]);
    ::close(infd[WRITE_END]);

    ::close(outfd[READ_END]);
    ::close(outfd[WRITE_END]);

    ::close(errfd[READ_END]);
    ::close(errfd[WRITE_END]);
}

int Shell::SendCmd(std::string &cmd) {
    return (write(infd[WRITE_END], cmd.c_str(), cmd.size()));
}


void Shell::ConsumePipes() {
    // PARENT
    if(pid < 0) {
        CleanUp();
        return;
    }

    std::array<char, 256> buffer;

//    StdOut.clear();
//    StdErr.clear();
    char *res;
    auto fd = fdopen(outfd[READ_END],"r");
    ssize_t bytes = 0;
    // TODO: Fix this - need select here
    while(true) {
        do {
            //bytes = ::read(outfd[READ_END], buffer.data(), buffer.size());
            res = fgets(buffer.data(), buffer.size(), fd);
            if ((onStdout != nullptr) && (res != NULL)) {
                //printf("GOT: %s\n", buffer.data());
                std::string str(buffer.data());
                strutil::trim(str);
                onStdout(str);
            }
        } while (res != nullptr);
    }

    do {
        bytes = ::read(errfd[READ_END], buffer.data(), buffer.size());
//        StdErr.append(buffer.data(), bytes);
    }
    while(bytes > 0);

    int status = 0;
    ::waitpid(pid, &status, 0);

    if(WIFEXITED(status)) {
        exitStatus = WEXITSTATUS(status);
    }
    CleanUp();
}
