//
// QuickOpenModal - every file of the project roots, filtered by name
//
#include "QuickOpenModal.h"

#include <algorithm>
#include <filesystem>

#include "Core/ActionHelper.h"
#include "Core/Editor.h"
#include "Core/Index/CodeIndex.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;
namespace fs = std::filesystem;

QuickOpenModal::QuickOpenModal(FileFinder::Ref finder, std::string activePath, std::unordered_set<std::string> openPaths) :
    QueryListModal("Open file", U"File: "), finder(std::move(finder)), activePath(std::move(activePath)),
    openPaths(std::move(openPaths)) {
    TakeSnapshot();
}

QuickOpenModal::Ptr QuickOpenModal::Create() {
    auto &editor = Editor::Instance();
    std::unordered_set<std::string> openPaths;
    for (auto &document : editor.GetWorkspace()->GetOpenDocuments()) {
        auto node = editor.GetWorkspace()->GetNodeFromDocument(document);
        if (node != nullptr) {
            openPaths.insert(pathutil::CanonicalPath(node->GetNodePath()).string());
        }
    }
    return std::make_unique<QuickOpenModal>(editor.GetFileFinder(), ActionHelper::ActivePath(), std::move(openPaths));
}

std::vector<QuickOpenModal::Entry> QuickOpenModal::EntriesOf(const FileFinder::Snapshot &snapshot, const std::string &activePath,
                                                             const std::unordered_set<std::string> &openPaths) {
    auto activeFolder = fs::path(activePath).parent_path();
    std::vector<Entry> entries;
    entries.reserve(snapshot.files.size());
    for (auto &file : snapshot.files) {
        fs::path path(file.path);
        auto &root = snapshot.roots[file.idxRoot].path;
        Entry entry;
        entry.path = file.path;
        entry.name = UnicodeHelper::utf8to32(path.filename().string());
        entry.lowerName = CodeIndex::LowerCase(entry.name);
        entry.relative = UnicodeHelper::utf8to32(path.lexically_relative(root).generic_string());
        entry.lowerRelative = CodeIndex::LowerCase(entry.relative);
        entry.folder = UnicodeHelper::utf8to32(path.parent_path().lexically_relative(root).generic_string());
        if (entry.folder == U".") {
            entry.folder.clear();
        }
        entry.isOpen = openPaths.contains(file.path);
        entry.isNearActive = !activePath.empty() && (path.parent_path() == activeFolder);
        entries.push_back(std::move(entry));
    }
    return entries;
}

std::vector<size_t> QuickOpenModal::Filter(const std::vector<Entry> &entries, const std::u32string &query, size_t maxRows) {
    std::vector<size_t> matches;
    std::vector<CodeIndex::kMatchQuality> qualities(entries.size(), CodeIndex::kMatchQuality::kNone);
    auto lowerQuery = CodeIndex::LowerCase(query);
    bool isPathQuery = (query.find(U'/') != std::u32string::npos);
    for (size_t i = 0; i < entries.size(); i++) {
        if (query.empty()) {
            matches.push_back(i);
            continue;
        }
        auto &entry = entries[i];
        qualities[i] = isPathQuery ? CodeIndex::MatchName(lowerQuery, entry.relative, entry.lowerRelative)
                                   : CodeIndex::MatchName(lowerQuery, entry.name, entry.lowerName);
        if (qualities[i] != CodeIndex::kMatchQuality::kNone) {
            matches.push_back(i);
        }
    }
    auto isBefore = [&entries, &qualities](size_t a, size_t b) {
        if (qualities[a] != qualities[b]) {
            return qualities[a] > qualities[b];
        }
        auto &entryA = entries[a];
        auto &entryB = entries[b];
        if (entryA.isOpen != entryB.isOpen) {
            return entryA.isOpen;
        }
        if (entryA.isNearActive != entryB.isNearActive) {
            return entryA.isNearActive;
        }
        if (entryA.relative.size() != entryB.relative.size()) {
            return entryA.relative.size() < entryB.relative.size();
        }
        return entryA.relative < entryB.relative;
    };
    // Only the rows shown need to be in order
    auto numRows = std::min(maxRows, matches.size());
    std::partial_sort(matches.begin(), matches.begin() + (std::ptrdiff_t)numRows, matches.end(), isBefore);
    matches.resize(numRows);
    return matches;
}

std::vector<std::u32string> QuickOpenModal::OnQuery(const std::u32string &query) {
    rowEntries = Filter(entries, query, kMaxRows);
    std::vector<std::u32string> rows;
    rows.reserve(rowEntries.size());
    for (auto idxEntry : rowEntries) {
        rows.push_back(entries[idxEntry].name);
    }
    return rows;
}

std::u32string QuickOpenModal::OnRowDetail(int idxRow) {
    return entries[rowEntries[(size_t)idxRow]].folder;
}

void QuickOpenModal::OnCommit(int idxRow) {
    ActionHelper::OpenFile(entries[rowEntries[(size_t)idxRow]].path);
}

// A walk that finished since the list was taken: the new list, the query run again on it
void QuickOpenModal::DrawViewContents() {
    if ((finder != nullptr) && (finder->GetSnapshot()->generation != generation)) {
        TakeSnapshot();
        Requery();
    }
    QueryListModal::DrawViewContents();
}

void QuickOpenModal::TakeSnapshot() {
    if (finder == nullptr) {
        return;
    }
    auto snapshot = finder->GetSnapshot();
    entries = EntriesOf(*snapshot, activePath, openPaths);
    generation = snapshot->generation;
}
