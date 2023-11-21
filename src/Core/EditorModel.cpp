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

// This is the global section in the config.yml for this view
static const std::string cfgSectionName = "editorview";


EditorModel::Ref EditorModel::Create(TextBuffer::Ref newTextBuffer) {
    EditorModel::Ref editorModel = std::make_shared<EditorModel>(newTextBuffer);
    editorModel->Begin();
    return editorModel;
}

void EditorModel::Begin() {
    logger = gnilk::Logger::GetLogRef("EditorModel");
    bUseCLionPageNav = Config::Instance()[cfgSectionName].GetBool("pgupdown_content_first", true);
    if (bUseCLionPageNav) {
        verticalNavigationViewModel = std::make_unique<VerticalNavigationCLion>();
    } else {
        verticalNavigationViewModel = std::make_unique<VerticalNavigationVSCode>();
    }
    verticalNavigationViewModel->lineCursor = GetLineCursorRef();

}

void EditorModel::OnViewInit(const Rect &rect) {
    viewRect = rect;

    verticalNavigationViewModel->HandleResize(rect);

    // Need support in controller to forward this to model...
    lineCursor.viewTopLine = 0;
    lineCursor.viewBottomLine = rect.Height();

    UpdateModelFromNavigation(true);

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

/////////
bool EditorModel::OnAction(const KeyPressAction &kpAction) {
    if (kpAction.actionModifier == kActionModifier::kActionModifierSelection) {
        if (!IsSelectionActive()) {
            logger->Debug("Shift pressed, selection inactive - BeginSelection");
            BeginSelection();
        }
    }

    bool result = false;

    // This is convoluted - will be dealt with when copy/paste works...
    if (kpAction.action == kAction::kActionCopyToClipboard) {
        logger->Debug("Set text to clipboard");
        auto selection = GetSelection();
        auto &clipboard = Editor::Instance().GetClipBoard();
        clipboard.CopyFromBuffer(GetTextBuffer(), selection.GetStart(), selection.GetEnd());

    } else if (kpAction.action == kAction::kActionCutToClipboard) {
        logger->Debug("Cut text to clipboard");
        auto selection = GetSelection();
        auto &clipboard = Editor::Instance().GetClipBoard();
        clipboard.CopyFromBuffer(GetTextBuffer(), selection.GetStart(), selection.GetEnd());

        lineCursor.idxActiveLine = selection.GetStart().y;
        lineCursor.cursor.position = selection.GetStart();
        lineCursor.cursor.position.y -= lineCursor.viewTopLine;   // Translate to screen coords..

        DeleteSelection();
        CancelSelection();
        UpdateModelFromNavigation(false);
    } else if (kpAction.action == kAction::kActionPasteFromClipboard) {
        PasteFromClipboard();
    } else if (kpAction.action == kAction::kActionInsertLineComment) {
        // Handle this here since we want to keep the selection...
        CommentSelectionOrLine();
    } else if (kpAction.action == kAction::kActionIndent && IsSelectionActive()) {
        IndentSelectionOrLine();
    } else if (kpAction.action == kAction::kActionUnindent && IsSelectionActive()) {
        UnindentSelectionOrLine();
    } else {
        result = DispatchAction(kpAction);
    }


    // We cancel selection here unless you have taken appropriate action..
    if ((kpAction.actionModifier != kActionModifier::kActionModifierSelection) && result && IsSelectionActive()) {
        CancelSelection();
    }

    // Update with cursor after navigation (if any happened)
    if (IsSelectionActive()) {
        UpdateSelection();
        logger->Debug(" Selection is Active, start=(%d:%d), end=(%d:%d)",
                      GetSelection().GetStart().x, GetSelection().GetStart().y,
                      GetSelection().GetEnd().x, GetSelection().GetEnd().y);
    }
    return result;
}


bool EditorModel::DispatchAction(const KeyPressAction &kpAction) {
    switch(kpAction.action) {
        case kAction::kActionLineLeft :
            return OnActionStepLeft();
        case kAction::kActionLineRight :
            return OnActionStepRight();
        case kAction::kActionPageUp :
            return OnActionPageUp();
        case kAction::kActionPageDown :
            return OnActionPageDown();
        case kAction::kActionLineDown :
            return OnActionLineDown(kpAction);
        case kAction::kActionLineUp :
            return OnActionLineUp();
        case kAction::kActionLineEnd :
            return OnActionLineEnd();
        case kAction::kActionLineHome :
            return OnActionLineHome();
        case kAction::kActionCommitLine :
            return OnActionCommitLine();
        case kAction::kActionIndent :
            return OnActionIndent();
        case kAction::kActionUnindent :
            return OnActionUnindent();
        case kAction::kActionBufferStart :
            [[fallthrough]];
        case kAction::kActionGotoFirstLine :
            return OnActionGotoFirstLine();
        case kAction::kActionBufferEnd :
            [[fallthrough]];
        case kAction::kActionGotoLastLine :
            return OnActionGotoLastLine();
        case kAction::kActionGotoTopLine :
            return OnActionGotoTopLine();
        case kAction::kActionGotoBottomLine :
            return OnActionGotoBottomLine();
        case kAction::kActionLineWordLeft :
            return OnActionWordLeft();
        case kAction::kActionLineWordRight :
            return OnActionWordRight();
        case kAction::kActionUndo :
            return OnActionUndo();
        case kAction::kActionNextSearchResult :
            return OnNextSearchResult();
        case kAction::kActionPrevSearchResult :
            return OnPrevSearchResult();
        default:
            break;
    }
    return false;
}
bool EditorModel::OnActionIndent() {
    auto undoItem = BeginUndoItem();
    AddTab();
    EndUndoItem(undoItem);
    UpdateSyntaxForActiveLineRegion();
    return true;
}
bool EditorModel::OnActionUnindent() {
    auto undoItem = BeginUndoItem();
    DelTab();
    EndUndoItem(undoItem);
    UpdateSyntaxForActiveLineRegion();
    return true;
}


//bool EditorView::OnActionBackspace() {
//    auto currentLine = editorModel->GetEditController()->LineAt(editorModel->idxActiveLine);
//    if (editorModel->cursor.position.x > 0) {
//        logger->Debug("OnActionBackspace");
//        std::string strMarker(editorModel->cursor.position.x-1,' ');
//        logger->Debug("  LineBefore: '%s'", currentLine->Buffer().data());
//        logger->Debug("               %s*", strMarker.c_str());
//        logger->Debug("  Delete at: %d", editorModel->cursor.position.x-1);
//        currentLine->Delete(editorModel->cursor.position.x-1);
//        logger->Debug("  LineAfter: '%s'", currentLine->Buffer().data());
//        editorModel->cursor.position.x--;
//        editorModel->GetEditController()->UpdateSyntaxForBuffer();
//    }
//    return true;
//}

// Move all actions to controller/model...
bool EditorModel::OnActionUndo() {
    //editorModel->GetTextBuffer()->Undo();
    auto &lineCursor = GetLineCursor();
    Undo(lineCursor.cursor, lineCursor.idxActiveLine);
    // auto nLinesAfter = GetTextBuffer()->NumLines();
    // //if ((nLinesAfter > lineCursor.viewBottomLine) && (lineCursor.Height() < nLinesAfter)
    // if (nLinesAfter > viewRect.Height()) {
    //     nLinesAfter = viewRect.Height();
    // }
    // lineCursor.viewBottomLine = lineCursor.viewTopLine + nLinesAfter;


    return true;
}

bool EditorModel::OnActionLineHome() {
    auto &lineCursor = GetLineCursor();
    lineCursor.cursor.position.x = 0;
    lineCursor.cursor.wantedColumn = 0;
    return true;
}

bool EditorModel::OnActionLineEnd() {
    auto &lineCursor = GetLineCursor();
    auto currentLine = LineAt(lineCursor.idxActiveLine);
    if (currentLine == nullptr) {
        return true;
    }
    auto endpos = currentLine->Length();
    lineCursor.cursor.position.x = endpos;
    lineCursor.cursor.wantedColumn = endpos;
    return true;
}


bool EditorModel::OnActionCommitLine() {
    auto &lineCursor = GetLineCursor();

    // Should newline be here
    logger->Debug("OnActionCommitLine, Before: idxActive=%zu", lineCursor.idxActiveLine);
    NewLine(lineCursor.idxActiveLine, lineCursor.cursor);

    // Need viewRect - this is the visible view of the renderer
    verticalNavigationViewModel->OnNavigateDown(1, viewRect, Lines().size());
    UpdateModelFromNavigation(true);
    logger->Debug("OnActionCommitLine, After: idxActive=%zu", lineCursor.idxActiveLine);

    //InvalidateView();
    return true;
}

bool EditorModel::OnActionWordRight() {
    auto currentLine = ActiveLine();
    auto &cursor = GetCursor();
    auto attrib = currentLine->AttributeAt(cursor.position.x);
    attrib++;
    cursor.position.x = attrib->idxOrigString;

    return true;
}

bool EditorModel::OnActionWordLeft() {
    auto currentLine = ActiveLine(); //editorModel->GetEditController()->LineAt(editorModel->idxActiveLine);
    auto &cursor = GetCursor();
    auto attrib = currentLine->AttributeAt(cursor.position.x);
    if (cursor.position.x == attrib->idxOrigString) {
        attrib--;
    }
    cursor.position.x = attrib->idxOrigString;
    return true;
}

bool EditorModel::OnActionGotoFirstLine() {
    logger->Debug("GotoFirstLine (def: CMD+Home), resetting cursor and view data!");
    auto &lineCursor = GetLineCursor();
    lineCursor.cursor.position.x = 0;
    lineCursor.cursor.position.y = 0;
    lineCursor.idxActiveLine = 0;
    lineCursor.viewTopLine = 0;
    // Need viewRect
    lineCursor.viewBottomLine = viewRect.Height();

    return true;
}
bool EditorModel::OnActionGotoLastLine() {
    logger->Debug("GotoLastLine (def: CMD+End), set cursor to last line!");
    auto &lineCursor = GetLineCursor();
    lineCursor.cursor.position.x = 0;
    lineCursor.cursor.position.y = viewRect.Height()-1;
    lineCursor.idxActiveLine = Lines().size()-1;
    lineCursor.viewBottomLine = Lines().size();
    lineCursor.viewTopLine = lineCursor.viewBottomLine - viewRect.Height();
    if (lineCursor.viewTopLine < 0) {
        lineCursor.viewTopLine = 0;
    }

    logger->Debug("Cursor: %d:%d, idxActiveLine: %d",lineCursor.cursor.position.x, lineCursor.cursor.position.y, lineCursor.idxActiveLine);

    return true;
}


bool EditorModel::OnActionStepLeft() {
    auto &cursor = GetCursor();
    cursor.position.x--;
    if (cursor.position.x < 0) {
        cursor.position.x = 0;
    }
    cursor.wantedColumn = cursor.position.x;
    return true;
}
bool EditorModel::OnActionStepRight() {
    auto currentLine = ActiveLine();
    auto &cursor = GetCursor();
    cursor.position.x++;
    if (cursor.position.x > (int)currentLine->Length()) {
        cursor.position.x = (int)currentLine->Length();
    }
    cursor.wantedColumn = cursor.position.x;
    return true;
}

bool EditorModel::OnNextSearchResult() {
    if (!HaveSearchResults()) {
        return false;
    }
    NextSearchResult();
    return true;
}
bool EditorModel::OnPrevSearchResult() {
    if (!HaveSearchResults()) {
        return false;
    }
    PrevSearchResult();
    return true;
}


// Not sure this should be here
void EditorModel::UpdateModelFromNavigation(bool updateCursor) {

    if (!updateCursor) {
        return;
    }

    auto currentLine = LineAt(lineCursor.idxActiveLine);
    if (currentLine == nullptr) {
        lineCursor.cursor.position.x = 0;
        lineCursor.cursor.position.y = 0;
        lineCursor.cursor.wantedColumn = 0;
        return;
    }

    lineCursor.cursor.position.x = lineCursor.cursor.wantedColumn;
    if (lineCursor.cursor.position.x > (int) currentLine->Length()) {
        lineCursor.cursor.position.x = (int) currentLine->Length();
    }
}

/*
 * Page Up/Down navigation works differently depending on your editor
 * CLion/Sublime:
 *      The content/text moves and cursor stays in position
 *      ALT+Up/Down, the cursor moves within the view area, content/text stays
 * VSCode:
 *      The cursor moves to next to last-visible line
 *      ALT+Up/Down the view area moves but cursor/activeline stays
 */

bool EditorModel::OnActionPageDown() {
    verticalNavigationViewModel->OnNavigateDown(viewRect.Height() - 1, viewRect, Lines().size());
    UpdateModelFromNavigation(true);
    return true;
}

bool EditorModel::OnActionPageUp() {
    verticalNavigationViewModel->OnNavigateUp(viewRect.Height() - 1, viewRect, Lines().size());
    UpdateModelFromNavigation(true);
    return true;
}

bool EditorModel::OnActionLineDown(const KeyPressAction &kpAction) {
    auto currentLine = ActiveLine();
    if (currentLine == nullptr) {
        return true;
    }
    auto &lineCursor = GetLineCursor();
    auto &cursor = GetCursor();
    verticalNavigationViewModel->OnNavigateDown(1, viewRect, Lines().size());
    UpdateModelFromNavigation(true);

    return true;
}
bool EditorModel::OnActionLineUp() {
    auto currentLine = ActiveLine();
    if (currentLine == nullptr) {
        return true;
    }

    verticalNavigationViewModel->OnNavigateUp(1, viewRect, Lines().size());
    UpdateModelFromNavigation(true);
    return true;
}

bool EditorModel::OnActionGotoTopLine() {
    auto &lineCursor = GetLineCursor();

    lineCursor.cursor.position.y = 0;
    lineCursor.idxActiveLine = lineCursor.viewTopLine;
    //logger->Debug("GotoTopLine, new cursor=(%d:%d)", editorModel->cursor.position.x, editorModel->cursor.position.y);
    return true;
}

bool EditorModel::OnActionGotoBottomLine() {
    //logger->Debug("GotoBottomLine (def: PageDown+CMDKey), cursor=(%d:%d)", editorModel->cursor.position.x, editorModel->cursor.position.y);

    auto &lineCursor = GetLineCursor();
    lineCursor.cursor.position.y = viewRect.Height()-1;
    lineCursor.idxActiveLine = lineCursor.viewBottomLine-1;

    //logger->Debug("GotoBottomLine, new  cursor=(%d:%d)", editorModel->cursor.position.x, editorModel->cursor.position.y);
    return true;
}

void EditorModel::Undo(Cursor &cursor, size_t &idxActiveLine) {
    if (!historyBuffer.HaveHistory()) {
        return;
    }

    auto regionStartLine = idxActiveLine;
    auto regionEndLine = idxActiveLine;

    logger->Debug("Undo, regionStartLine=%d", (int)regionStartLine);

    historyBuffer.Dump();
    logger->Debug("Undo, lines before: %zu", textBuffer->NumLines());
    auto nLinesRestored = historyBuffer.RestoreOneItem(cursor, idxActiveLine, textBuffer);
    logger->Debug("Undo, lines after: %zu - restored: %d", textBuffer->NumLines(), nLinesRestored);

    // Subtraction would lead to UB...
    if (nLinesRestored > regionStartLine) {
        regionStartLine = 0;
    }
    regionEndLine += nLinesRestored;
    UpdateSyntaxForRegion(regionStartLine, regionEndLine);
}


size_t EditorModel::NewLine(size_t idxActiveLine, Cursor &cursor) {

    auto undoItem = historyBuffer.NewUndoFromLineRange(idxActiveLine, idxActiveLine+1);
    undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kDeleteBeforeInsert);


    auto &lines = Lines();
    auto currentLine = LineAt(idxActiveLine);
    //auto tabSize = EditorConfig::Instance().tabSize;
    auto tabSize = textBuffer->GetLanguage().GetTabSize();

    int cursorXPos = 0;

    if (currentLine != nullptr) {
        logger->Debug("NewLine, current=%s [indent=%d]", UnicodeHelper::utf32toascii(currentLine->Buffer().data()).c_str(), currentLine->GetIndent());
    }

    Line::Ref emptyLine = nullptr;

    auto it = lines.begin() + idxActiveLine;
    if (lines.size() == 0) {
        textBuffer->Insert(idxActiveLine, Line::Create());
        UpdateSyntaxForBuffer();
    } else {
        if (cursor.position.x == 0) {
            // Insert empty line...
            textBuffer->Insert(idxActiveLine, Line::Create());
            UpdateSyntaxForActiveLineRegion();
            idxActiveLine++;
        } else {
            // Split, move some chars from current to new...
            auto newLine = Line::Create();
            currentLine->Move(newLine, 0, cursor.position.x);

            // Defer to the language parser if we should auto-insert a new line or not..
            // For instance, if you press enter next to '}' in CPP we insert another line and indent that..
            if (textBuffer->GetLanguage().OnPreCreateNewLine(newLine) == LanguageBase::kInsertAction::kNewLine) {
                // Insert an empty line - this will be the new active line...
                logger->Debug("Creating empty line...");
                emptyLine = Line::Create(U"");
                textBuffer->Insert(++idxActiveLine, emptyLine);
            }

            textBuffer->Insert(idxActiveLine+1, newLine);

            // This will compute the correct indent, -2/+2 are just arbitary choosen to expand the region
            // clipping is also performed by the syntax parser
            size_t idxStartParse = (idxActiveLine>2)?idxActiveLine-2:0;
            size_t idxEndParse = (textBuffer->NumLines() > (idxActiveLine + 2))?idxActiveLine+2:textBuffer->NumLines();

            auto ptrJob = UpdateSyntaxForRegion(idxStartParse, idxEndParse);
            ptrJob->WaitComplete();

            // Syntax update complete - we can now properly indent the line...
            cursorXPos = tabSize * newLine->Indent(tabSize);

            // Did we create an empty extra line? - if so, let's indent it properly.
            // note: we overwrite the cursor X as we will be positioned ourselves on this line
            if (emptyLine != nullptr) {
                logger->Debug("EmptyLine, inserting indent: %d", emptyLine->GetIndent());
                cursorXPos = tabSize * emptyLine->Indent(tabSize);
            }

            idxActiveLine++;
        }
    }

    cursor.wantedColumn = cursorXPos;
    cursor.position.x = cursorXPos;

    EndUndoItem(undoItem);

    return idxActiveLine;
}


void EditorModel::UpdateSyntaxForBuffer() {
    logger->Debug("Syntax update for full bufffer");
    textBuffer->Reparse();
}

Job::Ref EditorModel::UpdateSyntaxForRegion(size_t idxStartLine, size_t idxEndLine) {
    logger->Debug("Syntax update for region %zu - %zu", idxStartLine, idxEndLine);
    return textBuffer->ReparseRegion(idxStartLine, idxEndLine);
}

Job::Ref EditorModel::UpdateSyntaxForActiveLineRegion() {

    auto idxActiveLine = lineCursor.idxActiveLine;
    size_t idxStartParse = (idxActiveLine>2)?idxActiveLine-2:0;
    size_t idxEndParse = (textBuffer->NumLines() > (idxActiveLine + 2))?idxActiveLine+2:textBuffer->NumLines();
    logger->Debug("Syntax update for active line region, active line = %zu", idxActiveLine);
    return UpdateSyntaxForRegion(idxStartParse,idxEndParse);
}


UndoHistory::UndoItem::Ref EditorModel::BeginUndoItem() {
    auto undoItem = historyBuffer.NewUndoItem();
    return undoItem;
}
UndoHistory::UndoItem::Ref EditorModel::BeginUndoFromLineRange(size_t idxStartLine, size_t idxEndLine) {
    auto undoItem = historyBuffer.NewUndoFromLineRange(idxStartLine, idxEndLine);
    return undoItem;
}


void EditorModel::EndUndoItem(UndoHistory::UndoItem::Ref undoItem) {
    historyBuffer.PushUndoItem(undoItem);
}


void EditorModel::DeleteLinesNoSyntaxUpdate(size_t idxLineStart, size_t idxLineEnd) {
    for(auto lineIndex = idxLineStart;lineIndex < idxLineEnd; lineIndex++) {
        // Delete the same line several times - as we move the lines after up..
        textBuffer->DeleteLineAt(idxLineStart);
    }
}

void EditorModel::DeleteRange(const Point &startPos, const Point &endPos) {
    logger->Debug("DeleteRange, startPos (x=%d, y=%d), endPos (x=%d, y=%d)",
                  startPos.x, startPos.y,
                  endPos.x, endPos.y);

    auto undoItem = historyBuffer.NewUndoFromSelection();
    if ((startPos.x == 0) && (endPos.x == 0)) {
        undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kInsertAsNew);
    } else {
        undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kDeleteFirstBeforeInsert);
    }
    historyBuffer.PushUndoItem(undoItem);


    // Delete range within one line..
    if (startPos.y == endPos.y) {
        auto line = textBuffer->LineAt(startPos.y);
        line->Delete(startPos.x, endPos.x - startPos.x);
        UpdateSyntaxForRegion(startPos.y, endPos.y+1);

        return;
    }

    auto startLine = textBuffer->LineAt(startPos.y);
    int y = startPos.y;
    int dy = endPos.y - startPos.y;
    if (startPos.x != 0) {
        startLine->Delete(startPos.x, startLine->Length()-startPos.x);
        y++;
    }
    // FIX-ME: Special case, when (endPos.x == 0) && (start.x > 0) && (start.y != end.y) -> we should pull the last FULL line upp to start.x
    // Perhaps easier, if startPos.x > 0 and start.y != end.y we should concat the endpos line
    // I.e. no need for the if-case below, it can be integrated in to the upper if-case and solved directly (which makes it easier)

    // If x > 0, we have a partial marked end-line so let's delete that partial data before we chunk the lines
    if (endPos.x > 0) {
        // end-pos is not 0, so we need to chop off stuff at the last line and merge with the first line...
        auto line = textBuffer->LineAt(endPos.y);
        line->Delete(0, endPos.x);
        startLine->Append(line);
    }

    logger->Debug("DeleteRange, fromLine=%d, nLines=%d",y,dy);
    if (dy > 0) {
        DeleteLinesNoSyntaxUpdate(y, y + dy);
    }

    UpdateSyntaxForRegion(startPos.y, endPos.y+1);
}


void EditorModel::DeleteSelection() {
    auto startPos = currentSelection.GetStart();
    auto endPos = currentSelection.GetEnd();

    DeleteRange(startPos, endPos);
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
        AddLineComment(lineCursor.idxActiveLine, lineCursor.idxActiveLine+1, lineCommentPrefix);
        return;
    }

    auto start = currentSelection.GetStart();
    auto end = currentSelection.GetEnd();
    AddLineComment(start.y, end.y, lineCommentPrefix);
}

void EditorModel::IndentSelectionOrLine() {
    if (!GetTextBuffer()->HaveLanguage()) {
        return;
    }

    if (!IsSelectionActive()) {
        IndentLines(lineCursor.idxActiveLine, lineCursor.idxActiveLine + 1);
        return;
    }
    auto start = currentSelection.GetStart();
    auto end = currentSelection.GetEnd();
    IndentLines(start.y, end.y);
}

void EditorModel::UnindentSelectionOrLine() {

    if (!GetTextBuffer()->HaveLanguage()) {
        return;
    }

    if (!IsSelectionActive()) {
        UnindentLines(lineCursor.idxActiveLine, lineCursor.idxActiveLine + 1);
        return;
    }
    auto start = currentSelection.GetStart();
    auto end = currentSelection.GetEnd();
    UnindentLines(start.y, end.y);
}

void EditorModel::AddLineComment(size_t idxLineStart, size_t idxLineEnd, const std::u32string &lineCommentPrefix) {

    auto undoItem = BeginUndoFromLineRange(idxLineStart, idxLineEnd);
    undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kClearAndAppend);
    EndUndoItem(undoItem);


    for (size_t idxLine = idxLineStart; idxLine < idxLineEnd; idxLine += 1) {
        auto line = LineAt(idxLine);
        if (!line->StartsWith(lineCommentPrefix)) {
            line->Insert(0, lineCommentPrefix);
        } else {
            line->Delete(0, 2);
        }
    }

    UpdateSyntaxForRegion(idxLineStart, idxLineEnd);
}

void EditorModel::IndentLines(size_t idxLineStart, size_t idxLineEnd) {
    auto undoItem = BeginUndoFromLineRange(idxLineStart, idxLineEnd);
    undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kClearAndAppend);
    EndUndoItem(undoItem);

    auto tabSize = GetTextBuffer()->GetLanguage().GetTabSize();
    std::u32string strIndent;
    for(int i=0;i<tabSize;i++) {
        strIndent += U" ";
    }

    for (size_t idxLine = idxLineStart; idxLine < idxLineEnd; idxLine += 1) {
        auto line = LineAt(idxLine);
        line->Insert(0, strIndent);
    }

    UpdateSyntaxForRegion(idxLineStart, idxLineEnd);
}

void EditorModel::UnindentLines(size_t idxLineStart, size_t idxLineEnd) {
    auto undoItem = BeginUndoFromLineRange(idxLineStart, idxLineEnd);
    undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kClearAndAppend);
    EndUndoItem(undoItem);

    auto tabSize = GetTextBuffer()->GetLanguage().GetTabSize();

    for (size_t idxLine = idxLineStart; idxLine < idxLineEnd; idxLine += 1) {
        auto line = LineAt(idxLine);
        line->Unindent(tabSize);
    }

    UpdateSyntaxForRegion(idxLineStart, idxLineEnd);

}

void EditorModel::AddTab() {
    auto line = textBuffer->LineAt(lineCursor.idxActiveLine);
    auto undoItem = BeginUndoItem();

    auto tabSize = textBuffer->GetLanguage().GetTabSize();

    for (int i = 0; i < tabSize; i++) {
        AddCharToLineNoUndo(lineCursor.cursor, line, ' ');
    }
    EndUndoItem(undoItem);
}

void EditorModel::DelTab() {
    auto line = textBuffer->LineAt(lineCursor.idxActiveLine);
    auto nDel = textBuffer->GetLanguage().GetTabSize();
    if(lineCursor.cursor.position.x < nDel) {
        nDel = lineCursor.cursor.position.x;
    }
    auto undoItem = BeginUndoItem();
    for (int i = 0; i < nDel; i++) {
        RemoveCharFromLineNoUndo(lineCursor.cursor, line);
    }
    EndUndoItem(undoItem);
}


void EditorModel::AddCharToLineNoUndo(Cursor &cursor, Line::Ref line, char32_t ch) {
    line->Insert(cursor.position.x, ch);
    cursor.position.x++;
    cursor.wantedColumn = cursor.position.x;
}

void EditorModel::RemoveCharFromLineNoUndo(gedit::Cursor &cursor, Line::Ref line) {
    if (cursor.position.x > 0) {
        line->Delete(cursor.position.x-1);
        cursor.position.x--;
        if (cursor.position.x < 0) {
            cursor.position.x = 0;
        }
        cursor.wantedColumn = cursor.position.x;
    }
}

void EditorModel::PasteFromClipboard() {
    logger->Debug("Paste from clipboard");
    RuntimeConfig::Instance().GetScreen()->UpdateClipboardData();
    auto &clipboard = Editor::Instance().GetClipBoard();
    if (clipboard.Top() == nullptr) {
        logger->Debug("Clipboard empty!");
        return;
    }
    auto textBuffer = GetTextBuffer();
    auto nLines = clipboard.Top()->GetLineCount();
    auto ptWhere = lineCursor.cursor.position;

    auto undoItem = BeginUndoFromLineRange(lineCursor.idxActiveLine, lineCursor.idxActiveLine+nLines);
    undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kDeleteBeforeInsert);

    ptWhere.y += (int)lineCursor.viewTopLine;
    clipboard.PasteToBuffer(textBuffer, ptWhere);

    EndUndoItem(undoItem);

    textBuffer->ReparseRegion(lineCursor.idxActiveLine, lineCursor.idxActiveLine + nLines);

    lineCursor.idxActiveLine += nLines;
    lineCursor.cursor.position.y += nLines;

}
