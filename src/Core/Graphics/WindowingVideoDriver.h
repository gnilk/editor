//
// Which SDL video drivers put a window on a desktop (#49).
//

#ifndef GEDIT_WINDOWINGVIDEODRIVER_H
#define GEDIT_WINDOWINGVIDEODRIVER_H

#include <cstdlib>
#include <cstring>

namespace gedit {
    // With no X/Wayland (over ssh, no DISPLAY) SDL does not fail - it falls through to kmsdrm, offscreen or dummy
    // and runs with nothing anyone can see. Those are refused, so the editor fails with a message instead. A driver
    // asked for by name (SDL_VIDEODRIVER) is taken as it is.
    inline bool IsWindowingVideoDriver(const char *driverName) {
        auto requested = std::getenv("SDL_VIDEODRIVER");
        if ((requested != nullptr) && (requested[0] != '\0')) {
            return true;
        }
        if (driverName == nullptr) {
            return false;
        }
        static const char *windowing[] = {"x11", "wayland", "cocoa", "windows"};
        for (auto name : windowing) {
            if (std::strcmp(driverName, name) == 0) {
                return true;
            }
        }
        return false;
    }
}

#endif //GEDIT_WINDOWINGVIDEODRIVER_H
