//
// GitStatus - 'git rev-parse --show-toplevel --absolute-git-dir', then 'git status --porcelain=v1 -z', both through Process.
//

#include <future>
#include <memory>

#include "Core/Process.h"
#include "GitStatus.h"

using namespace gedit;

namespace {
    std::string TrimEnd(std::string text) {
        while (!text.empty() && ((text.back() == '\n') || (text.back() == '\r'))) {
            text.pop_back();
        }
        return text;
    }

    bool IsRenameOrCopy(char code) {
        return (code == 'R') || (code == 'C');
    }
}

void GitStatus::Query(const std::filesystem::path &root, DoneDelegate onDone) {
    auto revParseOut = std::make_shared<std::string>();
    auto revParse = Process::Create("git", {"-C", root.string(), "rev-parse", "--show-toplevel", "--absolute-git-dir"});
    revParse->SetOnStdOut([revParseOut](const std::string &data) {
        *revParseOut += data;
    });
    revParse->SetOnExit([root, revParseOut, onDone](const Process::Result &result) {
        Snapshot snapshot;
        if (result.state == Process::kState::kFailedToStart) {
            snapshot.error = result.error;
            onDone(snapshot);
            return;
        }
        if (!result.IsSuccess()) {
            snapshot.error = "not in a git repository";
            onDone(snapshot);
            return;
        }
        // One line each, in the order asked
        auto lines = TrimEnd(*revParseOut);
        auto idxNewLine = lines.find('\n');
        snapshot.isRepository = true;
        snapshot.topLevel = TrimEnd(lines.substr(0, idxNewLine));
        if (idxNewLine != std::string::npos) {
            snapshot.gitDir = TrimEnd(lines.substr(idxNewLine + 1));
        }

        auto statusOut = std::make_shared<std::string>();
        auto statusErr = std::make_shared<std::string>();
        auto status = Process::Create("git", {"--no-optional-locks", "-C", root.string(), "status", "--porcelain=v1", "-z",
                                              "--untracked-files=normal", "--", "."});
        status->SetOnStdOut([statusOut](const std::string &data) {
            *statusOut += data;
        });
        status->SetOnStdErr([statusErr](const std::string &data) {
            *statusErr += data;
        });
        status->SetOnExit([snapshot, statusOut, statusErr, onDone](const Process::Result &result) mutable {
            if (result.IsSuccess()) {
                snapshot.changes = Parse(*statusOut);
            } else if (result.state == Process::kState::kFailedToStart) {
                snapshot.error = result.error;
            } else {
                snapshot.error = statusErr->empty() ? "git status failed" : TrimEnd(*statusErr);
            }
            onDone(snapshot);
        });
        status->Start();
    });
    revParse->Start();
}

GitStatus::Snapshot GitStatus::QueryAndWait(const std::filesystem::path &root) {
    auto promise = std::make_shared<std::promise<Snapshot>>();
    auto future = promise->get_future();
    Query(root, [promise](const Snapshot &snapshot) {
        promise->set_value(snapshot);
    });
    return future.get();
}

std::vector<GitStatus::Change> GitStatus::Parse(std::string_view porcelain) {
    std::vector<Change> changes;
    auto nextField = [&porcelain](size_t &idxPos) -> std::string_view {
        auto idxEnd = porcelain.find('\0', idxPos);
        if (idxEnd == std::string_view::npos) {
            idxEnd = porcelain.size();
        }
        auto field = porcelain.substr(idxPos, idxEnd - idxPos);
        idxPos = idxEnd + 1;
        return field;
    };

    size_t idxPos = 0;
    while (idxPos < porcelain.size()) {
        // 'XY path', and for a rename or copy the old path follows as a field of its own
        auto entry = nextField(idxPos);
        if ((entry.size() < 4) || (entry[2] != ' ')) {
            continue;
        }
        Change change;
        change.index = entry[0];
        change.workTree = entry[1];
        change.path = std::string(entry.substr(3));
        if (IsRenameOrCopy(change.index) || IsRenameOrCopy(change.workTree)) {
            change.fromPath = std::string(nextField(idxPos));
        }
        change.change = ChangeOf(change.index, change.workTree);
        if (change.change == kChange::kNone) {
            continue;
        }
        changes.push_back(std::move(change));
    }
    return changes;
}

GitStatus::kChange GitStatus::ChangeOf(char index, char workTree) {
    if ((index == '?') && (workTree == '?')) {
        return kChange::kUntracked;
    }
    if (index == '!') {
        return kChange::kNone;
    }
    // Unmerged: either side 'U', or both added / both deleted
    if ((index == 'U') || (workTree == 'U') || ((index == 'A') && (workTree == 'A')) || ((index == 'D') && (workTree == 'D'))) {
        return kChange::kConflicted;
    }
    if ((index == 'D') || (workTree == 'D')) {
        return kChange::kDeleted;
    }
    if ((index == 'R') || (workTree == 'R')) {
        return kChange::kRenamed;
    }
    // A copy leaves the original where it was: only the new file is new
    if ((index == 'A') || (workTree == 'A') || (index == 'C') || (workTree == 'C')) {
        return kChange::kAdded;
    }
    if ((index == 'M') || (workTree == 'M') || (index == 'T') || (workTree == 'T')) {
        return kChange::kModified;
    }
    return kChange::kNone;
}
