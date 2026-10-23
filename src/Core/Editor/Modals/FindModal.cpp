//
// Find in the active document - see FindModal.h
//

#include "FindModal.h"
#include "Core/ActionHelper.h"
#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"

using namespace gedit;

FindModal::FindModal(Document::Ref document) : QueryListModal("Find", U"Find: "), document(document) {
    auto &lineCursor = document->GetLineCursor();
    idxStartLine = lineCursor.idxActiveLine;
    idxStartChar = lineCursor.cursor.position.x;
    startViewTopLine = lineCursor.viewTopLine;
}

std::u32string FindModal::SelectedText(Document::Ref document) {
    if ((document == nullptr) || !document->IsSelectionActive()) {
        return {};
    }
    auto &selection = document->GetSelection();
    auto start = selection.GetStart();
    auto end = selection.GetEnd();
    if ((start.y != end.y) || (end.x <= start.x) || (start.y < 0) || ((size_t)start.y >= document->Lines().size())) {
        return {};
    }
    auto &text = document->LineAt(start.y)->Buffer();
    if ((size_t)start.x >= text.size()) {
        return {};
    }
    return text.substr(start.x, end.x - start.x);
}

// Paste goes into the field - the clipboard is an editor service, so the toolkit modal doesn't see it
bool FindModal::OnAction(const EditorAction &kpAction) {
    if (kpAction.action != kAction::kActionPasteFromClipboard) {
        return QueryListModal::OnAction(kpAction);
    }
    // Pick up what another application put on the OS pasteboard (as Document::PasteFromClipboard)
    auto screen = RuntimeConfig::Instance().GetScreen();
    if (screen != nullptr) {
        screen->UpdateClipboardData();
    }
    auto item = Editor::Instance().GetClipBoard().Top();
    if (item != nullptr) {
        InsertText(item->AsText());
    }
    return true;
}

std::vector<std::u32string> FindModal::OnQuery(const std::u32string &query) {
    document->ClearSearchResults();
    if (query.empty()) {
        return {};
    }
    document->SearchFor(query, false);

    // '  12: text' - the numbers right-aligned, the text without its indent and with tabs as spaces
    auto &hits = document->GetSearchResults();
    std::vector<std::u32string> rows;
    if (hits.empty()) {
        return rows;
    }
    auto numberWidth = std::to_string(hits.back().idxLine + 1).size();
    for (auto &hit : hits) {
        auto number = std::to_string(hit.idxLine + 1);
        std::u32string row(numberWidth - number.size(), U' ');
        row.append(number.begin(), number.end());
        row += U": ";
        auto &text = document->LineAt(hit.idxLine)->Buffer();
        auto idxFirst = text.find_first_not_of(U" \t");
        if (idxFirst != std::u32string::npos) {
            for (auto ch : text.substr(idxFirst)) {
                row += (ch == U'\t') ? U' ' : ch;
            }
        }
        rows.push_back(row);
    }
    return rows;
}

void FindModal::OnPreview(int idxRow) {
    // No hits - back to where we started
    if (idxRow < 0) {
        Restore();
        return;
    }
    document->SetSearchHitIndex(idxRow);
    document->JumpToSearchHit(idxRow);
    document->ScrollActiveLineIntoLowerPart();
}

void FindModal::OnCommit(int idxRow) {
    // The results stay - Next/Prev continue from this hit
    document->CancelSelection();
    document->SetSearchHitIndex(idxRow);
    document->JumpToSearchHit(idxRow);
    document->ScrollActiveLineIntoLowerPart();
    if (RuntimeConfig::Instance().HasRootView()) {
        ActionHelper::SwitchToNamedView(glbEditorView);
    }
}

void FindModal::OnCancel() {
    document->ClearSearchResults();
    document->ResetSearchHitIndex();
    Restore();
}

void FindModal::Restore() {
    document->RestoreCursorAndViewTop(idxStartLine, idxStartChar, startViewTopLine);
}
