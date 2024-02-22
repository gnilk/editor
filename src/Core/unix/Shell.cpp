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
#include <fcntl.h>

#include <poll.h>

#include <logger.h>

#include "Core/AnsiParser.h"
#include "Core/HexDump.h"
#include "Core/UnicodeHelper.h"
#include "Core/StrUtil.h"
#include "Core/Config/Config.h"
#include "Shell.h"
#include "Core/CompileTimeConfig.h"

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
        logger->Error("[ERR] can't projectstat shell '%s' - please verify path", shell.c_str());
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
    if (err < 0) {
        logger->Error("failed tcgetattr, err: %d:%s", errno, strerror(errno));
        logger->Debug("This can happen when started as a UI application - not sure how ZSH will respond");
    } else {
        ptrTermIO = &tio;
    }

    struct termios shellTermios;

/*
    err = ttySetRaw(STDIN_FILENO, &shellTermios);
    if (err < 0) {
        perror("ttySetRaw");
    }
*/

    // Create a default termio structure
    shellTermios.c_iflag = TTYDEF_IFLAG;
    shellTermios.c_oflag = TTYDEF_OFLAG;
    shellTermios.c_cflag = TTYDEF_CFLAG;
    shellTermios.c_lflag = TTYDEF_LFLAG;
    // Disable echo - I can't get this to work properly...
    //shellTermios.c_lflag &= ~ECHO;
#ifndef TTYDEFCHARS
    cc_t    ttydefchars[NCCS] = {
            CEOF,   CEOL,   CEOL,   CERASE, CWERASE, CKILL, CREPRINT,
            _POSIX_VDISABLE, CINTR, CQUIT,  CSUSP,  CDSUSP, CSTART, CSTOP,  CLNEXT,
            CDISCARD, CMIN, CTIME,  CSTATUS, _POSIX_VDISABLE
    };
#endif
    memcpy(shellTermios.c_cc,ttydefchars, NCCS);

    // For some reason the underlying shell TERMIOS structure doesn't fully do this..
    // we get some echo from it - or there is a stray printf in my code I haven't found yet
    // but if we do this - I can see it...
//    tcsetattr(STDIN_FILENO, TCSANOW, &shellTermios);

    logger->Info("forking pty!");

    int amaster = 0;
    //pid = forkpty(&amaster, NULL, ptrTermIO, NULL);
//    pid = forkpty(&amaster, NULL, &shellTermios, NULL);
    pid = forkpty(&amaster, NULL, NULL, NULL);

    if(pid > 0) {

        // PARENT
        ::close(infd[READ_END]);    // Parent does not read from stdin
        ::close(outfd[WRITE_END]);  // Parent does not write to stdout
        ::close(errfd[WRITE_END]);  // Parent does not write to stderr
    }
    else if(pid == 0) {
        // CHILD
        // Note to self: DO NOT TOUCH THE TTY here - that's why I use forkpty...

        // To debug fork on GDB
        //  set follow-fork-mode child
        //  set detach-on-fork off
        //

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

int Shell::Write(uint8_t chr) {
    return write(infd[WRITE_END], &chr, 1);
}

// FIXME: There should be an 'WriteUTF8()' as well

void Shell::ConsumePipes() {
    // PARENT
    if(pid < 0) {
        CleanUp();
        return;
    }


    auto fdOut = fdopen(outfd[READ_END], "r");
    auto fdErr = fdopen(errfd[READ_END], "r");

    // mark as non-blocking
    auto flags = fcntl(outfd[READ_END], F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(outfd[READ_END], F_SETFL, flags);

    flags = fcntl(errfd[READ_END], F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(errfd[READ_END], F_SETFL, flags);



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
        if ((fds[1].revents & POLLIN) && (onStderr != nullptr)) {
            ReadAndDispatch(fdErr, onStderr);
        }
        std::this_thread::yield();
    }


    int status = 0;
    waitpid(pid, &status, 0);

    if(WIFEXITED(status)) {
        exitStatus = WEXITSTATUS(status);
    }
    CleanUp();
}
/*
void Shell::ReadAndDispatch(FILE *fd, OutputDelegate onData) {
#ifndef GEDIT_TERMINAL_LINE_SIZE
    #define GEDIT_TERMINAL_LINE_SIZE 1024
#endif
    static uint8_t buffer[GEDIT_TERMINAL_LINE_SIZE];
    char *res;

    do {
        memset(buffer, 0, GEDIT_TERMINAL_LINE_SIZE);
        res = fgets((char *)buffer, GEDIT_TERMINAL_LINE_SIZE, fd);
        if ((res != nullptr) && (onStdout != nullptr)) {

            AnsiParser ansiParser;
            auto stripped = ansiParser.Parse(buffer, GEDIT_TERMINAL_LINE_SIZE);

            std::u32string str;
            if (!UnicodeHelper::ConvertUTF8ToUTF32String(str, stripped)) {
                logger->Error("ReadAndDispatch, failed to UTF32 conversion for '%s'",(char *)stripped.c_str());
                continue;
            }
            onData(str);
        }
    } while(res != nullptr);
    res = nullptr;
}
 */
void Shell::ReadAndDispatch(FILE *fd, OutputDelegate onData) {
#define MAX_LINE_SIZE 1024
    uint8_t buffer[1024];
    ssize_t res;

    AnsiParser ansiParser;

    int fno = fileno(fd);

    do {
        memset(buffer,0, MAX_LINE_SIZE);
        // Note to self, DO NOT READ DATA LIKE THIS!!!!!!!!!!
        res = read(fno, buffer, MAX_LINE_SIZE);
        if ((res < 0) && (errno == EAGAIN)) {
            continue;
        }
        if ((res > 0) && (onStdout != nullptr)) {
            // Not sure this is the right way to do this!
            auto strStripped = ansiParser.Parse(buffer, MAX_LINE_SIZE);
            std::u32string str;
            if (!UnicodeHelper::ConvertUTF8ToUTF32String(str, strStripped)) {
                logger->Error("ReadAndDispatch, failed to UTF32 conversion for '%s'",strStripped.c_str());
                continue;
            }
            onData(str);
        }
    } while(res > 0);
    //res = nullptr;
}

