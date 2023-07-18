//
// Refactor this - controller should be the one modifying the textBuffer
// The model should only hold data which sits between the actual text-data and the viewer, like selection and search stuff
// We need a good way to define how the model and the controller interoperate as the controller either needs access to data in the model
// OR the model needs to talk to a modifier API in the controller
//
#include "Editor.h"
#include "EditorModel.h"
#include "EditorConfig.h"
#include "logger.h"
using namespace gedit;

EditorModel::Ref EditorModel::Create() {
    EditorModel::Ref editorModel = std::make_shared<EditorModel>();
    return editorModel;
}

// We should only handle stuff which might modify selection or similar...
bool EditorModel::HandleKeyPress(const gedit::KeyPress &keyPress) {
    bool wasHandled = false;
    auto line = textBuffer->LineAt(idxActiveLine);
    switch (keyPress.specialKey) {
        case Keyboard::kKeyCode_Backspace :
        case Keyboard::kKeyCode_DeleteForward :
            if (IsSelectionActive()) {
                DeleteSelection();
                CancelSelection();
                wasHandled = true;
            }
            break;
        case Keyboard::kKeyCode_Tab :
            // FIXME: Handle selection..
            break;
        default:
            break;
    }

    // Needed???
    if (wasHandled) {
        textBuffer->Reparse();
    }

    return wasHandled;
}


void EditorModel::DeleteSelection() {
    auto startPos = currentSelection.GetStart();
    auto endPos = currentSelection.GetEnd();

    editController->DeleteRange(startPos, endPos);

    idxActiveLine = startPos.y;
    cursor.position.x = startPos.x;
    cursor.position.y = startPos.y;

}

void EditorModel::CommentSelectionOrLine() {

    if (!textBuffer->HaveLanguage()) {
        return;
    }
    auto lineCommentPrefix = textBuffer->LangParser().GetLineComment();
    if (lineCommentPrefix.empty()) {
        return;
    }

    if (!IsSelectionActive()) {
        editController->AddLineComment(idxActiveLine, idxActiveLine+1, lineCommentPrefix);
        return;
    }

    auto start = currentSelection.GetStart();
    auto end = currentSelection.GetEnd();
    editController->AddLineComment(start.y, end.y, lineCommentPrefix);
}

size_t EditorModel::SearchFor(const std::string &searchItem) {
    searchResults.clear();

    auto nLines = textBuffer->NumLines();
    auto len = searchItem.length();
    for(size_t idxLine = 0; idxLine < nLines; idxLine++) {
        auto line = textBuffer->LineAt(idxLine);
        auto idxStart = line->Buffer().find(searchItem.c_str());
        if (idxStart == std::string_view::npos) {
            continue;
        }
        SearchResult result;
        result.idxLine = idxLine;
        result.cursor_x = idxStart;
        result.length = len;
        searchResults.push_back(result);
    }
    // Number of hits..
    return searchResults.size();
}

bool EditorModel::JumpToSearchHit(size_t idxHit) {
    if (idxHit >= searchResults.size()) {
        return false;
    }
    auto &result = searchResults[idxHit];
    cursor.position.y = result.idxLine;
    cursor.position.x = result.cursor_x;
    idxActiveLine = result.idxLine;
    return true;
}


void EditorModel::ClearSearchResults() {
    searchResults.clear();
}

void EditorModel::NextSearchResult() {
    idxActiveSearchHit++;
    if (!JumpToSearchHit(idxActiveSearchHit) && (idxActiveSearchHit > 0)) {
        idxActiveSearchHit -=1;
    }

}
void EditorModel::PrevSearchResult() {
    if (idxActiveSearchHit > 0) {
        idxActiveSearchHit -= 1;
    }
    JumpToSearchHit(idxActiveSearchHit);

}

void EditorModel::ResetSearchHitIndex() {
    idxActiveSearchHit = 0;
}
size_t EditorModel::GetSearchHitIndex() {
    return idxActiveSearchHit;
}

size_t EditorModel::GetReparseStartLineIndex() {
    if (!IsSelectionActive()) {
        return idxActiveLine;
    }
    return currentSelection.GetStart().y;
}
