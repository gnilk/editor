//
// The last build's errors and warnings, and where you are in them (docs/build-messages.md §6, hitlist #52).
//
// A run starts when a command we support is committed in the terminal (BeginRun) and gets the pty's bytes as they
// come (Feed - on the pty thread, so everything here is under one lock), so the list fills while the build still
// runs. It ends when the shell says the command finished (OnCommandDone), else when its job is seen to end
// (UpdateJobState) or no job runs once the terminal has been quiet a moment (OnQuiet - a build that started and ended
// between two reads), or at the next command. Only a supported command starts a new
// run: an 'ls' after the build leaves its list alone.
//
// Paths are resolved when asked (ResolvePath) against what the run knew: make's / ninja's directory, the shell's
// directory, the build directory the command named, the project root (§5).
//

#ifndef GEDIT_BUILDMESSAGES_H
#define GEDIT_BUILDMESSAGES_H

#include <cstdint>
#include <map>
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

        // The build tools we read (§9.2): 'make', 'cmake', 'ninja', 'clang++', 'trun', 'cargo', 'python3', ... - by the command's first word,
        // after 'VAR=x' assignments and 'time' / 'env' / 'nice'; a path to the tool counts ('/usr/bin/make')
        static bool IsBuildCommand(std::string_view cmdLine);

        // A command was committed: a supported one starts a new run (the old list goes), any other ends the
        // running one. 'directory' is the shell's directory when it ran (empty: unknown). True when a run started.
        // 'terminal' is the one it was typed in (docs/panel-tabs.md §6): a run belongs to it - another terminal's
        // bytes and commands leave it alone, only its next build replaces it.
        bool OnCommand(const std::string &cmdLine, const std::string &directory, const std::string &projectRoot,
                       const void *terminal = nullptr);
        // The pty's bytes; nothing while no run is running, or from another terminal than the run's
        void Feed(std::string_view bytes, const void *terminal = nullptr);
        // The terminal's foreground job, after a read: once a job was seen, its end ends the run. True when it did.
        bool UpdateJobState(bool isJobRunning, const void *terminal = nullptr);
        // The shell says the command finished (OSC 133;D): the end of this terminal's run, also when the job was too fast
        // for UpdateJobState to ever see it running. True when it ended a run.
        bool OnCommandDone(const void *terminal = nullptr);
        // The run's terminal was quiet for a while (no shell integration to say the command ended): a job running now is
        // seen; none - the command ended, also when it was too fast for any read to see it run. True when it ended the run.
        bool OnQuiet(bool isJobRunning, const void *terminal = nullptr);
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
        // Counts the runs started - a view sees a new run by it changing
        uint64_t RunNumber() const;
        // The message F8 / a pick is on (an index into Messages()); none before the first step of a run
        std::optional<size_t> Current() const;
        // 'error    a.cpp:3:16  use of undeclared identifier' - a row of the list (the modal, the Messages tab)
        static std::string RowText(const BuildMessage &message);
        // The code a row points at goes on the line under it, from here - below the place, past 'error    '
        static constexpr int CODE_INDENT = 11;

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
        // The lines (from 1) of the file at 'canonicalPath' that have an error or a warning - the gutter's marks; an
        // error wins on a line. Each message's path is resolved once per run.
        std::map<uint32_t, BuildMessage::kSeverity> LineMarks(const std::string &canonicalPath);

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
        uint64_t runNumber = 0;
        bool isRunning = false;
        bool wasJobSeen = false;
        std::string command;
        std::string directory;
        std::string buildDirectory;
        std::string projectRoot;
        const void *runTerminal = nullptr;
        // Index into the parser's messages; none before the first step
        std::optional<size_t> current;
        // ResolvePath of the messages so far, by index - for LineMarks, which is asked on every draw of the gutter
        std::vector<std::string> resolvedPaths;
    };
}

#endif //GEDIT_BUILDMESSAGES_H
