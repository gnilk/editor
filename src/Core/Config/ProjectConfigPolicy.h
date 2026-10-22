//
// What a project config ('<root>/.goatedit/config.yml') may NOT set. A project config travels with the
// repository, so opening a folder must never be enough to run someone else's commands - keys that start
// processes, load code or choose where files get written stay system/user-only. Used both when the
// project config is merged at startup and when '.cfg --project set' writes one.
//

#ifndef EDITOR_PROJECTCONFIGPOLICY_H
#define EDITOR_PROJECTCONFIGPOLICY_H

#include <string>
#include <vector>

#include "Core/Config/ConfigNode.h"

namespace gedit {
    class ProjectConfigPolicy {
    public:
        // True if a project config may not set the value at 'keyPath' (e.g. {"terminal", "shell"}).
        static bool IsDenied(const std::vector<std::string> &keyPath);
        // Removes the denied keys from a loaded project config; returns them as "section.key" (for
        // logging/tests). A denied section given as a non-map is dropped whole - it would replace the
        // entire section (denied keys included) on merge.
        static std::vector<std::string> StripDeniedKeys(ConfigNode &projectConfig);
    };
}

#endif //EDITOR_PROJECTCONFIGPOLICY_H
