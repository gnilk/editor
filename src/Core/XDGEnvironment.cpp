//
// Created by gnilk on 07.08.23.
//
// The desktop session - see XDGEnvironment.h. The XDG base dirs are UserDirs'.
//

#include "Core/Platform.h"
#include "XDGEnvironment.h"

using namespace gedit;

// Returns the global instance - yes we are a singleton - makes sense in this case...
const XDGEnvironment &XDGEnvironment::Instance() {
    static XDGEnvironment glbInstance;
    return glbInstance;
}

XDGEnvironment::XDGEnvironment() : sessionType(Platform::GetEnvOr("XDG_SESSION_TYPE", "tty")) {
}

const std::string &XDGEnvironment::GetSessionType() const {
    return sessionType;
}

bool XDGEnvironment::IsTerminalSession() const {
    auto sshConnection = Platform::GetEnvOr("SSH_CONNECTION", "");
    if (sshConnection.empty()) {
        sshConnection = Platform::GetEnvOr("SSH_TTY", "");
    }
    return IsTerminalSession(sessionType, Platform::GetEnvOr("DISPLAY", ""), Platform::GetEnvOr("WAYLAND_DISPLAY", ""),
                             sshConnection);
}

bool XDGEnvironment::IsTerminalSession([[maybe_unused]] const std::string &sessionType,
                                       [[maybe_unused]] const std::string &display,
                                       [[maybe_unused]] const std::string &waylandDisplay,
                                       [[maybe_unused]] const std::string &sshConnection) {
#ifdef GEDIT_MACOS
    // The window server doesn't care about DISPLAY; over ssh there is no screen of the user's to draw on
    return !sshConnection.empty();
#else
    if (sessionType == "tty") {
        return true;
    }
    return display.empty() && waylandDisplay.empty();
#endif
}
