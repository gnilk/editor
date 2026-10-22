//
// Config access for scripts ('.cfg'). Levels: the RUNNING config (system -> user -> project, merged at
// startup), the USER config file and the PROJECT config file. The system config is read-only from here.
//
// Nothing is kept per level: every call loads the level files fresh from disk into temporaries, so a
// hand-edit since startup is never overwritten with a stale copy. A set edits the target FILE in place
// (ConfigFilePatcher - comments survive, no yaml-cpp round-trip), then puts the resulting EFFECTIVE value
// (highest level defining the key) into the running config - so a user value the project overrides
// leaves the running instance unchanged, with a warning.
//
// All calls return the lines to show the user; 'isOk' says whether the action succeeded.
//

#ifndef EDITOR_CONFIGAPI_H
#define EDITOR_CONFIGAPI_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gedit {
    class ConfigAPI {
    public:
        enum class kLevel {
            kRunning,
            kUser,
            kProject,
        };
        struct Result {
            bool isOk = false;
            std::vector<std::string> lines;
        };
    public:
        ConfigAPI() = default;
        virtual ~ConfigAPI() = default;

        // "" -> running, "user", "project"; nullopt for anything else
        static std::optional<kLevel> LevelFromString(const std::string &level);
        // "editorview.wrap" -> {"editorview", "wrap"}; empty when malformed
        static std::vector<std::string> SplitKey(const std::string &key);

        // 'section.key = value' lines (running: with the level each value comes from), filtered by prefix
        Result List(kLevel level, const std::string &prefix);
        Result Get(kLevel level, const std::string &key);
        // level must be user or project
        Result Set(kLevel level, const std::string &key, const std::string &value);
        // Open the level's config file in a buffer (created, with a header comment, if missing)
        Result Edit(kLevel level);

        // The file for a level - the one that was loaded if it exists, else where it would be created.
        // Empty for the running level, and for project when no project is open.
        std::filesystem::path ConfigFilePath(kLevel level);
    private:
        // Re-run ReInitView over the view tree (posted to the runloop) so views pick up a changed value
        static void ReInitializeViews();
    };
}

#endif //EDITOR_CONFIGAPI_H
