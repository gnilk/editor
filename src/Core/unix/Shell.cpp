//
// Created by gnilk on 19.01.23.
//

#if defined(GEDIT_MACOS)
#include <util.h>
#elif defined(GEDIT_LINUX)
#include <pty.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ttydefaults.h>
#include <unistd.h>
#endif

#include <termios.h>
#include <thread>
#include <array>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>

#include <logger.h>

#include "Core/VTermParser.h"
#include "Core/HexDump.h"
#include "Core/UnicodeHelper.h"
#include "Core/StrUtil.h"
#include "Shell.h"

#ifndef GEDIT_DEFAULT_POLL_TMO_MS
#define GEDIT_DEFAULT_POLL_TMO_MS 100
#endif

using namespace gedit;

static Shell *glbShell = nullptr;

static volatile sig_atomic_t gSignalStatus = {};
static void signal_handler(int signal)
{
    gSignalStatus = signal;
    glbShell->OnSignal(signal);
}

void Shell::OnSignal(int sig) {
    auto pidSig = getpid();
    logger->Debug("Signal raised, sig=%d, for pid=%d", sig, pidSig);
    if (GetState() == State::kRunning) {
        extSignal = sig;
        ChangeState(State::kTerminate);
    } else {
        logger->Debug("Not running, ignoring signal!");
    }
}

bool Shell::Begin(const std::string &shell, const std::string &args, const std::vector<std::string> &initScript) {
    logger = gnilk::Logger::GetLogger("Shell");
    glbShell = this;
    if (!StartShellProc(shell, args)) {
        return false;
    }
    SendInitScript(initScript);
    return true;
}

bool Shell::StartShellProc(const std::string &shell, const std::string &shellInitStr) {
    //auto shell = Config::Instance()["terminal"].GetStr("shell","/bin/bash");
    //auto shellInitStr = Config::Instance()["terminal"].GetStr("init", "-ils");

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
        logger->Error("Failed to create stdin pipe, errno=%d", errno);
        return false;
    }

    rc = ::pipe(outfd);
    if(rc < 0) {
        logger->Error("Failed to create stdout pipe, errno=%d", errno);
        ::close(infd[READ_END]);
        ::close(infd[WRITE_END]);
        return false;
    }

    rc = ::pipe(errfd);
    if(rc < 0) {
        logger->Error("Failed to create stderr pipe, errno=%d", errno);
        ::close(infd[READ_END]);
        ::close(infd[WRITE_END]);

        ::close(outfd[READ_END]);
        ::close(outfd[WRITE_END]);
        return false;
    }

    // this only fills out the structure with the 'default' values.
    InitializeDefaultTermiosAttr();
    // Apply (we can also apply in the forkpty call) - actually don'y know what's best..
    // Or skip - default is skip this...
    if (bSetTermios) {
        tcsetattr(STDIN_FILENO, TCSANOW, &shellTermios);
    }

    logger->Debug("Setting up Signal Handling");
    logger->Debug("Main pid=%d", getpid());
    // FIXME: Use sigaction
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGCHLD, signal_handler);

    logger->Debug("SignalValue=%d", gSignalStatus);

    logger->Info("Forking PTY!");

    int amaster = 0;
    //pid = forkpty(&amaster, NULL, ptrTermIO, NULL);
    //pid = forkpty(&amaster, NULL, &shellTermios, NULL);
    pid = forkpty(&amaster, NULL, NULL, NULL);

    if(pid > 0) {
        // PARENT
        logger->Debug("Started child with pid=%d", pid);
        ::close(infd[READ_END]);    // Parent does not read from stdin
        ::close(outfd[WRITE_END]);  // Parent does not write to stdout
        ::close(errfd[WRITE_END]);  // Parent does not write to stderr
    }
    else if(pid == 0) {
        // CHILD
        // Note to self: DO NOT TOUCH THE TTY here - that's why I use forkpty...

        // Note: for the logger to work we can't use an in-proc queue we need to have the PIPE IPC in the logger

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

        // This will never be reached?
        exit(EXIT_SUCCESS);
    }
    logger->Debug("Starting pipe threads");
    std::thread(&Shell::ConsumePipes, this).detach();
    return true;
}

void Shell::InitializeDefaultTermiosAttr() {

    // Create a default termio structure
    shellTermios.c_iflag = TTYDEF_IFLAG;
    shellTermios.c_oflag = TTYDEF_OFLAG;
    shellTermios.c_cflag = TTYDEF_CFLAG;
    shellTermios.c_lflag = TTYDEF_LFLAG;
    // Disable echo - I can't get this to work properly...
    shellTermios.c_lflag &= ~ECHO;
#ifndef TTYDEFCHARS
    cc_t    ttydefchars[NCCS] = {
            CEOF,   CEOL,   CEOL,   CERASE, CWERASE, CKILL, CREPRINT,
            _POSIX_VDISABLE, CINTR, CQUIT,  CSUSP,  CDSUSP, CSTART, CSTOP,  CLNEXT,
            CDISCARD, CMIN, CTIME,  CSTATUS, _POSIX_VDISABLE
    };
#endif
    memcpy(shellTermios.c_cc,ttydefchars, NCCS);

}

void Shell::SendInitScript(const std::vector<std::string> &initScript) {
    // Perhaps move this to CommandController - which allows us to execute internal plugins as well as shell commands...
    //auto initScript = Config::Instance()["terminal"].GetSequenceOfStr("bootstrap");
    logger->Debug("Executing shell bootstrap script");
    for(const auto &s : initScript) {
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

// Consume pipes - called from thread
void Shell::ConsumePipes() {
    // PARENT
    if(pid < 0) {
        CleanUp();
        return;
    }

    ChangeState(State::kRunning);

    auto fdOut = fdopen(outfd[READ_END], "r");
    auto fdErr = fdopen(errfd[READ_END], "r");

    // mark as non-blocking
    auto flags = fcntl(outfd[READ_END], F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(outfd[READ_END], F_SETFL, flags);

    flags = fcntl(errfd[READ_END], F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(errfd[READ_END], F_SETFL, flags);

    nfds_t nfds;
    struct pollfd fds[2];

    nfds = 2;
    fds[0].fd = outfd[READ_END];
    fds[0].events = POLLIN;
    fds[1].fd = errfd[READ_END];
    fds[1].events = POLLIN;

    bool bContinue = true;
    while(bContinue) {
        if (GetState() == State::kTerminate) {
            break;
        }

        // Poll the pipe to check if we have data
        int poll_num = poll(fds, nfds, GEDIT_DEFAULT_POLL_TMO_MS);
        if (poll_num == -1) {
            logger->Error("poll error=%d:%s", errno, strerror(errno));
            if (errno == EINTR) continue;
            break;
        }
        if (poll_num == 0) {
            continue;
        }
        if ((fds[0].revents & POLLIN) && (onStdout != nullptr)) {
            bContinue = ReadAndDispatch(Stream::kStdOut, fdOut, onStdout);
        }
        // STDERR handling doesn't quite work - no clue why...
        // OR it seems the combo of stdout/stderr polling is causing problems
        // needs more debugging...
        if ((fds[1].revents & POLLIN) && (onStderr != nullptr)) {
            bContinue = ReadAndDispatch(Stream::kStdErr, fdErr, onStderr);
        }
        std::this_thread::yield();
    }

    if (GetState() != State::kTerminate) {
        logger->Error("Something went wrong, leaving...");
    }

    // Was this a termination request?
    if (extSignal == SIGTERM) {
        // ask the child to terminate
        logger->Debug("Termination through SIGTERM, sending kill signal to child");
        kill(pid,SIGKILL);
        int status = 0;
        logger->Debug("Waitpid, pid=%d", pid);
        waitpid(pid, &status, 0);

        if(WIFEXITED(status)) {
            exitStatus = WEXITSTATUS(status);
        }
    }
    logger->Debug("Leaving ConsumePipe for pid=%d (child=%d)", getpid(), pid);
    CleanUp();

    ChangeState(State::kTerminated);
}


bool Shell::ReadAndDispatch(Stream stream, FILE *fd, OutputDelegate onData) {
#define MAX_LINE_SIZE 1024
    uint8_t buffer[1024];
    //char *res;
    ssize_t res;

    if (fRawAnsi == nullptr) {
        fRawAnsi = fopen("raw_ansi.txt","w+");
    }

    VTermParser ansiParser;

    int fno = fileno(fd);

    do {
        if (gSignalStatus) {
            logger->Debug("ReadAndDispatch, signal raised - we are leaving");
            return false;
        }


        memset(buffer,0, MAX_LINE_SIZE);
        res = read(fno, buffer, MAX_LINE_SIZE);
        if ((res < 0) && (errno == EAGAIN)) {
            continue;
        }
        if ((res > 0) && (onData != nullptr)) {
            if (fRawAnsi != nullptr) {
                fprintf(fRawAnsi,"=== Begin-Data [%zu bytes, stream=%d] ===\n", res, static_cast<int>(stream));
                HexDump::ToFile(fRawAnsi, buffer, res);
                fprintf(fRawAnsi,"\n=== End-Data ===\n");
                fflush(fRawAnsi);
            }
            onData(stream, buffer, res);
        } else {
            // error
            if (errno == EPIPE) {
                logger->Debug("ReadAndDispatch,Pipe error - did we close?");
            } else {
                logger->Error("ReadAndDispatch, uncaught error, errno=%d", errno);
                printf("errno: %d\n", errno);
                perror("ReadAndDispatch");
            }
            return false;
        }
    } while(res > 0);
    return true;
}
