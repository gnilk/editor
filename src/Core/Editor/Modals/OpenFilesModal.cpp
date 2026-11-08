//
// OpenFilesModal - the open files, filtered by name
//

#include <algorithm>
#include <system_error>

#include "OpenFilesModal.h"
#include "Core/ActionHelper.h"
#include "Core/Editor.h"
#include "Core/Index/CodeIndex.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;
namespace fs = std::filesystem;

OpenFilesModal::OpenFilesModal(std::vector<Entry> entries, Document::Ref activeDocument) :
    QueryListModal("Open files", U"File: "), entries(std::move(entries)), activeDocument(std::move(activeDocument)) {
}

OpenFilesModal::Ptr OpenFilesModal::Create() {
    auto &editor = Editor::Instance();
    auto workspace = editor.GetWorkspace();
    std::vector<Entry> entries;
    for (auto &document : workspace->GetOpenDocuments()) {
        auto node = workspace->GetNodeFromDocument(document);
        if (node == nullptr) {
            continue;
        }
        Entry entry;
        entry.document = document;
        entry.name = UnicodeHelper::utf8to32(node->GetDisplayName());
        entry.markers = UnicodeHelper::utf8to32(workspace->StatusMarkers(document));
        entry.folder = FolderOf(document->GetPath(), editor.GetIndexedRoots());
        entries.push_back(std::move(entry));
    }
    return std::make_unique<OpenFilesModal>(std::move(entries), workspace->GetActiveDocument());
}

std::vector<size_t> OpenFilesModal::Filter(const std::vector<Entry> &entries, const std::u32string &query) {
    std::vector<size_t> matches;
    std::vector<CodeIndex::kMatchQuality> qualities(entries.size(), CodeIndex::kMatchQuality::kNone);
    auto lowerQuery = CodeIndex::LowerCase(query);
    for (size_t i = 0; i < entries.size(); i++) {
        if (query.empty()) {
            matches.push_back(i);
            continue;
        }
        auto lowerName = CodeIndex::LowerCase(entries[i].name);
        qualities[i] = CodeIndex::MatchName(lowerQuery, entries[i].name, lowerName);
        if (qualities[i] != CodeIndex::kMatchQuality::kNone) {
            matches.push_back(i);
        }
    }
    std::stable_sort(matches.begin(), matches.end(), [&qualities](size_t a, size_t b) {
        return qualities[a] > qualities[b];
    });
    return matches;
}

std::u32string OpenFilesModal::FolderOf(const fs::path &path, const std::vector<fs::path> &roots) {
    std::error_code error;
    auto folder = fs::weakly_canonical(path, error).parent_path();
    if (error) {
        folder = path.parent_path();
    }
    for (auto &root : roots) {
        auto relative = folder.lexically_relative(root);
        if (!relative.empty() && (*relative.begin() != "..")) {
            return (relative == ".") ? std::u32string{} : UnicodeHelper::utf8to32(relative.string());
        }
    }
    return UnicodeHelper::utf8to32(folder.string());
}

std::vector<std::u32string> OpenFilesModal::OnQuery(const std::u32string &query) {
    rowEntries = Filter(entries, query);
    std::vector<std::u32string> rows;
    for (auto idxEntry : rowEntries) {
        rows.push_back(entries[idxEntry].markers + entries[idxEntry].name);
    }
    return rows;
}

std::u32string OpenFilesModal::OnRowDetail(int idxRow) {
    return entries[rowEntries[(size_t)idxRow]].folder;
}

// Nothing typed: where you are. Something typed: the best match.
int OpenFilesModal::OnInitialRow(const std::u32string &query) {
    if (!query.empty()) {
        return 0;
    }
    for (size_t i = 0; i < rowEntries.size(); i++) {
        if (entries[rowEntries[i]].document == activeDocument) {
            return (int)i;
        }
    }
    return 0;
}

void OpenFilesModal::OnCommit(int idxRow) {
    auto document = entries[rowEntries[(size_t)idxRow]].document;
    Editor::Instance().SetActiveDocument(document);
    // From the terminal or the tree too: you asked for the file, so you get its editor
    ActionHelper::SwitchToNamedView(glbEditorView);
}
