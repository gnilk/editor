//
// Theme management for scripts ('.theme') and the command line ('--import-theme'). Import converts a VS Code
// theme or Sublime colour scheme (ThemeImporter, docs/code-index.md §11.4) and writes <name>.theme.yml +
// <name>.colors.json next to the user config - never the system assets. Switching to it stays a config edit.
//
// All calls return the lines to show the user; 'isOk' says whether the action succeeded.
//

#ifndef EDITOR_THEMESAPI_H
#define EDITOR_THEMESAPI_H

#include <string>
#include <vector>

namespace gedit {
    class ThemesAPI {
    public:
        struct Result {
            bool isOk = false;
            std::vector<std::string> lines;
        };
    public:
        ThemesAPI() = default;
        virtual ~ThemesAPI() = default;

        // 'file' relative to the working directory, '~/' for home; 'name' empty: from the theme's own name.
        // An earlier import of the same name is only overwritten when 'isForced'.
        Result Import(const std::string &file, const std::string &name, bool isForced);
    };
}

#endif //EDITOR_THEMESAPI_H
