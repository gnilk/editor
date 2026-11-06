//
// Go to symbol - see SymbolSearchModal.h
//

#include "SymbolSearchModal.h"
#include "Core/ActionHelper.h"
#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

SymbolSearchModal::SymbolSearchModal(CodeIndex::Ref index, Document::Ref activeDocument, std::string activePath, std::vector<std::filesystem::path> roots) :
    QueryListModal("Go to symbol", U"Symbol: "), index(std::move(index)), activeDocument(std::move(activeDocument)), activePath(std::move(activePath)), roots(std::move(roots)) {
    if (this->activeDocument != nullptr) {
        auto &lineCursor = this->activeDocument->GetLineCursor();
        idxStartLine = lineCursor.idxActiveLine;
        idxStartChar = lineCursor.cursor.position.x;
        startViewTopLine = lineCursor.viewTopLine;
    }
}

SymbolSearchModal::Ptr SymbolSearchModal::CreateForHits(std::vector<CodeIndex::Hit> hits, Document::Ref activeDocument, std::string activePath, std::vector<std::filesystem::path> roots) {
    auto modal = Create(nullptr, std::move(activeDocument), std::move(activePath), std::move(roots));
    modal->fixedHits = std::move(hits);
    return modal;
}

std::vector<std::u32string> SymbolSearchModal::OnQuery(const std::u32string &query) {
    if (index != nullptr) {
        CodeIndex::QueryContext context;
        context.activePath = activePath;
        hits = index->Query(query, context);
    } else {
        hits.clear();
        auto lowerQuery = CodeIndex::LowerCase(query);
        std::copy_if(fixedHits.begin(), fixedHits.end(), std::back_inserter(hits), [&lowerQuery](const CodeIndex::Hit &hit) {
            return lowerQuery.empty() || (CodeIndex::LowerCase(hit.qualifiedName).find(lowerQuery) != std::u32string::npos);
        });
    }

    std::vector<std::u32string> rows;
    rows.reserve(hits.size());
    for (auto &hit : hits) {
        std::u32string row(1, KindMarker(hit));
        row += U' ';
        row += hit.qualifiedName;
        rows.push_back(std::move(row));
    }
    return rows;
}

char32_t SymbolSearchModal::KindMarker(const CodeIndex::Hit &hit) {
    switch (hit.kind) {
        case kSymbolKind::kNamespace :
            return U'n';
        case kSymbolKind::kClass :
            return U'c';
        case kSymbolKind::kStruct :
            return U's';
        case kSymbolKind::kUnion :
            return U'u';
        case kSymbolKind::kEnum :
            return U'e';
        case kSymbolKind::kEnumerator :
            return U'v';
        case kSymbolKind::kFunction :
            return hit.isDefinition ? U'F' : U'f';
        case kSymbolKind::kMethod :
            return hit.isDefinition ? U'M' : U'm';
        case kSymbolKind::kMember :
            return U'd';
        case kSymbolKind::kVariable :
            return U'g';
        case kSymbolKind::kAlias :
            return U't';
        case kSymbolKind::kMacro :
            return U'#';
    }
    return U' ';
}

std::u32string SymbolSearchModal::OnRowDetail(int idxRow) {
    if ((idxRow < 0) || ((size_t)idxRow >= hits.size())) {
        return {};
    }
    return UnicodeHelper::utf8to32(HitLocation(hits[(size_t)idxRow], roots));
}

std::string SymbolSearchModal::HitLocation(const CodeIndex::Hit &hit, const std::vector<std::filesystem::path> &roots) {
    std::filesystem::path path(hit.path);
    for (auto &root : roots) {
        auto relative = path.lexically_relative(root);
        if (!relative.empty() && (*relative.begin() != "..")) {
            path = relative;
            break;
        }
    }
    return path.string() + ":" + std::to_string(hit.line + 1);
}

// Only the active document: previewing another file would have to open it just to look
void SymbolSearchModal::OnPreview(int idxRow) {
    if (activeDocument == nullptr) {
        return;
    }
    if ((idxRow < 0) || (hits[(size_t)idxRow].path != activePath)) {
        RestoreActiveDocument();
        return;
    }
    auto &hit = hits[(size_t)idxRow];
    activeDocument->SetCursorPosition(hit.line, hit.column);
    activeDocument->ScrollActiveLineIntoLowerPart();
}

// Back first: a preview moved the cursor, and the jump starts from where the modal did
void SymbolSearchModal::OnCommit(int idxRow) {
    auto hit = hits[(size_t)idxRow];
    RestoreActiveDocument();
    ActionHelper::JumpTo(hit.path, hit.line, hit.column);
}

void SymbolSearchModal::OnCancel() {
    RestoreActiveDocument();
}

void SymbolSearchModal::RestoreActiveDocument() {
    if (activeDocument != nullptr) {
        activeDocument->RestoreCursorAndViewTop(idxStartLine, idxStartChar, startViewTopLine);
    }
}
