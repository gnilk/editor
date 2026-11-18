//
// What the user should know about how the editor started (a project's settings not used, a theme that can't load, a
// language file with a mistake, ...). Each goes to stderr at once - a launch from a shell sees it there - and to the
// terminal panel once it is up, since a launch through 'goat' ('open') or a desktop launcher has no stderr anyone
// reads (hitlist #50). Notices from before the panel exists are held until Attach.
//

#ifndef GOATEDIT_STARTUPNOTICES_H
#define GOATEDIT_STARTUPNOTICES_H

#include <mutex>
#include <string>
#include <vector>

#include "Core/RuntimeConfig.h"

namespace gedit {
    class StartupNotices {
    public:
        StartupNotices() = default;
        virtual ~StartupNotices() = default;

        // One line, without the "goatedit: " prefix (added here)
        void Add(const std::string &text);
        // The console is up: the held notices go to it, in order, and later ones straight there. nullptr (no terminal
        // panel - headless) keeps holding them.
        void Attach(IOutputConsole *newConsole);
        // The held notices, prefixed - what Attach will write
        std::vector<std::string> Held() const;
        // Stop writing to stderr (the unit tests)
        void SetEchoToStderr(bool isEcho) {
            isEchoToStderr = isEcho;
        }

    private:
        mutable std::mutex lock;
        std::vector<std::string> held;
        IOutputConsole *console = nullptr;
        bool isEchoToStderr = true;
    };
}

#endif //GOATEDIT_STARTUPNOTICES_H
