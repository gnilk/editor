//
// The last build's errors and warnings, and where you are in them (docs/build-messages.md §6, hitlist #52).
//
// A run starts when a command we support is committed in the terminal (BeginRun) and gets the pty's bytes as they
// come (Feed - on the pty thread, so everything here is under one lock), so the list fills while the build still
// runs. It ends when its job does (UpdateJobState), or at the next command. Only a supported command starts a new
// run: an 'ls' after the build leaves its list alone.
//
// Paths are resolved when asked (ResolvePath) against what the run knew: make's / ninja's directory, the shell's
// directory, the build directory the command named, the project root (§5).
//

#ifndef GEDIT_BUILDMESSAGES_H
#define GEDIT_BUILDMESSAGES_H

#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "BuildOutputParser.h"

namespace gedit {
    class BuildMessages {
    public:
        struct Position {
            BuildMessage message;
            std::string resolvedPath;   // empty when the file isn't found
            size_t index = 0;           // from 0, in Order()
            size_t count = 0;
        };

    public:
        BuildMessages() = default;
        virtual ~BuildMessages() = default;

        // The build tools we read (§9.2): 'make', 'cmake', 'ninja', 'clang++', ... - by the command's first word,
        // after 'VAR=x' assignments and 'time' / 'env' / 'nice'; a path to the tool counts ('/usr/bin/make')
        static bool IsBuildCommand(std::string_view cmdLine);

        // A command was committed: a supported one starts a new run (the old list goes), any other ends the
        // running one. 'directory' is the shell's directory when it ran (empty: unknown). True when a run started.
        bool OnCommand(const std::string &cmdLine, const std::string &directory, const std::string &projectRoot);
        // The pty's bytes; nothing while no run is running
        void Feed(std::string_view bytes);
        // The terminal's foreground job, after a read: once a job was seen, its end ends the run. True when it did.
        bool UpdateJobState(bool isJobRunning);
        // True when a running run ended
        bool EndRun();

        bool IsRunning() const;
        bool HasRun() const;
        std::string Command() const;
        std::vector<BuildMessage> Messages() const;
        size_t NumErrors() const;
        size_t NumWarnings() const;
        // "3 errors, 12 warnings" / "no errors or warnings"
        std::string Summary() const;

        // The order the messages are stepped through and listed in: errors first, then warnings, each as the build
        // printed them; notes and messages with no file left out. Indices into 'messages'.
        static std::vector<size_t> Order(const std::vector<BuildMessage> &messages);

        // The next / previous message in Order() (wraps around); nothing when there is none
        std::optional<Position> Next();
        std::optional<Position> Previous();
        // Make 'index' (into Messages()) the current one - the modal's pick, so F8 goes on from there
        std::optional<Position> Select(size_t index);

        // The file a message points at, or empty when none of the candidates has it
        std::string ResolvePath(const BuildMessage &message) const;

    private:
        std::optional<Position> Step(int direction);
        std::optional<Position> PositionOf(size_t index) const;
        std::string ResolvePathLocked(const BuildMessage &message) const;
        // 'cmake --build <dir>', 'make -C <dir>', 'ninja -C <dir>' - the directory a tool's paths start from
        static std::string BuildDirectoryOf(const std::string &cmdLine);

    private:
        mutable std::mutex lock;
        BuildOutputParser parser;
        bool hasRun = false;
        bool isRunning = false;
        bool wasJobSeen = false;
        std::string command;
        std::string directory;
        std::string buildDirectory;
        std::string projectRoot;
        // Index into the parser's messages; none before the first step
        std::optional<size_t> current;
    };
}

#endif //GEDIT_BUILDMESSAGES_H
