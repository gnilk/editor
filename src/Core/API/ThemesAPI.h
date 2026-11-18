//
// Theme management for scripts ('.theme') and the command line ('--import-theme'). Import converts a VS Code
// theme or Sublime colour scheme (ThemeImporter, docs/code-index.md §11.4) and writes <name>.theme.yml +
// <name>.colors.json in themes/ next to the user config - never the system assets. List shows the themes there and
// the shipped ones; Use switches to one, live, and saves the choice in the user config (docs/theme-collection.md §5).
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
        // A theme file found by List: in the user's themes/, the user dir itself (imported before themes/) or the
        // shipped Themes/
        struct ThemeInfo {
            std::string fileName;       // 'x.theme.yml' - what main.theme names
            std::string name;           // the theme's own name ('name:')
            std::string author;
            std::string license;
            bool isUser = false;
        };
    public:
        ThemesAPI() = default;
        virtual ~ThemesAPI() = default;

        // 'file' relative to the working directory, '~/' for home; 'name' empty: from the theme's own name.
        // An earlier import of the same name is only overwritten when 'isForced'.
        Result Import(const std::string &file, const std::string &name, bool isForced);
        // Every theme 'use' can switch to, the current one marked - one shadowed by a user theme of the same file
        // name is the user's
        Result List();
        // 'name': the file name, without '.theme.yml', or the theme's own name (case doesn't matter). Switches live and
        // writes main.theme to the user config.
        Result Use(const std::string &name);

        static std::vector<ThemeInfo> Available();
    };
}

#endif //EDITOR_THEMESAPI_H
