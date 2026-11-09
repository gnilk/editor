//
// The file as the last commit has it, for the change marks in the gutter (docs/usability-round.md §0c). Plain: the
// text as git prints it, or why there is none; splitting and comparing are the caller's. Built on Process.
//
// 'git show HEAD:./<name>' from the file's folder. When the commit doesn't have it, 'git status' of that file says
// why: new (untracked, or added and staged) - every line is added; renamed - the base is the old name's text;
// anything else (ignored, not in a repository, no git) - no marks.
//
#ifndef GOATEDIT_GITBASE_H
#define GOATEDIT_GITBASE_H

#include <filesystem>
#include <functional>
#include <string>

namespace gedit {
    class GitBase {
    public:
        enum class kKind {
            kNone,          // no marks
            kBase,          // 'text' is the file in the last commit
            kAllAdded,      // new since the last commit
        };

        struct Result {
            kKind kind = kKind::kNone;
            std::string text;
            std::string error;
        };

        using DoneDelegate = std::function<void(const Result &result)>;
    public:
        // In the background; 'onDone' on git's thread - the caller posts to the main thread
        static void Query(const std::filesystem::path &file, DoneDelegate onDone);
        // The same, waiting - for tests
        static Result QueryAndWait(const std::filesystem::path &file);
    };
}

#endif //GOATEDIT_GITBASE_H
