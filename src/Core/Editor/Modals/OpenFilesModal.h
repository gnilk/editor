//
// The open files, to switch between (docs/usability-round.md §1) - Cmd+E / Ctrl+E, or a click on the header's '...'.
// In the header's order, the active one selected; typing filters by name, best match first. Each row carries the
// header's markers in front of the name and the file's folder, relative to its root, on the right.
//

#ifndef GOATEDIT_OPENFILESMODAL_H
#define GOATEDIT_OPENFILESMODAL_H

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "Core/Document.h"
#include "Core/UI/Modals/QueryListModal.h"

namespace gedit {
    class OpenFilesModal : public QueryListModal {
    public:
        using Ptr = std::unique_ptr<OpenFilesModal>;

        // One open file as listed: what it is called, its markers, where it lives
        struct Entry {
            Document::Ref document = nullptr;
            std::u32string name;
            std::u32string markers;     // '* ', 'R ', '[!] ', '[D] ' - as the header shows them
            std::u32string folder;      // relative to the root that holds it
        };
    public:
        OpenFilesModal(std::vector<Entry> entries, Document::Ref activeDocument);
        virtual ~OpenFilesModal() = default;

        // The workspace's open documents, as they are now
        static Ptr Create();

        // Which entries match the query, best first; ties in the given order. Empty query: all, in order.
        static std::vector<size_t> Filter(const std::vector<Entry> &entries, const std::u32string &query);
        // 'src/Core' for '<root>/src/Core/Workspace.cpp' when a root holds it, else the folder as it is
        static std::u32string FolderOf(const std::filesystem::path &path, const std::vector<std::filesystem::path> &roots);
    protected:
        std::vector<std::u32string> OnQuery(const std::u32string &query) override;
        std::u32string OnRowDetail(int idxRow) override;
        int OnInitialRow(const std::u32string &query) override;
        void OnCommit(int idxRow) override;
    private:
        std::vector<Entry> entries;
        Document::Ref activeDocument = nullptr;
        std::vector<size_t> rowEntries;     // the entry behind each row of the last query
    };
}

#endif //GOATEDIT_OPENFILESMODAL_H
