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

static char32_t KindMarker(kSymbolKind kind) {
    switch (kind) {
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
            return U'f';
        case kSymbolKind::kMethod :
            return U'm';
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

std::vector<std::u32string> SymbolSearchModal::OnQuery(const std::u32string &query) {
    CodeIndex::QueryContext context;
    context.activePath = activePath;
    hits = index->Query(query, context);

    std::vector<std::u32string> rows;
    rows.reserve(hits.size());
    for (auto &hit : hits) {
        std::u32string row(1, KindMarker(hit.kind));
        row += U' ';
        row += hit.qualifiedName;
        rows.push_back(std::move(row));
    }
    return rows;
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

void SymbolSearchModal::OnCommit(int idxRow) {
    auto hit = hits[(size_t)idxRow];
    auto document = Editor::Instance().LoadDocument(hit.path);
    if (document == nullptr) {
        return;
    }
    Editor::Instance().SetActiveDocument(document);
    document->CancelSelection();
    document->SetCursorPosition(hit.line, hit.column);
    document->ScrollActiveLineIntoLowerPart();
    if (RuntimeConfig::Instance().HasRootView()) {
        ActionHelper::SwitchToNamedView(glbEditorView);
    }
}

void SymbolSearchModal::OnCancel() {
    RestoreActiveDocument();
}

void SymbolSearchModal::RestoreActiveDocument() {
    if (activeDocument != nullptr) {
        activeDocument->RestoreCursorAndViewTop(idxStartLine, idxStartChar, startViewTopLine);
    }
}
