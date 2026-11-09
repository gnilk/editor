//
// GitBase - 'git show HEAD:./<name>', and 'git status' of the file when that fails
//
#include <future>
#include <memory>

#include "Core/Process.h"
#include "GitBase.h"
#include "GitStatus.h"

using namespace gedit;
namespace fs = std::filesystem;

namespace {
    // A command's output, and how it ended, to 'onDone' on its thread
    void RunGit(const std::vector<std::string> &arguments, std::function<void(bool isSuccess, const std::string &out)> onDone) {
        auto out = std::make_shared<std::string>();
        auto process = Process::Create("git", arguments);
        process->SetOnStdOut([out](const std::string &data) {
            *out += data;
        });
        process->SetOnExit([out, onDone](const Process::Result &result) {
            onDone(result.IsSuccess(), *out);
        });
        process->Start();
    }

    // Binary - not lines to compare
    GitBase::Result BaseFrom(const std::string &text) {
        GitBase::Result result;
        if (text.find('\0') != std::string::npos) {
            result.error = "binary";
            return result;
        }
        result.kind = GitBase::kKind::kBase;
        result.text = text;
        return result;
    }

    void ShowAt(const std::string &dir, const std::string &object, GitBase::DoneDelegate onDone) {
        RunGit({"--no-optional-locks", "-C", dir, "show", object}, [onDone](bool isSuccess, const std::string &out) {
            if (!isSuccess) {
                GitBase::Result result;
                result.error = "not in the last commit";
                onDone(result);
                return;
            }
            onDone(BaseFrom(out));
        });
    }
}

void GitBase::Query(const fs::path &file, DoneDelegate onDone) {
    auto dir = file.parent_path().string();
    auto name = file.filename().string();
    RunGit({"--no-optional-locks", "-C", dir, "show", "HEAD:./" + name}, [dir, name, onDone](bool isSuccess, const std::string &out) {
        if (isSuccess) {
            onDone(BaseFrom(out));
            return;
        }
        // Not in the commit under this name: why
        RunGit({"--no-optional-locks", "-C", dir, "status", "--porcelain=v1", "-z", "--untracked-files=all", "--", name},
               [dir, onDone](bool isSuccess, const std::string &out) {
            Result result;
            if (!isSuccess) {
                result.error = "not in a git repository";
                onDone(result);
                return;
            }
            auto changes = GitStatus::Parse(out);
            if (changes.empty()) {
                result.error = "ignored";
                onDone(result);
                return;
            }
            auto &change = changes.front();
            if (change.change == GitStatus::kChange::kRenamed) {
                // The old name, from the top level - as 'HEAD:<path>' takes it
                ShowAt(dir, "HEAD:" + change.fromPath, onDone);
                return;
            }
            if ((change.change == GitStatus::kChange::kUntracked) || (change.change == GitStatus::kChange::kAdded)) {
                result.kind = kKind::kAllAdded;
            }
            onDone(result);
        });
    });
}

GitBase::Result GitBase::QueryAndWait(const fs::path &file) {
    auto promise = std::make_shared<std::promise<Result>>();
    auto future = promise->get_future();
    Query(file, [promise](const Result &result) {
        promise->set_value(result);
    });
    return future.get();
}
