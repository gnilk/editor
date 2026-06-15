//
// Refactor this - controller should be the one modifying the textBuffer
// The document should only hold data which sits between the actual text-data and the viewer, like selection and search stuff
// We need a good way to define how the document and the controller interoperate as the controller either needs access to data in the document
// OR the document needs to talk to a modifier API in the controller
//
#include <chrono>
#include "Editor.h"
#include "Document.h"
#include "Core/Session/SessionState.h"
#include "Core/Language/IndentCache.h"
#include "Core/Language/IndentEngine.h"
#include "logger.h"

using namespace gedit;

// Look up the language's indent table and ask the engine for the new line's indent (and the '{|}'
// three-line expansion). The reference text is the line being split, splitX the cursor column on it.
static IndentEngine::Action ComputeNewLineIndent(LanguageBase &language, const std::u32string &referenceText,
                                                 int splitX, int tabSize);
// Prepend nSpaces leading spaces to a line; returns the resulting cursor column (== nSpaces).
static int ApplyLeadingIndent(const Line::Ref &line, int nSpaces);

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

    // Re-seed the viewport against the (possibly new) view height WITHOUT discarding the saved scroll
    // anchor. OnViewInit runs on every view (re-)init - including the document-switch re-point, where
    // the EditorView re-points at the now-active document (EditorView::ReInitView). Forcing
    // viewTopLine=0 here would throw away each document's scroll position on every switch, drawing the
    // caret off-screen even though its logical position survived. Keep viewTopLine; derive the bottom
    // from it + the height; then RefocusViewArea re-centres only if the active line fell out of view
    // (e.g. the height shrank). A freshly created document has viewTopLine==0, so first init is
    // unchanged.
    documentViewState->lineCursor.viewBottomLine = documentViewState->lineCursor.viewTopLine + rect.Height();
    RefocusViewArea();

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

void Document::SetCursorPosition(size_t idxLine, size_t idxChar) {
    GetCursor().position.y = idxLine;
    GetCursor().position.x = idxChar;
    documentViewState->lineCursor.idxActiveLine = idxLine;
    CaptureWantedColumn(GetCursor(), LineAt(idxLine));

    RefocusViewArea();
}

bool Document::JumpToSearchHit(size_t idxHit) {
    if (idxHit >= searchResults.size()) {
        return false;
    }
    auto &result = searchResults[idxHit];
    SetCursorPosition(result.idxLine, result.cursor_x);
    return true;
}

// Call this function to re-center the view area around the active line...
// the active line (line in focus) is positioned 1/3 (of num-lines) down from top
void Document::RefocusViewArea() {
    if (!documentViewState->lineCursor.IsInside(documentViewState->lineCursor.idxActiveLine)) {

        auto height = documentViewState->lineCursor.Height();
        int margin = height / 3;

        documentViewState->lineCursor.viewTopLine = documentViewState->lineCursor.idxActiveLine - margin;
        if (documentViewState->lineCursor.viewTopLine < 0) {
            documentViewState->lineCursor.viewTopLine = 0;
        }
        documentViewState->lineCursor.viewBottomLine = documentViewState->lineCursor.viewTopLine + height;
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

DocumentSession Document::ToSession() const {
    DocumentSession session = documentViewState->ToSession();
    session.path = path.string();
    return session;
}

void Document::FromSession(const DocumentSession &session) {
    // Path/buffer identity is owned by the reopen path; here we only restore the per-view editing state.
    documentViewState->FromSession(session);
}

/////////
bool Document::OnAction(const EditorAction &kpAction) {
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
        clipboard.CopyFromBuffer(GetTextBuffer(), selection.GetStart(), SelectionEndForCopy(selection));

    } else if (kpAction.action == kAction::kActionCutToClipboard) {
        logger->Debug("Cut text to clipboard");
        auto selection = GetSelection();
        auto startPos = selection.GetStart();
        auto endPos = SelectionEndForCopy(selection);   // whole-line cuts consume the trailing newline
        auto &clipboard = Editor::Instance().GetClipBoard();
        clipboard.CopyFromBuffer(GetTextBuffer(), startPos, endPos);

        documentViewState->lineCursor.idxActiveLine = startPos.y;
        documentViewState->lineCursor.cursor.position = startPos;
        documentViewState->lineCursor.cursor.position.y -= documentViewState->lineCursor.viewTopLine;   // Translate to screen coords..

        DeleteRange(startPos, endPos);
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


bool Document::DispatchAction(const EditorAction &kpAction) {
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
        case kAction::kActionReformatLine :
            return OnActionReformatLine();
        case kAction::kActionReformatBlock :
            return OnActionReformatBlock();
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
bool Document::OnActionReformatLine() {
    auto idxLine = documentViewState->lineCursor.idxActiveLine;
    ReindentLineRange(idxLine, idxLine);
    return true;
}
bool Document::OnActionReformatBlock() {
    if (IsSelectionActive()) {
        auto start = documentViewState->currentSelection.GetStart();
        auto end = documentViewState->currentSelection.GetEnd();
        size_t endY = end.y;
        // A selection ending at the very start of end.y doesn't actually include that line.
        if ((end.x == 0) && (end.y > start.y)) {
            endY = end.y - 1;
        }
        ReindentLineRange(start.y, endY);
    } else {
        // No selection: reformat the enclosing '{ }' block (or just the current line if not inside one).
        auto &lc = documentViewState->lineCursor;
        auto table = IndentCache::Instance().GetTableForLanguage(textBuffer->GetLanguage().GetConfigNodeName()).get();
        auto block = IndentEngine::FindEnclosingBlock(textBuffer->Lines(), lc.idxActiveLine,
                                                      lc.cursor.position.x, table);
        if (block.found) {
            ReindentLineRange(block.openY, block.closeY);
        } else {
            ReindentLineRange(lc.idxActiveLine, lc.idxActiveLine);
        }
    }
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
    Undo(documentViewState->lineCursor.cursor, documentViewState->lineCursor.idxActiveLine);
    // auto nLinesAfter = GetTextBuffer()->NumLines();
    // //if ((nLinesAfter > documentViewState->lineCursor.viewBottomLine) && (documentViewState->lineCursor.Height() < nLinesAfter)
    // if (nLinesAfter > viewRect.Height()) {
    //     nLinesAfter = viewRect.Height();
    // }
    // documentViewState->lineCursor.viewBottomLine = documentViewState->lineCursor.viewTopLine + nLinesAfter;


    return true;
}

bool Document::OnActionLineHome() {
    documentViewState->lineCursor.cursor.position.x = 0;
    documentViewState->lineCursor.cursor.wantedColumn = 0;
    return true;
}

bool Document::OnActionLineEnd() {
    auto currentLine = LineAt(documentViewState->lineCursor.idxActiveLine);
    if (currentLine == nullptr) {
        return true;
    }
    auto endpos = currentLine->Length();
    documentViewState->lineCursor.cursor.position.x = endpos;
    CaptureWantedColumn(documentViewState->lineCursor.cursor, currentLine);
    return true;
}


bool Document::OnActionCommitLine() {

    // Should newline be here
    logger->Debug("OnActionCommitLine, Before: idxActive=%zu", documentViewState->lineCursor.idxActiveLine);
    NewLine(documentViewState->lineCursor.idxActiveLine, documentViewState->lineCursor.cursor);

    // Need viewRect - this is the visible view of the renderer
    verticalNavigationViewModel->OnNavigateDown(1, viewRect, Lines().size());
    UpdateDocumentFromNavigation(true);
    logger->Debug("OnActionCommitLine, After: idxActive=%zu", documentViewState->lineCursor.idxActiveLine);

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
        documentViewState->lineCursor.cursor.position.x = endpos;
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
        // Main case, trying to be smart here - but I am not sure I am...
        // a basic 'search backwards' is probably smarter..
        // also - this does not work for any view - we should probably not fiddle around in the cursor structure directly..
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
    documentViewState->lineCursor.cursor.position.x = 0;
    documentViewState->lineCursor.cursor.position.y = 0;
    documentViewState->lineCursor.idxActiveLine = 0;
    documentViewState->lineCursor.viewTopLine = 0;
    // Need viewRect
    documentViewState->lineCursor.viewBottomLine = viewRect.Height();

    return true;
}
bool Document::OnActionGotoLastLine() {
    logger->Debug("GotoLastLine (def: CMD+End), set cursor to last line!");
    documentViewState->lineCursor.cursor.position.x = 0;
    documentViewState->lineCursor.cursor.position.y = viewRect.Height()-1;
    documentViewState->lineCursor.idxActiveLine = Lines().size()-1;
    documentViewState->lineCursor.viewBottomLine = Lines().size();
    documentViewState->lineCursor.viewTopLine = documentViewState->lineCursor.viewBottomLine - viewRect.Height();
    if (documentViewState->lineCursor.viewTopLine < 0) {
        documentViewState->lineCursor.viewTopLine = 0;
    }

    logger->Debug("Cursor: %d:%d, idxActiveLine: %d",documentViewState->lineCursor.cursor.position.x, documentViewState->lineCursor.cursor.position.y, documentViewState->lineCursor.idxActiveLine);

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

    auto currentLine = LineAt(documentViewState->lineCursor.idxActiveLine);
    if (currentLine == nullptr) {
        documentViewState->lineCursor.cursor.position.x = 0;
        documentViewState->lineCursor.cursor.position.y = 0;
        documentViewState->lineCursor.cursor.wantedColumn = 0;
        return;
    }

    ApplyWantedColumn(documentViewState->lineCursor.cursor, currentLine);
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

bool Document::OnActionLineDown(const EditorAction &kpAction) {
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

    documentViewState->lineCursor.cursor.position.y = 0;
    documentViewState->lineCursor.idxActiveLine = documentViewState->lineCursor.viewTopLine;
    //logger->Debug("GotoTopLine, new cursor=(%d:%d)", document->cursor.position.x, document->cursor.position.y);
    return true;
}

bool Document::OnActionGotoBottomLine() {
    //logger->Debug("GotoBottomLine (def: PageDown+CMDKey), cursor=(%d:%d)", document->cursor.position.x, document->cursor.position.y);

    documentViewState->lineCursor.cursor.position.y = viewRect.Height()-1;
    documentViewState->lineCursor.idxActiveLine = documentViewState->lineCursor.viewBottomLine-1;

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

    auto undoItem = editState->historyBuffer.NewUndoFromLineRange(idxActiveLine, idxActiveLine+1, GetLineCursor(), textBuffer);
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
            // Split: the part AFTER the cursor moves down to a new line; the part before stays as the
            // reference line whose leading whitespace the indent engine measures.
            auto referenceText = currentLine->Buffer();
            int splitX = cursor.position.x;

            auto newLine = Line::Create();
            currentLine->Move(newLine, 0, splitX);

            // Ask the indent engine for the new line's indent + whether to expand '{|}' into three lines.
            auto indent = ComputeNewLineIndent(textBuffer->GetLanguage(), referenceText, splitX, tabSize);

            if (indent.insertBlankLine) {
                // '{|}' expansion: an indented empty line (where the cursor lands) above the closer line.
                emptyLine = Line::Create(U"");
                textBuffer->Insert(idxActiveLine + 1, emptyLine);
                textBuffer->Insert(idxActiveLine + 2, newLine);
            } else {
                textBuffer->Insert(idxActiveLine + 1, newLine);
            }

            // Reparse the region for tokenisation / drawing (-2/+2 expands it; the parser also clips). The
            // indent VALUE now comes from the engine above, not this reparse.
            size_t idxStartParse = (idxActiveLine>2)?idxActiveLine-2:0;
            size_t idxEndParse = (textBuffer->NumLines() > (idxActiveLine + 2))?idxActiveLine+2:textBuffer->NumLines();

            // With threaded parsing off the region is parsed synchronously and the job is null.
            auto ptrJob = UpdateSyntaxForRegion(idxStartParse, idxEndParse);
            if (ptrJob != nullptr) {
                ptrJob->WaitComplete();
            }

            // Apply the engine indent. In the expansion case the cursor lands on the middle empty line.
            if (indent.insertBlankLine) {
                ApplyLeadingIndent(newLine, indent.indentLevel * tabSize);
                cursorXPos = ApplyLeadingIndent(emptyLine, indent.blankLineLevel * tabSize);
            } else {
                cursorXPos = ApplyLeadingIndent(newLine, indent.indentLevel * tabSize);
            }

            idxActiveLine++;
        }
    }

    cursor.position.x = cursorXPos;
    CaptureWantedColumn(cursor, LineAt(idxActiveLine));

    EndUndoItem(undoItem);

    return idxActiveLine;
}

static IndentEngine::Action ComputeNewLineIndent(LanguageBase &language, const std::u32string &referenceText,
                                                 int splitX, int tabSize) {
    IndentEngine::Context ctx;
    ctx.table = IndentCache::Instance().GetTableForLanguage(language.GetConfigNodeName()).get();
    ctx.lineText = referenceText;
    ctx.cursorX = splitX;
    ctx.tabSize = tabSize;
    return IndentEngine::OnNewLine(ctx);
}

static int ApplyLeadingIndent(const Line::Ref &line, int nSpaces) {
    if (nSpaces > 0) {
        line->Insert(0, std::u32string(nSpaces, U' '));
    }
    return nSpaces;
}

// Replace a line's existing leading-whitespace run with exactly nSpaces spaces (reformat both grows and
// shrinks, unlike the newline path which only inserts). A wholly-blank line is left empty - reformat does
// not leave trailing whitespace on blank lines.
static void SetLineLeadingIndent(const Line::Ref &line, int nSpaces) {
    const auto &buf = line->Buffer();
    int runLen = 0;
    while ((runLen < (int)buf.size()) && ((buf[runLen] == U' ') || (buf[runLen] == U'\t'))) {
        runLen++;
    }
    bool allWhitespace = (runLen == (int)buf.size());
    if (runLen > 0) {
        line->Delete(0, runLen);
    }
    if (!allWhitespace && (nSpaces > 0)) {
        line->Insert(0, std::u32string(nSpaces, U' '));
    }
}

// Count of leading whitespace characters (spaces/tabs) on a line buffer.
static int LeadingWhitespaceCharCount(const std::u32string &buf) {
    int n = 0;
    while ((n < (int)buf.size()) && ((buf[n] == U' ') || (buf[n] == U'\t'))) {
        n++;
    }
    return n;
}

// Token class of the char at position x: the span whose start is the greatest idxOrigString <= x (attribs are
// ascending). kRegular when the line is unparsed (so an unparsed brace still counts - degrades to naive
// matching). NB: does NOT use Line::AttributeAt - that returns the FIRST span for any x in the last span,
// which would misread a trailing comment (always the last span) as code.
static kLanguageTokenClass TokenClassAtChar(const Line::Ref &line, int x) {
    auto &attribs = line->Attributes();
    if (attribs.empty()) {
        return kLanguageTokenClass::kRegular;
    }
    kLanguageTokenClass cls = kLanguageTokenClass::kRegular;
    for (const auto &a : attribs) {
        if (a.idxOrigString > x) {
            break;
        }
        cls = a.tokenClass;
    }
    return cls;
}

// True when a frozen line's content is a COMMENT (vs a string). A block-comment interior may be shifted as a
// rigid unit to follow its code; a multi-line/raw string must stay byte-faithful. Classifies by the first
// content char's token class (a frozen line sits entirely within one construct, so one char is decisive).
static bool IsCommentInterior(const Line::Ref &line) {
    const auto &buf = line->Buffer();
    for (int x = 0; x < (int)buf.size(); x++) {
        char32_t ch = buf[x];
        if ((ch == U' ') || (ch == U'\t')) {
            continue;
        }
        kLanguageTokenClass cls = TokenClassAtChar(line, x);
        return (cls == kLanguageTokenClass::kLineComment) || (cls == kLanguageTokenClass::kBlockComment) ||
               (cls == kLanguageTokenClass::kCommentedText);
    }
    return false;   // a blank line inside a construct - nothing to shift, leave it
}

// Resolve a line's structural triggers for the reindent walk: the first/last non-space char that is NOT
// inside a string/comment, plus whether the line STARTS inside a multi-line construct (then it is frozen -
// left byte-faithful, contributing no structure). Needs the parsed tokens + lexer state, so it lives here at
// the Document layer rather than in the pure engine.
static IndentEngine::RangeLineSyntax ComputeLineSyntax(const Line::Ref &line, const IndentTable *table) {
    IndentEngine::RangeLineSyntax syn;
    syn.isFrozen = (line->GetStateStackDepth() > 1);
    if (syn.isFrozen) {
        return syn;
    }
    const auto &buf = line->Buffer();
    for (int x = 0; x < (int)buf.size(); x++) {
        char32_t ch = buf[x];
        if ((ch == U' ') || (ch == U'\t')) {
            continue;
        }
        if ((table != nullptr) && table->IsSuppressed(TokenClassAtChar(line, x))) {
            continue;   // inside a string/comment span - not a structural trigger
        }
        if (syn.firstStructural == 0) {
            syn.firstStructural = ch;
        }
        syn.lastStructural = ch;
    }
    return syn;
}

// String form of SetLineLeadingIndent - used when building replacement line content before it is committed.
static void SetStringLeadingIndent(std::u32string &str, int nSpaces) {
    size_t runLen = 0;
    while ((runLen < str.size()) && ((str[runLen] == U' ') || (str[runLen] == U'\t'))) {
        runLen++;
    }
    bool allWhitespace = (runLen == str.size());
    str.erase(0, runLen);
    if (!allWhitespace && (nSpaces > 0)) {
        str.insert(0, std::u32string(nSpaces, U' '));
    }
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

    auto idxActiveLine = documentViewState->lineCursor.idxActiveLine;
    size_t idxStartParse = (idxActiveLine>2)?idxActiveLine-2:0;
    size_t idxEndParse = (textBuffer->NumLines() > (idxActiveLine + 2))?idxActiveLine+2:textBuffer->NumLines();
    logger->Debug("Syntax update for active line region, active line = %zu", idxActiveLine);
    return UpdateSyntaxForRegion(idxStartParse,idxEndParse);
}


UndoHistory::UndoItem::Ref Document::BeginUndoItem() {
    auto undoItem = editState->historyBuffer.NewUndoItem(GetLineCursor(), textBuffer);
    return undoItem;
}
UndoHistory::UndoItem::Ref Document::BeginUndoFromLineRange(size_t idxStartLine, size_t idxEndLine) {
    auto undoItem = editState->historyBuffer.NewUndoFromLineRange(idxStartLine, idxEndLine, GetLineCursor(), textBuffer);
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

    auto undoItem = editState->historyBuffer.NewUndoFromSelection(GetLineCursor(), GetSelection(), textBuffer);
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
    auto startPos = documentViewState->currentSelection.GetStart();
    auto endPos = documentViewState->currentSelection.GetEnd();

    DeleteRange(startPos, endPos);
}

gedit::Point Document::SelectionEndForCopy(const Selection &selection) {
    auto startPos = selection.GetStart();
    auto endPos = selection.GetEnd();
    if ((startPos.x != 0) || (endPos.x == 0)) {
        return endPos;      // not a whole-line selection, or already at a line break
    }
    auto lastLine = GetTextBuffer()->LineAt(endPos.y);
    bool endsAtEol = (lastLine != nullptr) && (endPos.x == (int)lastLine->Length());
    bool hasFollowingLine = ((size_t)(endPos.y + 1) < GetTextBuffer()->NumLines());
    if (endsAtEol && hasFollowingLine) {
        // Whole lines selected to EOL: the canonical end is column 0 of the next line. There the
        // existing copy/delete paths preserve / remove the trailing newline symmetrically.
        return gedit::Point(0, endPos.y + 1);
    }
    return endPos;          // stops mid-line, or at the buffer's last line (no trailing newline)
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
        AddLineComment(documentViewState->lineCursor.idxActiveLine, documentViewState->lineCursor.idxActiveLine+1, lineCommentPrefix);
        return;
    }

    auto start = documentViewState->currentSelection.GetStart();
    auto end = documentViewState->currentSelection.GetEnd();
    AddLineComment(start.y, end.y, lineCommentPrefix);
}

void Document::IndentSelectionOrLine() {
    if (!GetTextBuffer()->HaveLanguage()) {
        return;
    }

    if (!IsSelectionActive()) {
        IndentLines(documentViewState->lineCursor.idxActiveLine, documentViewState->lineCursor.idxActiveLine + 1);
        return;
    }
    auto start = documentViewState->currentSelection.GetStart();
    auto end = documentViewState->currentSelection.GetEnd();
    IndentLines(start.y, end.y);
}

void Document::UnindentSelectionOrLine() {

    if (!GetTextBuffer()->HaveLanguage()) {
        return;
    }

    if (!IsSelectionActive()) {
        UnindentLines(documentViewState->lineCursor.idxActiveLine, documentViewState->lineCursor.idxActiveLine + 1);
        return;
    }
    auto start = documentViewState->currentSelection.GetStart();
    auto end = documentViewState->currentSelection.GetEnd();
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

void Document::ReindentLineRange(size_t startY, size_t endY) {
    if (!GetTextBuffer()->HaveLanguage()) {
        return;
    }
    const auto &lines = textBuffer->Lines();
    size_t numLines = textBuffer->NumLines();
    if (lines.empty() || (startY >= numLines)) {
        return;
    }
    if (endY >= numLines) {
        endY = numLines - 1;
    }
    if (endY < startY) {
        endY = startY;
    }

    int tabSize = GetTabSize();
    auto table = IndentCache::Instance().GetTableForLanguage(textBuffer->GetLanguage().GetConfigNodeName()).get();

    // Forward-extend through any construct the range ends inside; seed from the trusted clean line above.
    size_t endExtended = IndentEngine::FindRangeEnd(lines, endY);
    int anchorLevel = IndentEngine::FindAnchorLevel(lines, startY, table, tabSize);

    // Compute the new levels BEFORE mutating - the RangeContext views point into the live line buffers.
    IndentEngine::RangeContext ctx;
    ctx.table = table;
    ctx.tabSize = tabSize;
    ctx.anchorLevel = anchorLevel;
    for (size_t y = startY; y <= endExtended; y++) {
        ctx.lines.emplace_back(lines[y]->Buffer());
        ctx.syntax.emplace_back(ComputeLineSyntax(lines[y], table));
    }
    auto levels = IndentEngine::ReindentRange(ctx);

    // Snapshot the rewritten range for undo, then set each line's leading whitespace to its computed level.
    auto undoItem = BeginUndoFromLineRange(startY, endExtended + 1);
    undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kClearAndAppend);
    EndUndoItem(undoItem);

    // Capture the active line's pre-edit geometry so the caret can follow its content across the reindent.
    auto &lineCursor = documentViewState->lineCursor;
    size_t activeIdx = lineCursor.idxActiveLine;
    bool activeInRange = (activeIdx >= startY) && (activeIdx <= endExtended);
    int oldCursorX = lineCursor.cursor.position.x;
    int oldLeadingChars = activeInRange ? LeadingWhitespaceCharCount(lines[activeIdx]->Buffer()) : 0;

    // Apply the computed indents. A frozen line (-1) is the interior of a multi-line construct: a COMMENT is
    // shifted as a rigid unit by the delta its opener moved (preserving its internal art), while a STRING is
    // left byte-faithful (shifting it would change the string's value). constructDelta carries the opener's
    // shift forward to the frozen run that immediately follows it.
    int constructDelta = 0;
    for (size_t i = 0; i < levels.size(); i++) {
        const auto &ln = lines[startY + i];
        int oldLead = LeadingWhitespaceCharCount(ln->Buffer());
        if (levels[i] >= 0) {
            SetLineLeadingIndent(ln, levels[i] * tabSize);
            constructDelta = (levels[i] * tabSize) - oldLead;
        } else if (IsCommentInterior(ln)) {
            int newLead = oldLead + constructDelta;
            SetLineLeadingIndent(ln, (newLead < 0) ? 0 : newLead);
        }
        // else: string interior - leave its bytes untouched.
    }

    UpdateSyntaxForRegion(startY, endExtended + 1);

    // Reposition the caret. A caret that sat in (or at the start of) the old indentation rides to the new
    // start of text; one already within the text keeps its character by shifting with the indent delta.
    auto activeLine = LineAt(activeIdx);
    if (activeLine != nullptr) {
        int newCursorX = oldCursorX;
        if (activeInRange) {
            int newLeadingChars = LeadingWhitespaceCharCount(activeLine->Buffer());
            newCursorX = (oldCursorX <= oldLeadingChars) ? newLeadingChars
                                                         : (oldCursorX + (newLeadingChars - oldLeadingChars));
        }
        if (newCursorX < 0) {
            newCursorX = 0;
        }
        if (newCursorX > (int)activeLine->Length()) {
            newCursorX = (int)activeLine->Length();
        }
        lineCursor.cursor.position.x = newCursorX;
        lineCursor.cursor.wantedColumn = activeLine->CharToVisualColumn(newCursorX, tabSize);
    }
}

void Document::ReplaceLineRange(size_t startY, size_t endY, const std::vector<std::u32string> &newLines) {
    size_t numLines = textBuffer->NumLines();
    if (startY >= numLines) {
        return;
    }
    if (endY >= numLines) {
        endY = numLines - 1;
    }
    if (endY < startY) {
        endY = startY;
    }

    // Snapshot the originals; on undo, delete the newLines.size() replacement lines and restore them.
    auto undoItem = editState->historyBuffer.NewUndoFromLineRange(startY, endY + 1, GetLineCursor(), textBuffer);
    undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kReplaceRange);
    undoItem->SetReplaceCount((int32_t)newLines.size());
    editState->historyBuffer.PushUndoItem(undoItem);

    size_t nOld = (endY - startY) + 1;
    for (size_t i = 0; i < nOld; i++) {
        textBuffer->DeleteLineAt(startY);
    }
    for (size_t i = 0; i < newLines.size(); i++) {
        textBuffer->Insert(startY + i, Line::Create(newLines[i]));
    }

    UpdateSyntaxForRegion(startY, startY + newLines.size());
}

void Document::SurroundLineRangeWithBlock(size_t startY, size_t endY, char32_t open, char32_t close) {
    if (!GetTextBuffer()->HaveLanguage()) {
        return;
    }
    size_t numLines = textBuffer->NumLines();
    if (startY >= numLines) {
        return;
    }
    if (endY >= numLines) {
        endY = numLines - 1;
    }
    if (endY < startY) {
        endY = startY;
    }

    const auto &lines = textBuffer->Lines();
    int tabSize = GetTabSize();
    auto table = IndentCache::Instance().GetTableForLanguage(textBuffer->GetLanguage().GetConfigNodeName()).get();

    // Build the block: opener line, the body lines verbatim, closer line - braces on their own lines so the
    // reindent walk nests them (a line ending in the opener pushes the body in; a line starting with the
    // closer dedents it).
    std::vector<std::u32string> block;
    block.reserve((endY - startY) + 3);
    block.emplace_back(1, open);
    for (size_t y = startY; y <= endY; y++) {
        block.push_back(lines[y]->Buffer());
    }
    block.emplace_back(1, close);

    // Reindent the whole block relative to the trusted line above the range, then apply to the strings.
    int anchorLevel = IndentEngine::FindAnchorLevel(lines, startY, table, tabSize);
    IndentEngine::RangeContext ctx;
    ctx.table = table;
    ctx.tabSize = tabSize;
    ctx.anchorLevel = anchorLevel;
    for (const auto &l : block) {
        ctx.lines.emplace_back(l);
    }
    auto levels = IndentEngine::ReindentRange(ctx);
    for (size_t i = 0; i < block.size(); i++) {
        SetStringLeadingIndent(block[i], levels[i] * tabSize);
    }

    ReplaceLineRange(startY, endY, block);

    // Park the cursor at the end of the closer line (the last line of the new block). The block grew the
    // line count, so scroll the new active line back into view (RefocusViewArea) and write position.y in
    // SCREEN coords (idxActiveLine - viewTopLine) - it is the screen row the caret is drawn at, NOT the
    // absolute line index, so an absolute y would put the caret off-screen on a scrolled view.
    auto &lineCursor = documentViewState->lineCursor;
    lineCursor.idxActiveLine = startY + block.size() - 1;
    auto closeLine = LineAt(lineCursor.idxActiveLine);
    int x = (closeLine != nullptr) ? (int)closeLine->Length() : 0;
    RefocusViewArea();
    lineCursor.cursor.position = { x, (int)lineCursor.idxActiveLine - (int)lineCursor.viewTopLine };
    lineCursor.cursor.wantedColumn = (closeLine != nullptr) ? closeLine->CharToVisualColumn(x, tabSize) : 0;
}

void Document::AddTab() {
    auto line = textBuffer->LineAt(documentViewState->lineCursor.idxActiveLine);
    auto undoItem = BeginUndoItem();

    auto tabSize = textBuffer->GetLanguage().GetTabSize();

    for (int i = 0; i < tabSize; i++) {
        AddCharToLineNoUndo(documentViewState->lineCursor.cursor, line, ' ');
    }
    EndUndoItem(undoItem);
}

void Document::DelTab() {
    auto line = textBuffer->LineAt(documentViewState->lineCursor.idxActiveLine);
    auto nDel = textBuffer->GetLanguage().GetTabSize();
    if(documentViewState->lineCursor.cursor.position.x < nDel) {
        nDel = documentViewState->lineCursor.cursor.position.x;
    }
    auto undoItem = BeginUndoItem();
    for (int i = 0; i < nDel; i++) {
        RemoveCharFromLineNoUndo(documentViewState->lineCursor.cursor, line);
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

    auto ptWhere = documentViewState->lineCursor.cursor.position;
    ptWhere.y += (int)documentViewState->lineCursor.viewTopLine;

    UndoHistory::UndoItem::Ref undoItem;
    if (linesAdded == 0) {
        // In-place splice on a single line: snapshot and restore just that one line.
        undoItem = BeginUndoFromLineRange(documentViewState->lineCursor.idxActiveLine, documentViewState->lineCursor.idxActiveLine + 1);
        undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kClearAndAppend);
    } else {
        // Multi-line splice: the paste inserts 'linesAdded' new lines; undo deletes them and
        // restores the original target line.
        undoItem = BeginUndoFromLineRange(documentViewState->lineCursor.idxActiveLine, documentViewState->lineCursor.idxActiveLine + linesAdded);
        undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kDeleteBeforeInsert);
    }

    auto ptEnd = clipboard.PasteToBuffer(textBuffer, ptWhere);

    EndUndoItem(undoItem);

    // Reparse every line the splice touched (target line through the last inserted line).
    textBuffer->ReparseRegion(documentViewState->lineCursor.idxActiveLine, documentViewState->lineCursor.idxActiveLine + linesAdded + 1);

    // Land the caret at the end of the pasted text (PasteToBuffer reports where that is).
    documentViewState->lineCursor.idxActiveLine += linesAdded;
    documentViewState->lineCursor.cursor.position.y += (int)linesAdded;
    documentViewState->lineCursor.cursor.position.x = ptEnd.x;
    CaptureWantedColumn(documentViewState->lineCursor.cursor, ActiveLine());
}
