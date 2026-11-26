//
// Created by gnilk on 07.08.23.
//
// The desktop session this process runs in (XDG_SESSION_TYPE, DISPLAY, ...). Where the user's files live is UserDirs'
// (docs/windows-port.md §2.1: the XDG base dirs moved there, one table per platform).
//

#ifndef EDITOR_XDGENVIRONMENT_H
#define EDITOR_XDGENVIRONMENT_H

#include <string>

namespace gedit {
    class XDGEnvironment {
    public:
        virtual ~XDGEnvironment() = default;
        static const XDGEnvironment &Instance();

        // XDG_SESSION_TYPE, 'tty' when unset
        [[nodiscard]]
        const std::string &GetSessionType() const;

        // No display to open a window on, so a launch without --backend gets the ANSI backend (#46). Linux: a 'tty'
        // session (logind sets it for ssh), or neither DISPLAY nor WAYLAND_DISPLAY set (containers, ssh without
        // pam_systemd). macOS: a login over ssh - a window would open on the Mac's own screen, if at all.
        [[nodiscard]]
        bool IsTerminalSession() const;
        // The decision, with the environment passed in (an empty value is an unset one) - for the tests
        [[nodiscard]]
        static bool IsTerminalSession(const std::string &sessionType, const std::string &display,
                                      const std::string &waylandDisplay, const std::string &sshConnection);

    private:
        XDGEnvironment();

    private:
        std::string sessionType;
    };
}


#endif //EDITOR_XDGENVIRONMENT_H
