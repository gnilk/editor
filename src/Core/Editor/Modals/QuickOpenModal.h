//
// Open a file by part of its name, from anywhere (docs/usability-round.md §3 b) - Cmd+P / Ctrl+P. Every file of the
// project roots (FileFinder - what the roots' rules leave out left out), ranked as Go to symbol ranks names
// (CodeIndex::MatchName: prefix, word starts, subsequence); a query with a '/' is matched against the path below the
// root instead. Ties: the open files first, then the active file's folder, then the shorter path.
//
// The list is the last walk's; Cmd+P starts a new walk, and its result replaces the list when it is in.
//
#ifndef GOATEDIT_QUICKOPENMODAL_H
#define GOATEDIT_QUICKOPENMODAL_H

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "Core/FileSystem/FileFinder.h"
#include "Core/UI/Modals/QueryListModal.h"

namespace gedit {
    class QuickOpenModal : public QueryListModal {
    public:
        using Ptr = std::unique_ptr<QuickOpenModal>;

        // A file as listed, its lower-case forms made once
        struct Entry {
            std::string path;
            std::u32string name;
            std::u32string lowerName;
            std::u32string relative;        // below its root: 'src/Core/Document.cpp'
            std::u32string lowerRelative;
            std::u32string folder;          // 'src/Core'
            bool isOpen = false;
            bool isNearActive = false;      // in the active file's folder
        };

        static constexpr size_t kMaxRows = 500;

    public:
        QuickOpenModal(FileFinder::Ref finder, std::string activePath, std::unordered_set<std::string> openPaths);
        virtual ~QuickOpenModal() = default;

        // The editor's finder, the active file, the open ones
        static Ptr Create();

        static std::vector<Entry> EntriesOf(const FileFinder::Snapshot &snapshot, const std::string &activePath,
                                            const std::unordered_set<std::string> &openPaths);
        // Which entries match the query, best first, at most 'maxRows'. Empty query: all of them, ties' order.
        static std::vector<size_t> Filter(const std::vector<Entry> &entries, const std::u32string &query, size_t maxRows);

    protected:
        std::vector<std::u32string> OnQuery(const std::u32string &query) override;
        std::u32string OnRowDetail(int idxRow) override;
        void OnCommit(int idxRow) override;
        void DrawViewContents() override;

    private:
        void TakeSnapshot();

    private:
        FileFinder::Ref finder;
        std::string activePath;
        std::unordered_set<std::string> openPaths;
        uint64_t generation = 0;
        std::vector<Entry> entries;
        std::vector<size_t> rowEntries;     // the entry behind each row of the last query
    };
}

#endif //GOATEDIT_QUICKOPENMODAL_H
