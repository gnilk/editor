//
// Refactor this - controller should be the one modifying the textBuffer
// The model should only hold data which sits between the actual text-data and the viewer, like selection and search stuff
// We need a good way to define how the model and the controller interoperate as the controller either needs access to data in the model
// OR the model needs to talk to a modifier API in the controller
//
#include <chrono>
#include "Editor.h"
#include "EditorModel.h"
#include "logger.h"
using namespace gedit;

EditorModel::Ref EditorModel::Create() {
    EditorModel::Ref editorModel = std::make_shared<EditorModel>();
    return editorModel;
}

void EditorModel::DeleteSelection() {
    auto startPos = currentSelection.GetStart();
    auto endPos = currentSelection.GetEnd();

    editController->DeleteRange(startPos, endPos);

}

void EditorModel::CommentSelectionOrLine() {

    if (!textBuffer->HaveLanguage()) {
        return;
    }
    auto lineCommentPrefix = textBuffer->GetLanguage().GetLineComment();
    if (lineCommentPrefix.empty()) {
        return;
    }

    if (!IsSelectionActive()) {
        editController->AddLineComment(lineCursor.idxActiveLine, lineCursor.idxActiveLine+1, lineCommentPrefix);
        return;
    }

    auto start = currentSelection.GetStart();
    auto end = currentSelection.GetEnd();
    editController->AddLineComment(start.y, end.y, lineCommentPrefix);
}

void EditorModel::IndentSelectionOrLine() {
    if (!textBuffer->HaveLanguage()) {
        return;
    }

    if (!IsSelectionActive()) {
        editController->IndentLines(lineCursor.idxActiveLine, lineCursor.idxActiveLine + 1);
        return;
    }
    auto start = currentSelection.GetStart();
    auto end = currentSelection.GetEnd();
    editController->IndentLines(start.y, end.y);
}

void EditorModel::UnindentSelectionOrLine() {
    if (!textBuffer->HaveLanguage()) {
        return;
    }

    if (!IsSelectionActive()) {
        editController->UnindentLines(lineCursor.idxActiveLine, lineCursor.idxActiveLine + 1);
        return;
    }
    auto start = currentSelection.GetStart();
    auto end = currentSelection.GetEnd();
    editController->UnindentLines(start.y, end.y);

}



// This is a little naive and I should probably spin it of to a specific thread
size_t EditorModel::SearchFor(const std::u32string &searchItem) {

    auto tStart = std::chrono::steady_clock::now();

    searchResults.clear();
    auto nLines = textBuffer->NumLines();
    auto len = searchItem.length();
    for(size_t idxLine = 0; idxLine < nLines; idxLine++) {
        // This is a cut-off time for searching, we don't want to stall
        // Currently my Macbook M1 use 170ms to search through 105MB of data
        auto tDuration = std::chrono::steady_clock::now() - tStart;
        auto msDuration = std::chrono::duration_cast<std::chrono::milliseconds>(tDuration).count();
        if (msDuration > 1000) {
            auto logger = gnilk::Logger::GetLogger("EditModel");
            logger->Debug("Search aborted at line: %zu, exceeding run-time!", idxLine);
            break;
        }



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

    auto tDuration = std::chrono::steady_clock::now() - tStart;
    auto msDuration = std::chrono::duration_cast<std::chrono::milliseconds>(tDuration).count();
    auto logger = gnilk::Logger::GetLogger("EditModel");
    logger->Debug("Search took %zu milliseconds", msDuration);

    // Number of hits..
    return searchResults.size();
}

bool EditorModel::JumpToSearchHit(size_t idxHit) {
    if (idxHit >= searchResults.size()) {
        return false;
    }
    auto &result = searchResults[idxHit];
    GetCursor().position.y = result.idxLine;
    GetCursor().position.x = result.cursor_x;
    GetCursor().wantedColumn = result.cursor_x;
    lineCursor.idxActiveLine = result.idxLine;

    RefocusViewArea();
    return true;
}

// Call this function to re-center the view area around the active line...
// the active line (line in focus) is positioned 1/3 (of num-lines) down from top
void EditorModel::RefocusViewArea() {
    if (!lineCursor.IsInside(lineCursor.idxActiveLine)) {

        auto height = lineCursor.Height();
        int margin = height / 3;

        lineCursor.viewTopLine = lineCursor.idxActiveLine - margin;
        if (lineCursor.viewTopLine < 0) {
            lineCursor.viewTopLine = 0;
        }
        lineCursor.viewBottomLine = lineCursor.viewTopLine + height;
    }
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

bool EditorModel::LoadData(const std::filesystem::path &pathName) {
    auto logger = gnilk::Logger::GetLogger("EditorModel");
    logger->Debug("LoadData, start: %s", pathName.c_str());
    if (!textBuffer->Load(pathName)) {
        return false;
    }
    logger->Debug("LoadData, ok, file loaded");
    auto lang = Editor::Instance().GetLanguageForExtension(pathName.extension());
    if (lang != nullptr) {
        logger->Debug("LoadData, setting language: %s", UnicodeHelper::utf32toascii(lang->Identifier()).c_str());
        textBuffer->SetLanguage(lang);
        logger->Debug("LoadData, done");
    }
    return true;
}

bool EditorModel::SaveData(const std::filesystem::path &pathName) {
    return textBuffer->Save(pathName);
}
bool EditorModel::SaveDataNoChangeCheck(const std::filesystem::path &pathName) {
    return textBuffer->SaveForce(pathName);
}
