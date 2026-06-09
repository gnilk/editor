//
// Refactor this - controller should be the one modifying the textBuffer
// The document should only hold data which sits between the actual text-data and the viewer, like selection and search stuff
// We need a good way to define how the document and the controller interoperate as the controller either needs access to data in the document
// OR the document needs to talk to a modifier API in the controller
//
#include <chrono>
#include "Editor.h"
#include "Document.h"
#include "logger.h"

using namespace gedit;

// This is the global section in the config.yml for this view
static const std::string cfgSectionName = "editorview";


Document::Ref Document::Create(TextBuffer::Ref newTextBuffer) {
    Document::Ref document = std::make_shared<Document>(newTextBuffer);
    document->Begin();
    return document;
}

void Document::Begin() {
    logger = gnilk::Logger::GetLogRef("Document");
    bUseCLionPageNav = Config::Instance()[cfgSectionName].GetBool("pgupdown_content_first", true);
    if (bUseCLionPageNav) {
        verticalNavigationViewModel = std::make_unique<VerticalNavigationCLion>();
    } else {
        verticalNavigationViewModel = std::make_unique<VerticalNavigationVSCode>();
    }
    verticalNavigationViewModel->lineCursor = GetLineCursorRef();

}

void Document::OnViewInit(const Rect &rect) {
    viewRect = rect;

    verticalNavigationViewModel->HandleResize(rect);

    // Need support in controller to forward this to document...
    viewState->lineCursor.viewTopLine = 0;
    viewState->lineCursor.viewBottomLine = rect.Height();

    UpdateDocumentFromNavigation(true);

}



// This is a little naive and I should probably spin it of to a specific thread
size_t Document::SearchFor(const std::u32string &searchItem) {

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
            auto logger = gnilk::Logger::GetLogger("Document");
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
    auto logger = gnilk::Logger::GetLogger("Document");
    logger->Debug("Search took %zu milliseconds", msDuration);

    // Number of hits..
    return searchResults.size();
}

bool Document::JumpToSearchHit(size_t idxHit) {
    if (idxHit >= searchResults.size()) {
        return false;
    }
    auto &result = searchResults[idxHit];
    GetCursor().position.y = result.idxLine;
    GetCursor().position.x = result.cursor_x;
    viewState->lineCursor.idxActiveLine = result.idxLine;
    CaptureWantedColumn(GetCursor(), LineAt(result.idxLine));

    RefocusViewArea();
    return true;
}

// Call this function to re-center the view area around the active line...
// the active line (line in focus) is positioned 1/3 (of num-lines) down from top
void Document::RefocusViewArea() {
    if (!viewState->lineCursor.IsInside(viewState->lineCursor.idxActiveLine)) {

        auto height = viewState->lineCursor.Height();
        int margin = height / 3;

        viewState->lineCursor.viewTopLine = viewState->lineCursor.idxActiveLine - margin;
        if (viewState->lineCursor.viewTopLine < 0) {
            viewState->lineCursor.viewTopLine = 0;
        }
        viewState->lineCursor.viewBottomLine = viewState->lineCursor.viewTopLine + height;
    }
}


void Document::ClearSearchResults() {
    searchResults.clear();
}

void Document::NextSearchResult() {
    idxActiveSearchHit++;
    if (!JumpToSearchHit(idxActiveSearchHit) && (idxActiveSearchHit > 0)) {
        idxActiveSearchHit -=1;
    }

}
void Document::PrevSearchResult() {
    if (idxActiveSearchHit > 0) {
        idxActiveSearchHit -= 1;
    }
    JumpToSearchHit(idxActiveSearchHit);

}

void Document::ResetSearchHitIndex() {
    idxActiveSearchHit = 0;
}

size_t Document::GetSearchHitIndex() {
    return idxActiveSearchHit;
}

bool Document::LoadData(const std::filesystem::path &pathName) {
    auto logger = gnilk::Logger::GetLogger("Document");
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

bool Document::SaveData(const std::filesystem::path &pathName) {
    return textBuffer->Save(pathName);
}
bool Document::SaveDataNoChangeCheck(const std::filesystem::path &pathName) {
    return textBuffer->SaveForce(pathName);
}

// No-arg variants operate on the document's own path (its file identity).
bool Document::Load() {
    return LoadData(path);
}
bool Document::Save() {
    return SaveData(path);
}
bool Document::SaveForce() {
    return SaveDataNoChangeCheck(path);
}

/////////
bool Document::OnAction(const KeyPressAction &kpAction) {
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

        viewState->lineCursor.idxActiveLine = selection.GetStart().y;
        viewState->lineCursor.cursor.position = selection.GetStart();
        viewState->lineCursor.cursor.position.y -= viewState->lineCursor.viewTopLine;   // Translate to screen coords..

        DeleteSelection();
        CancelSelection();
        UpdateDocumentFromNavigation(false);
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


bool Document::DispatchAction(const KeyPressAction &kpAction) {
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
bool Document::OnActionIndent() {
    auto undoItem = BeginUndoItem();
    AddTab();
    EndUndoItem(undoItem);
    UpdateSyntaxForActiveLineRegion();
    return true;
}
bool Document::OnActionUnindent() {
    auto undoItem = BeginUndoItem();
    DelTab();
    EndUndoItem(undoItem);
    UpdateSyntaxForActiveLineRegion();
    return true;
}


//bool EditorView::OnActionBackspace() {
//    auto currentLine = document->GetEditController()->LineAt(document->idxActiveLine);
//    if (document->cursor.position.x > 0) {
//        logger->Debug("OnActionBackspace");
//        std::string strMarker(document->cursor.position.x-1,' ');
//        logger->Debug("  LineBefore: '%s'", currentLine->Buffer().data());
//        logger->Debug("               %s*", strMarker.c_str());
//        logger->Debug("  Delete at: %d", document->cursor.position.x-1);
//        currentLine->Delete(document->cursor.position.x-1);
//        logger->Debug("  LineAfter: '%s'", currentLine->Buffer().data());
//        document->cursor.position.x--;
//        document->GetEditController()->UpdateSyntaxForBuffer();
//    }
//    return true;
//}

// Move all actions to controller/document...
bool Document::OnActionUndo() {
    //document->GetTextBuffer()->Undo();
    Undo(viewState->lineCursor.cursor, viewState->lineCursor.idxActiveLine);
    // auto nLinesAfter = GetTextBuffer()->NumLines();
    // //if ((nLinesAfter > viewState->lineCursor.viewBottomLine) && (viewState->lineCursor.Height() < nLinesAfter)
    // if (nLinesAfter > viewRect.Height()) {
    //     nLinesAfter = viewRect.Height();
    // }
    // viewState->lineCursor.viewBottomLine = viewState->lineCursor.viewTopLine + nLinesAfter;


    return true;
}

bool Document::OnActionLineHome() {
    viewState->lineCursor.cursor.position.x = 0;
    viewState->lineCursor.cursor.wantedColumn = 0;
    return true;
}

bool Document::OnActionLineEnd() {
    auto currentLine = LineAt(viewState->lineCursor.idxActiveLine);
    if (currentLine == nullptr) {
        return true;
    }
    auto endpos = currentLine->Length();
    viewState->lineCursor.cursor.position.x = endpos;
    CaptureWantedColumn(viewState->lineCursor.cursor, currentLine);
    return true;
}


bool Document::OnActionCommitLine() {

    // Should newline be here
    logger->Debug("OnActionCommitLine, Before: idxActive=%zu", viewState->lineCursor.idxActiveLine);
    NewLine(viewState->lineCursor.idxActiveLine, viewState->lineCursor.cursor);

    // Need viewRect - this is the visible view of the renderer
    verticalNavigationViewModel->OnNavigateDown(1, viewRect, Lines().size());
    UpdateDocumentFromNavigation(true);
    logger->Debug("OnActionCommitLine, After: idxActive=%zu", viewState->lineCursor.idxActiveLine);

    //InvalidateView();
    return true;
}

bool Document::OnActionWordRight() {
    auto currentLine = ActiveLine();
    auto &cursor = GetCursor();
    auto attrib = currentLine->AttributeAt(cursor.position.x);
    // End of line? - just navigate down one line and start over
    if (cursor.position.x == currentLine->Length()) {
        verticalNavigationViewModel->OnNavigateDown(1, viewRect, Lines().size());
        cursor.position.x = 0;
    } else if ((attrib->idxOrigString < cursor.position.x) && (cursor.position.x < currentLine->Length())) {
        // Last token - position ourselves at the end
        auto endpos = currentLine->Length();
        viewState->lineCursor.cursor.position.x = endpos;
    } else {
        // Skip to beginning of next token...
        attrib++;
        cursor.position.x = attrib->idxOrigString;
    }
    return true;
}

bool Document::OnActionWordLeft() {
    auto currentLine = ActiveLine(); //document->GetEditController()->LineAt(document->idxActiveLine);
    auto &cursor = GetCursor();
    if (cursor.position.x == 0) {
        verticalNavigationViewModel->OnNavigateUp(1, viewRect, Lines().size());
        currentLine = ActiveLine();
        cursor.position.x = currentLine->Length();
    } else {
        auto attrib = currentLine->AttributeAt(cursor.position.x);
        if (cursor.position.x == attrib->idxOrigString) {
            attrib--;
        }
        cursor.position.x = attrib->idxOrigString;
    }
    return true;
}

bool Document::OnActionGotoFirstLine() {
    logger->Debug("GotoFirstLine (def: CMD+Home), resetting cursor and view data!");
    viewState->lineCursor.cursor.position.x = 0;
    viewState->lineCursor.cursor.position.y = 0;
    viewState->lineCursor.idxActiveLine = 0;
    viewState->lineCursor.viewTopLine = 0;
    // Need viewRect
    viewState->lineCursor.viewBottomLine = viewRect.Height();

    return true;
}
bool Document::OnActionGotoLastLine() {
    logger->Debug("GotoLastLine (def: CMD+End), set cursor to last line!");
    viewState->lineCursor.cursor.position.x = 0;
    viewState->lineCursor.cursor.position.y = viewRect.Height()-1;
    viewState->lineCursor.idxActiveLine = Lines().size()-1;
    viewState->lineCursor.viewBottomLine = Lines().size();
    viewState->lineCursor.viewTopLine = viewState->lineCursor.viewBottomLine - viewRect.Height();
    if (viewState->lineCursor.viewTopLine < 0) {
        viewState->lineCursor.viewTopLine = 0;
    }

    logger->Debug("Cursor: %d:%d, idxActiveLine: %d",viewState->lineCursor.cursor.position.x, viewState->lineCursor.cursor.position.y, viewState->lineCursor.idxActiveLine);

    return true;
}


bool Document::OnActionStepLeft() {
    auto &cursor = GetCursor();
    cursor.position.x--;
    if (cursor.position.x < 0) {
        cursor.position.x = 0;
    }
    CaptureWantedColumn(cursor, ActiveLine());
    return true;
}
bool Document::OnActionStepRight() {
    auto currentLine = ActiveLine();
    auto &cursor = GetCursor();
    cursor.position.x++;
    if (cursor.position.x > (int)currentLine->Length()) {
        cursor.position.x = (int)currentLine->Length();
    }
    CaptureWantedColumn(cursor, currentLine);
    return true;
}

bool Document::OnNextSearchResult() {
    if (!HaveSearchResults()) {
        return false;
    }
    NextSearchResult();
    return true;
}
bool Document::OnPrevSearchResult() {
    if (!HaveSearchResults()) {
        return false;
    }
    PrevSearchResult();
    return true;
}


// Not sure this should be here
void Document::UpdateDocumentFromNavigation(bool updateCursor) {

    if (!updateCursor) {
        return;
    }

    auto currentLine = LineAt(viewState->lineCursor.idxActiveLine);
    if (currentLine == nullptr) {
        viewState->lineCursor.cursor.position.x = 0;
        viewState->lineCursor.cursor.position.y = 0;
        viewState->lineCursor.cursor.wantedColumn = 0;
        return;
    }

    ApplyWantedColumn(viewState->lineCursor.cursor, currentLine);
}

int Document::GetTabSize() {
    if (!textBuffer->HaveLanguage()) {
        return 4;
    }
    return textBuffer->GetLanguage().GetTabSize();
}

void Document::CaptureWantedColumn(Cursor &cursor, const Line::Ref &line) {
    if (line == nullptr) {
        cursor.wantedColumn = cursor.position.x;
        return;
    }
    cursor.wantedColumn = line->CharToVisualColumn(cursor.position.x, GetTabSize());
}

void Document::ApplyWantedColumn(Cursor &cursor, const Line::Ref &line) {
    if (line == nullptr) {
        cursor.position.x = 0;
        return;
    }
    cursor.position.x = line->VisualToCharIndex(cursor.wantedColumn, GetTabSize());
    if (cursor.position.x > (int) line->Length()) {
        cursor.position.x = (int) line->Length();
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

bool Document::OnActionPageDown() {
    verticalNavigationViewModel->OnNavigateDown(viewRect.Height() - 1, viewRect, Lines().size());
    UpdateDocumentFromNavigation(true);
    return true;
}

bool Document::OnActionPageUp() {
    verticalNavigationViewModel->OnNavigateUp(viewRect.Height() - 1, viewRect, Lines().size());
    UpdateDocumentFromNavigation(true);
    return true;
}

bool Document::OnActionLineDown(const KeyPressAction &kpAction) {
    auto currentLine = ActiveLine();
    if (currentLine == nullptr) {
        return true;
    }
    auto &cursor = GetCursor();
    verticalNavigationViewModel->OnNavigateDown(1, viewRect, Lines().size());
    UpdateDocumentFromNavigation(true);

    return true;
}
bool Document::OnActionLineUp() {
    auto currentLine = ActiveLine();
    if (currentLine == nullptr) {
        return true;
    }

    verticalNavigationViewModel->OnNavigateUp(1, viewRect, Lines().size());
    UpdateDocumentFromNavigation(true);
    return true;
}

bool Document::OnActionGotoTopLine() {

    viewState->lineCursor.cursor.position.y = 0;
    viewState->lineCursor.idxActiveLine = viewState->lineCursor.viewTopLine;
    //logger->Debug("GotoTopLine, new cursor=(%d:%d)", document->cursor.position.x, document->cursor.position.y);
    return true;
}

bool Document::OnActionGotoBottomLine() {
    //logger->Debug("GotoBottomLine (def: PageDown+CMDKey), cursor=(%d:%d)", document->cursor.position.x, document->cursor.position.y);

    viewState->lineCursor.cursor.position.y = viewRect.Height()-1;
    viewState->lineCursor.idxActiveLine = viewState->lineCursor.viewBottomLine-1;

    //logger->Debug("GotoBottomLine, new  cursor=(%d:%d)", document->cursor.position.x, document->cursor.position.y);
    return true;
}

void Document::Undo(Cursor &cursor, size_t &idxActiveLine) {
    if (!editState->historyBuffer.HaveHistory()) {
        return;
    }

    auto regionStartLine = idxActiveLine;
    auto regionEndLine = idxActiveLine;

    logger->Debug("Undo, regionStartLine=%d", (int)regionStartLine);

    editState->historyBuffer.Dump();
    logger->Debug("Undo, lines before: %zu", textBuffer->NumLines());
    auto nLinesRestored = editState->historyBuffer.RestoreOneItem(cursor, idxActiveLine, textBuffer);
    logger->Debug("Undo, lines after: %zu - restored: %d", textBuffer->NumLines(), nLinesRestored);

    // Subtraction would lead to UB...
    if (nLinesRestored > regionStartLine) {
        regionStartLine = 0;
    }
    regionEndLine += nLinesRestored;
    UpdateSyntaxForRegion(regionStartLine, regionEndLine);
}


size_t Document::NewLine(size_t idxActiveLine, Cursor &cursor) {

    auto undoItem = editState->historyBuffer.NewUndoFromLineRange(idxActiveLine, idxActiveLine+1);
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

    cursor.position.x = cursorXPos;
    CaptureWantedColumn(cursor, LineAt(idxActiveLine));

    EndUndoItem(undoItem);

    return idxActiveLine;
}


void Document::UpdateSyntaxForBuffer() {
    logger->Debug("Syntax update for full bufffer");
    textBuffer->Reparse();
}

Job::Ref Document::UpdateSyntaxForRegion(size_t idxStartLine, size_t idxEndLine) {
    logger->Debug("Syntax update for region %zu - %zu", idxStartLine, idxEndLine);
    return textBuffer->ReparseRegion(idxStartLine, idxEndLine);
}

Job::Ref Document::UpdateSyntaxForActiveLineRegion() {

    auto idxActiveLine = viewState->lineCursor.idxActiveLine;
    size_t idxStartParse = (idxActiveLine>2)?idxActiveLine-2:0;
    size_t idxEndParse = (textBuffer->NumLines() > (idxActiveLine + 2))?idxActiveLine+2:textBuffer->NumLines();
    logger->Debug("Syntax update for active line region, active line = %zu", idxActiveLine);
    return UpdateSyntaxForRegion(idxStartParse,idxEndParse);
}


UndoHistory::UndoItem::Ref Document::BeginUndoItem() {
    auto undoItem = editState->historyBuffer.NewUndoItem();
    return undoItem;
}
UndoHistory::UndoItem::Ref Document::BeginUndoFromLineRange(size_t idxStartLine, size_t idxEndLine) {
    auto undoItem = editState->historyBuffer.NewUndoFromLineRange(idxStartLine, idxEndLine);
    return undoItem;
}


void Document::EndUndoItem(UndoHistory::UndoItem::Ref undoItem) {
    editState->historyBuffer.PushUndoItem(undoItem);
}


void Document::DeleteLinesNoSyntaxUpdate(size_t idxLineStart, size_t idxLineEnd) {
    for(auto lineIndex = idxLineStart;lineIndex < idxLineEnd; lineIndex++) {
        // Delete the same line several times - as we move the lines after up..
        textBuffer->DeleteLineAt(idxLineStart);
    }
}

void Document::DeleteRange(const Point &startPos, const Point &endPos) {
    logger->Debug("DeleteRange, startPos (x=%d, y=%d), endPos (x=%d, y=%d)",
                  startPos.x, startPos.y,
                  endPos.x, endPos.y);

    auto undoItem = editState->historyBuffer.NewUndoFromSelection();
    if ((startPos.x == 0) && (endPos.x == 0)) {
        undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kInsertAsNew);
    } else {
        undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kDeleteFirstBeforeInsert);
    }
    editState->historyBuffer.PushUndoItem(undoItem);


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


void Document::DeleteSelection() {
    auto startPos = viewState->currentSelection.GetStart();
    auto endPos = viewState->currentSelection.GetEnd();

    DeleteRange(startPos, endPos);
}

void Document::CommentSelectionOrLine() {


    if (!textBuffer->HaveLanguage()) {
        return;
    }
    auto lineCommentPrefix = textBuffer->GetLanguage().GetLineComment();
    if (lineCommentPrefix.empty()) {
        return;
    }

    if (!IsSelectionActive()) {
        AddLineComment(viewState->lineCursor.idxActiveLine, viewState->lineCursor.idxActiveLine+1, lineCommentPrefix);
        return;
    }

    auto start = viewState->currentSelection.GetStart();
    auto end = viewState->currentSelection.GetEnd();
    AddLineComment(start.y, end.y, lineCommentPrefix);
}

void Document::IndentSelectionOrLine() {
    if (!GetTextBuffer()->HaveLanguage()) {
        return;
    }

    if (!IsSelectionActive()) {
        IndentLines(viewState->lineCursor.idxActiveLine, viewState->lineCursor.idxActiveLine + 1);
        return;
    }
    auto start = viewState->currentSelection.GetStart();
    auto end = viewState->currentSelection.GetEnd();
    IndentLines(start.y, end.y);
}

void Document::UnindentSelectionOrLine() {

    if (!GetTextBuffer()->HaveLanguage()) {
        return;
    }

    if (!IsSelectionActive()) {
        UnindentLines(viewState->lineCursor.idxActiveLine, viewState->lineCursor.idxActiveLine + 1);
        return;
    }
    auto start = viewState->currentSelection.GetStart();
    auto end = viewState->currentSelection.GetEnd();
    UnindentLines(start.y, end.y);
}

void Document::AddLineComment(size_t idxLineStart, size_t idxLineEnd, const std::u32string &lineCommentPrefix) {

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

void Document::IndentLines(size_t idxLineStart, size_t idxLineEnd) {
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

void Document::UnindentLines(size_t idxLineStart, size_t idxLineEnd) {
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

void Document::AddTab() {
    auto line = textBuffer->LineAt(viewState->lineCursor.idxActiveLine);
    auto undoItem = BeginUndoItem();

    auto tabSize = textBuffer->GetLanguage().GetTabSize();

    for (int i = 0; i < tabSize; i++) {
        AddCharToLineNoUndo(viewState->lineCursor.cursor, line, ' ');
    }
    EndUndoItem(undoItem);
}

void Document::DelTab() {
    auto line = textBuffer->LineAt(viewState->lineCursor.idxActiveLine);
    auto nDel = textBuffer->GetLanguage().GetTabSize();
    if(viewState->lineCursor.cursor.position.x < nDel) {
        nDel = viewState->lineCursor.cursor.position.x;
    }
    auto undoItem = BeginUndoItem();
    for (int i = 0; i < nDel; i++) {
        RemoveCharFromLineNoUndo(viewState->lineCursor.cursor, line);
    }
    EndUndoItem(undoItem);
}


void Document::AddCharToLineNoUndo(Cursor &cursor, Line::Ref line, char32_t ch) {
    line->Insert(cursor.position.x, ch);
    cursor.position.x++;
    CaptureWantedColumn(cursor, line);
}

void Document::RemoveCharFromLineNoUndo(gedit::Cursor &cursor, Line::Ref line) {
    if (cursor.position.x > 0) {
        line->Delete(cursor.position.x-1);
        cursor.position.x--;
        if (cursor.position.x < 0) {
            cursor.position.x = 0;
        }
        CaptureWantedColumn(cursor, line);
    }
}

void Document::PasteFromClipboard() {
    logger->Debug("Paste from clipboard");
    RuntimeConfig::Instance().GetScreen()->UpdateClipboardData();
    auto &clipboard = Editor::Instance().GetClipBoard();
    if (clipboard.Top() == nullptr) {
        logger->Debug("Clipboard empty!");
        return;
    }
    auto textBuffer = GetTextBuffer();

    // The paste occupies 'lineCount' lines on the target; 'linesAdded' is how many NEW lines it
    // introduces (0 for a splice contained within a single line). The exact number depends on the
    // selection shape, so ask the clipboard item rather than guessing from its raw line count.
    auto lineCount = clipboard.Top()->GetPasteLineCount();
    size_t linesAdded = (lineCount > 0) ? (lineCount - 1) : 0;

    auto ptWhere = viewState->lineCursor.cursor.position;
    ptWhere.y += (int)viewState->lineCursor.viewTopLine;

    UndoHistory::UndoItem::Ref undoItem;
    if (linesAdded == 0) {
        // In-place splice on a single line: snapshot and restore just that one line.
        undoItem = BeginUndoFromLineRange(viewState->lineCursor.idxActiveLine, viewState->lineCursor.idxActiveLine + 1);
        undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kClearAndAppend);
    } else {
        // Multi-line splice: the paste inserts 'linesAdded' new lines; undo deletes them and
        // restores the original target line.
        undoItem = BeginUndoFromLineRange(viewState->lineCursor.idxActiveLine, viewState->lineCursor.idxActiveLine + linesAdded);
        undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kDeleteBeforeInsert);
    }

    auto ptEnd = clipboard.PasteToBuffer(textBuffer, ptWhere);

    EndUndoItem(undoItem);

    // Reparse every line the splice touched (target line through the last inserted line).
    textBuffer->ReparseRegion(viewState->lineCursor.idxActiveLine, viewState->lineCursor.idxActiveLine + linesAdded + 1);

    // Land the caret at the end of the pasted text (PasteToBuffer reports where that is).
    viewState->lineCursor.idxActiveLine += linesAdded;
    viewState->lineCursor.cursor.position.y += (int)linesAdded;
    viewState->lineCursor.cursor.position.x = ptEnd.x;
    CaptureWantedColumn(viewState->lineCursor.cursor, ActiveLine());
}
