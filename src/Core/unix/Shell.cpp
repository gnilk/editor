//
// Created by gnilk on 19.01.23.
//

#if defined(GEDIT_MACOS)
#include <util.h>
#elif defined(GEDIT_LINUX)
#include <pty.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include <termios.h>
#include <thread>
#include <array>
#include <sys/stat.h>
#include <string.h>

#include <poll.h>

#include <logger.h>
#include "Core/UnicodeHelper.h"
#include "Core/StrUtil.h"
#include "Core/Config/Config.h"
#include "Shell.h"

using namespace gedit;

bool Shell::Begin() {
    logger = gnilk::Logger::GetLogger("Shell");
    if (!StartShellProc()) {
        return false;
    }
    SendInitScript();
    return true;
}
bool Shell::StartShellProc() {
    auto shell = Config::Instance()["terminal"].GetStr("shell","/bin/bash");
    auto shellInitStr = Config::Instance()["terminal"].GetStr("init", "-ils");

    logger->Debug("Starting shell process: %s %s", shell.c_str(), shellInitStr.c_str());

    struct stat shellstat;
    // Verify if shell exists...
    if (stat(shell.c_str(),&shellstat)) {
        logger->Error("[ERR] can't stat shell '%s' - please verify path", shell.c_str());
        return false;
    }
    // FIXME: We could make sure it is an executeable and so forth...

    auto rc = ::pipe(infd);
    if(rc < 0) {
        //throw std::runtime_error(std::strerror(errno));
        logger->Error("Failed to create stdin pipe!");
        return false;
    }

    rc = ::pipe(outfd);
    if(rc < 0) {
        ::close(infd[READ_END]);
        ::close(infd[WRITE_END]);
        //throw std::runtime_error(std::strerror(errno));
        logger->Error("Failed to create stdout pipe");
        return false;
    }

    rc = ::pipe(errfd);
    if(rc < 0) {
        logger->Error("Failed to create stderr pipe");
        ::close(infd[READ_END]);
        ::close(infd[WRITE_END]);

        ::close(outfd[READ_END]);
        ::close(outfd[WRITE_END]);
        //throw std::runtime_error(std::strerror(errno));
        return false;
    }

    // This is needed for 'zsh' but not for bash (fork is enough)
    struct termios tio;
    struct termios *ptrTermIO = nullptr;

    // This will fail if we are starting through the UI launcher...
    // It is however ok to provide a 'nullptr' to forkpty for the termios
    int err = tcgetattr(STDIN_FILENO, &tio);
    if (err) {
        logger->Error("failed tcgetattr, err: %d:%s", errno, strerror(errno));
        logger->Debug("This can happen when started as a UI application - not sure how ZSH will respond");
    } else {
        ptrTermIO = &tio;
    }

    logger->Error("forking pty!");

    int amaster = 0;
    pid = forkpty(&amaster, NULL, ptrTermIO, NULL);


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


        // zsh - Can't have -i ??
        //::execl("/bin/zsh", "/bin/zsh", "-is", nullptr);
        ::execl(shell.c_str(), shell.c_str(), shellInitStr.c_str(), nullptr);
        ::exit(EXIT_SUCCESS);
    }
    logger->Debug("Starting pipe threads");
    std::thread(&Shell::ConsumePipes, this).detach();
    return true;
}

void Shell::SendInitScript() {
    // Perhaps move this to CommandController - which allows us to execute internal plugins as well as shell commands...
    auto initScript = Config::Instance()["terminal"].GetSequenceOfStr("bootstrap");
    logger->Debug("Executing shell bootstrap script");
    for(auto &s : initScript) {
        std::u32string strCmd;
        if (!UnicodeHelper::ConvertUTF8ToUTF32String(strCmd, s)) {
            logger->Error("SendInitScript, failed to converts to u32 for '%s'", s.c_str());
        }
        strCmd += U"\n";
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

int Shell::SendCmd(std::u32string &cmd) {

    std::string strCmdUTF8;
    if (!UnicodeHelper::ConvertUTF32ToUTF8String(strCmdUTF8, cmd)) {
        logger->Error("SendCmd, failed to convert to UTF8!");
        return -1;
    }
    return (write(infd[WRITE_END], strCmdUTF8.c_str(), strCmdUTF8.size()));
}

// Maximum 100ms per poll
#ifndef GEDIT_DEFAULT_POLL_TMO_MS
#define GEDIT_DEFAULT_POLL_TMO_MS 100
#endif

void Shell::ConsumePipes() {
    // PARENT
    if(pid < 0) {
        CleanUp();
        return;
    }


    auto fdOut = fdopen(outfd[READ_END], "r");
    auto fdErr = fdopen(errfd[READ_END], "r");
    ssize_t bytes = 0;

    nfds_t nfds;
    struct pollfd fds[2];

    nfds = 2;
    fds[0].fd = outfd[READ_END];
    fds[0].events = POLLIN;
    fds[1].fd = errfd[READ_END];
    fds[1].events = POLLIN;

    while(true) {
        int poll_num = poll(fds, nfds, GEDIT_DEFAULT_POLL_TMO_MS);
        if (poll_num == -1) {
            if (errno == EINTR) continue;
            logger->Error("poll error=%d:%s", errno, strerror(errno));
            break;
        }
        if (poll_num == 0) continue;
        if ((fds[0].revents & POLLIN) && (onStdout != nullptr)) {
            ReadAndDispatch(fdOut, onStdout);
        }
        // STDERR handling doesn't quite work - no clue why...
        // OR it seems the combo of stdout/stderr polling is causing problems
        // needs more debugging...
//        if ((fds[1].revents & POLLIN) && (onStdout != nullptr)) {
//            ReadAndDispatch(fdErr, [this](std::string &str) {
//                logger->Debug("stderr: %s", str.c_str());
//            });
//        }

    }

//    do {
//        bytes = ::read(errfd[READ_END], buffer.data(), buffer.size());
////        StdErr.append(buffer.data(), bytes);
//    }
//    while(bytes > 0);

    int status = 0;
    waitpid(pid, &status, 0);

    if(WIFEXITED(status)) {
        exitStatus = WEXITSTATUS(status);
    }
    CleanUp();
}

void Shell::ReadAndDispatch(FILE *fd, OutputDelegate onData) {
    std::array<char, 256> buffer;
    char *res;

    do {
        res = fgets(buffer.data(), buffer.size(), fd);
        if ((res != nullptr) && (onStdout != nullptr)) {

            std::u32string str;
            if (!UnicodeHelper::ConvertUTF8ToUTF32String(str, buffer.data())) {
                logger->Error("ReadAndDispatch, failed to UTF32 conversion for '%s'",buffer.data());
                continue;
            }
            onData(str);
        }
    } while(res != nullptr);
}
