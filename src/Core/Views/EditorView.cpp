//
// Created by gnilk on 14.02.23.
//
#include <unordered_map>

#include "Core/Action.h"
#include "Core/Config/Config.h"
#include "Core/DrawContext.h"
#include "Core/EditorModel.h"
#include "Core/KeyMapping.h"
#include "Core/RuntimeConfig.h"
#include "Core/LineRender.h"
#include "EditorView.h"
#include "Core/Editor.h"
#include "Core/ActionHelper.h"


#include "fmt/xchar.h"

using namespace gedit;

// This is the global section in the config.yml for this view
static const std::string cfgSectionName = "editorview";

void EditorView::InitView()  {
    logger = gnilk::Logger::GetLogger("EditorView");
    logger->Debug("InitView!");

    auto screen = RuntimeConfig::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    window->SetCaption("EditorView");

    auto &rect = window->GetContentDC().GetRect();

    bUseCLionPageNav = Config::Instance()[cfgSectionName].GetBool("pgupdown_content_first", true);

    editorModel = Editor::Instance().GetActiveModel();
    if (editorModel == nullptr) {
        logger->Error("EditorModel is null - no active textbuffer");
        return;
    }
    lineCursor = editorModel->GetLineCursorRef();

    // This is the visible area...
    lineCursor->viewTopLine = 0;
    lineCursor->viewBottomLine = rect.Height();

    UpdateModelFromNavigation(true);

    editorModel->GetEditController()->SetTextBufferChangedHandler([this]()->void {
        auto node = Editor::Instance().GetWorkspace()->GetNodeFromModel(editorModel);
        if (node == nullptr) {
            return;
        }
        window->SetCaption(node->GetDisplayName());
//       auto textBuffer = editorModel->GetEditController()->GetTextBuffer();
//       window->SetCaption(textBuffer->GetName());
    });
}

void EditorView::ReInitView() {
    auto screen = RuntimeConfig::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    auto &rect = window->GetContentDC().GetRect();

    bUseCLionPageNav = Config::Instance()[cfgSectionName].GetBool("pgupdown_content_first", true);

    editorModel = Editor::Instance().GetActiveModel();
    if (editorModel == nullptr) {
        logger->Error("EditorModel is null - no active textbuffer");
        return;
    }

    // Fetch and update the view-model information
    lineCursor = editorModel->GetLineCursorRef();
    HandleResize(viewRect);
    UpdateModelFromNavigation(true);

}

void EditorView::OnResized() {
    // Update the view Bottom line - as this affects how many lines we draw...
    //viewBottomLine = GetContentRect().Height();
    auto &lineCursor = editorModel->GetLineCursor();
    lineCursor.viewBottomLine = GetContentRect().Height();
    if (!lineCursor.IsInside(lineCursor.idxActiveLine)) {
        // FIXME: Snap to either first or last...
    }


    UpdateModelFromNavigation(true);
    ViewBase::OnResized();
}

void EditorView::DrawViewContents() {
    // Nothing to draw we don't have a model...
    if (editorModel == nullptr) {
        return;
    }

    auto &dc = window->GetContentDC(); //ViewBase::ContentAreaDrawContext();
    auto &selection = editorModel->GetSelection();
    auto &lineCursor = editorModel->GetLineCursor();

    logger->Debug("DrawViewContents, dc Height=%d, topLine=%d, bottomLine=%d",
                  dc.GetRect().Height(), lineCursor.viewTopLine, lineCursor.viewBottomLine);

    dc.ClearOverlays();


    // Add in the result from search if any...
    if (editorModel->searchResults.size() > 0) {
        logger->Debug("Overlays from search results");
        logger->Debug("  SR0: x=%d, len=%d line=%d", editorModel->searchResults[0].cursor_x, editorModel->searchResults[0].length, editorModel->searchResults[0].idxLine);
        for(auto &result : editorModel->searchResults) {
            // Perhaps a have a function to translate this - surprised I don't have one..

//            if ((result.idxLine >= lineCursor.viewTopLine) && (result.idxLine <lineCursor.viewBottomLine)) {
            if (lineCursor.IsInside(result.idxLine)) {
                DrawContext::Overlay overlay;
                auto yPos = result.idxLine - lineCursor.viewTopLine;
                overlay.Set(Point(result.cursor_x, yPos),
                            Point(result.cursor_x + result.length, yPos));
                overlay.isActive = true;
                dc.AddOverlay(overlay);
            }
        }
    }

    if (selection.IsActive()) {
        DrawContext::Overlay overlay;
        overlay.Set(selection.GetStart(), selection.GetEnd());
        overlay.attributes = 0;     // ??
        overlay.isActive = true;

        int dy = selection.GetEnd().y - selection.GetStart().y;
        overlay.start.y -= lineCursor.viewTopLine;
        overlay.end.y = overlay.start.y + dy;

        dc.AddOverlay(overlay);

        logger->Debug("Transform overlay to:");
        logger->Debug("  (%d:%d) - (%d:%d)", overlay.start.x, overlay.start.y, overlay.end.x, overlay.end.y);
        // ---- End test

    }

    LineRender lineRender(dc);
    // Consider refactoring this function call...
    lineRender.DrawLines(editorModel->GetEditController()->Lines(),
                         lineCursor.viewTopLine,
                         lineCursor.viewBottomLine,
                         editorModel->GetSelection());
}

void EditorView::OnActivate(bool isActive) {
    logger->Debug("OnActive, isActive: %s", isActive?"yes":"no");
    if (isActive) {
        Editor::Instance().SetActiveKeyMapping(Config::Instance()[cfgSectionName].GetStr("keymap", "default_keymap"));
        // Maximize editor content view...
        MaximizeContentHeight();
    }
}

void EditorView::OnKeyPress(const KeyPress &keyPress) {
    if (editorModel == nullptr) {
        return;
    }
    // Unless we can edit - we do nothing
    if (!editorModel->GetTextBuffer()->CanEdit()) return;

    // In case we have selection active - we treat the whole thing a bit differently...
    if (editorModel->IsSelectionActive()) {
        HandleKeyPressWithSelection(keyPress);
        InvalidateView();
        return;
    }

    auto &lineCursor = editorModel->GetLineCursor();

    // Let the controller have a go - this is regular editing and so forth
    if (editorModel->GetEditController()->HandleKeyPress(lineCursor.cursor, lineCursor.idxActiveLine, keyPress)) {
        UpdateModelFromNavigation(true);
        InvalidateView();
        return;
    }

    // This handles regular backspace/delete/home/end (which are default actions for any single-line editing)
    if (editorModel->GetEditController()->HandleSpecialKeyPress(lineCursor.cursor, lineCursor.idxActiveLine, keyPress)) {
        UpdateModelFromNavigation(true);
        InvalidateView();
        return;
    }


    // It was not to us..
    ViewBase::OnKeyPress(keyPress);
}

void EditorView::HandleKeyPressWithSelection(const KeyPress &keyPress) {

    auto &selection = editorModel->GetSelection();
    auto &lineCursor = editorModel->GetLineCursor();

    lineCursor.idxActiveLine = selection.GetStart().y;
    lineCursor.cursor.position = selection.GetStart();
    lineCursor.cursor.position.y -= lineCursor.viewTopLine;   // Translate to screen coords..

    // Save here - because 'UpdateModelFromNavigiation' updates the wanted column - bad/good?
    auto tmpCursor = lineCursor.cursor;

    // FIXME: This?!?!?!?!?
    UpdateModelFromNavigation(false);


    switch (keyPress.specialKey) {
        case Keyboard::kKeyCode_Backspace :
        case Keyboard::kKeyCode_DeleteForward :
            editorModel->DeleteSelection();
            break;
        case Keyboard::kKeyCode_Tab :
            // Handle this - indent!
            break;
        default: {
            // This is a bit ugly (understatement of this project so far...)
            // But any - valid - keypress should lead to the selection being deleted and the new key inserted...
            if (editorModel->GetEditController()->HandleKeyPress(lineCursor.cursor, lineCursor.idxActiveLine, keyPress)) {
                // revert the last insert
                editorModel->GetEditController()->Undo(lineCursor.cursor);
                // delete the selection (buffer is now fine)
                editorModel->DeleteSelection();

                // Restore the cursor where it should be and repeat the keypress handling again...
                lineCursor.cursor = tmpCursor;
                editorModel->GetEditController()->HandleKeyPress(lineCursor.cursor, lineCursor.idxActiveLine, keyPress);
            }
        }
    }

    // Regardless of the hacky thing above - let's cancel out the selection...
    editorModel->CancelSelection();
    UpdateModelFromNavigation(false);
}

//
// Add actions here - all except human-readable inserting of text
//
bool EditorView::OnAction(const KeyPressAction &kpAction) {
    if (editorModel == nullptr) {
        return false;
    }

    auto &lineCursor = editorModel->GetLineCursor();

    if (kpAction.actionModifier == kActionModifier::kActionModifierSelection) {
        if (!editorModel->IsSelectionActive()) {
            logger->Debug("Shift pressed, selection inactive - BeginSelection");
            editorModel->BeginSelection();
        }
    }

    // This is convoluted - will be dealt with when copy/paste works...
    if (kpAction.action == kAction::kActionCopyToClipboard) {
        logger->Debug("Set text to clipboard");
        auto selection = editorModel->GetSelection();

        auto &clipboard = Editor::Instance().GetClipBoard();
        clipboard.CopyFromBuffer(editorModel->GetTextBuffer(), selection.GetStart(), selection.GetEnd());

    } else if (kpAction.action == kAction::kActionCutToClipboard) {
        logger->Debug("Cut text to clipboard");
        auto selection = editorModel->GetSelection();
        auto &clipboard = Editor::Instance().GetClipBoard();
        clipboard.CopyFromBuffer(editorModel->GetTextBuffer(), selection.GetStart(), selection.GetEnd());

        lineCursor.idxActiveLine = selection.GetStart().y;
        lineCursor.cursor.position = selection.GetStart();
        lineCursor.cursor.position.y -= lineCursor.viewTopLine;   // Translate to screen coords..

        editorModel->DeleteSelection();
        editorModel->CancelSelection();
        UpdateModelFromNavigation(false);

    } else if (kpAction.action == kAction::kActionPasteFromClipboard) {
        logger->Debug("Paste from clipboard");
        auto &clipboard = Editor::Instance().GetClipBoard();
        if (clipboard.Top() != nullptr) {
            auto nLines = clipboard.Top()->GetLineCount();
            auto ptWhere = lineCursor.cursor.position;
            ptWhere.y += (int)lineCursor.viewTopLine;
            clipboard.PasteToBuffer(editorModel->GetTextBuffer(), ptWhere);

            editorModel->GetTextBuffer()->ReparseRegion(lineCursor.idxActiveLine, lineCursor.idxActiveLine + nLines);
        }
    } else if (kpAction.action == kAction::kActionInsertLineComment) {
        // Handle this here since we want to keep the selection...
        editorModel->CommentSelectionOrLine();
    }


    auto result = DispatchAction(kpAction);

    // FIXME: Not sure this is the correct thing to do...
    if ((kpAction.actionModifier != kActionModifier::kActionModifierSelection) && result && editorModel->IsSelectionActive()) {
        editorModel->CancelSelection();
    }

    // Update with cursor after navigation (if any happened)
    if (editorModel->IsSelectionActive()) {
        editorModel->UpdateSelection();
        logger->Debug(" Selection is Active, start=(%d:%d), end=(%d:%d)",
                      editorModel->GetSelection().GetStart().x, editorModel->GetSelection().GetStart().y,
                      editorModel->GetSelection().GetEnd().x, editorModel->GetSelection().GetEnd().y);
    }

    return result;
}
bool EditorView::DispatchAction(const KeyPressAction &kpAction) {
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
        case kAction::kActionCycleActiveBufferNext :
            return OnActionNextBuffer();
        case kAction::kActionCycleActiveBufferPrev :
            return OnActionPreviousBuffer();
        case kAction::kActionCycleActiveEditor :
            return OnActionCycleActiveBuffer();
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

bool EditorView::OnActionUndo() {
    //editorModel->GetTextBuffer()->Undo();
    editorModel->GetEditController()->Undo(editorModel->GetCursor());
    return true;
}

bool EditorView::OnActionLineHome() {
    auto &lineCursor = editorModel->GetLineCursor();
    lineCursor.cursor.position.x = 0;
    lineCursor.cursor.wantedColumn = 0;
    return true;
}

bool EditorView::OnActionLineEnd() {
    auto &lineCursor = editorModel->GetLineCursor();
    auto currentLine = editorModel->GetEditController()->LineAt(lineCursor.idxActiveLine);
    if (currentLine == nullptr) {
        return true;
    }
    auto endpos = currentLine->Length();
    lineCursor.cursor.position.x = endpos;
    lineCursor.cursor.wantedColumn = endpos;
    return true;
}

bool EditorView::OnActionCommitLine() {
    auto &lineCursor = editorModel->GetLineCursor();
    logger->Debug("OnActionCommitLine, Before: idxActive=%zu", lineCursor.idxActiveLine);
    editorModel->GetEditController()->NewLine(lineCursor.idxActiveLine, lineCursor.cursor);
    OnNavigateDownVSCode(1, viewRect, editorModel->Lines().size());
    UpdateModelFromNavigation(true);
    logger->Debug("OnActionCommitLine, After: idxActive=%zu", lineCursor.idxActiveLine);

    InvalidateView();
    return true;
}

bool EditorView::OnActionWordRight() {
    auto currentLine = editorModel->ActiveLine();
    auto &cursor = editorModel->GetCursor();
    auto attrib = currentLine->AttributeAt(cursor.position.x);
    attrib++;
    cursor.position.x = attrib->idxOrigString;

    return true;
}

bool EditorView::OnActionWordLeft() {
    auto currentLine = editorModel->ActiveLine(); //editorModel->GetEditController()->LineAt(editorModel->idxActiveLine);
    auto &cursor = editorModel->GetCursor();
    auto attrib = currentLine->AttributeAt(cursor.position.x);
    if (cursor.position.x == attrib->idxOrigString) {
        attrib--;
    }
    cursor.position.x = attrib->idxOrigString;
    return true;
}

bool EditorView::OnActionGotoFirstLine() {
    logger->Debug("GotoFirstLine (def: CMD+Home), resetting cursor and view data!");
    auto &lineCursor = editorModel->GetLineCursor();
    lineCursor.cursor.position.x = 0;
    lineCursor.cursor.position.y = 0;
    lineCursor.idxActiveLine = 0;
    lineCursor.viewTopLine = 0;
    lineCursor.viewBottomLine = GetContentRect().Height();

    return true;
}
bool EditorView::OnActionGotoLastLine() {
    logger->Debug("GotoLastLine (def: CMD+End), set cursor to last line!");
    auto &lineCursor = editorModel->GetLineCursor();
    lineCursor.cursor.position.x = 0;
    lineCursor.cursor.position.y = GetContentRect().Height()-1;
    lineCursor.idxActiveLine = editorModel->Lines().size()-1;
    lineCursor.viewBottomLine = editorModel->Lines().size();
    lineCursor.viewTopLine = lineCursor.viewBottomLine - GetContentRect().Height();

    logger->Debug("Cursor: %d:%d, idxActiveLine: %d",lineCursor.cursor.position.x, lineCursor.cursor.position.y, lineCursor.idxActiveLine);

    return true;
}


bool EditorView::OnActionStepLeft() {
    auto &cursor = editorModel->GetCursor();
    cursor.position.x--;
    if (cursor.position.x < 0) {
        cursor.position.x = 0;
    }
    cursor.wantedColumn = cursor.position.x;
    return true;
}
bool EditorView::OnActionStepRight() {
    auto currentLine = editorModel->ActiveLine();
    auto &cursor = editorModel->GetCursor();
    cursor.position.x++;
    if (cursor.position.x > (int)currentLine->Length()) {
        cursor.position.x = (int)currentLine->Length();
    }
    cursor.wantedColumn = cursor.position.x;
    return true;
}

// Not sure this should be here
void EditorView::UpdateModelFromNavigation(bool updateCursor) {
    if (editorModel == nullptr) {
        return;
    }

    if (!updateCursor) {
        return;
    }

    auto lineCursor = editorModel->GetLineCursor();

    auto currentLine = editorModel->LineAt(lineCursor.idxActiveLine);
    if (currentLine == nullptr) {
        lineCursor.cursor.position.x = 0;
        lineCursor.cursor.position.y = 0;
        lineCursor.cursor.wantedColumn = 0;
        return;
    }

    // ????
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

bool EditorView::OnActionPageDown() {

    if (!bUseCLionPageNav) {
        OnNavigateDownVSCode(viewRect.Height() - 1, viewRect, editorModel->Lines().size());
    } else {
        OnNavigateDownCLion(viewRect.Height() - 1, viewRect, editorModel->Lines().size());
    }
    UpdateModelFromNavigation(true);
    return true;
}

bool EditorView::OnActionPageUp() {
    if (!bUseCLionPageNav) {
        OnNavigateUpVSCode(viewRect.Height() - 1, viewRect, editorModel->Lines().size());
    } else {
        OnNavigateUpCLion(viewRect.Height() - 1, viewRect, editorModel->Lines().size());
    }
    UpdateModelFromNavigation(true);
    return true;
}

bool EditorView::OnActionLineDown(const KeyPressAction &kpAction) {
    auto currentLine = editorModel->ActiveLine();
    if (currentLine == nullptr) {
        return true;
    }
    auto &lineCursor = editorModel->GetLineCursor();
    auto &cursor = editorModel->GetCursor();
    OnNavigateDownVSCode(1, viewRect, editorModel->Lines().size());
    UpdateModelFromNavigation(true);

    return true;
}
bool EditorView::OnActionLineUp() {
    auto currentLine = editorModel->ActiveLine();
    if (currentLine == nullptr) {
        return true;
    }

    OnNavigateUpVSCode(1, viewRect, editorModel->Lines().size());
    UpdateModelFromNavigation(true);
    return true;
}
bool EditorView::OnActionGotoTopLine() {
    auto &lineCursor = editorModel->GetLineCursor();

    lineCursor.cursor.position.y = 0;
    lineCursor.idxActiveLine = lineCursor.viewTopLine;
    //logger->Debug("GotoTopLine, new cursor=(%d:%d)", editorModel->cursor.position.x, editorModel->cursor.position.y);
    return true;
}
bool EditorView::OnActionGotoBottomLine() {
    //logger->Debug("GotoBottomLine (def: PageDown+CMDKey), cursor=(%d:%d)", editorModel->cursor.position.x, editorModel->cursor.position.y);

    auto &lineCursor = editorModel->GetLineCursor();
    lineCursor.cursor.position.y = GetContentRect().Height()-1;
    lineCursor.idxActiveLine = lineCursor.viewBottomLine-1;

    //logger->Debug("GotoBottomLine, new  cursor=(%d:%d)", editorModel->cursor.position.x, editorModel->cursor.position.y);
    return true;
}

bool EditorView::OnActionNextBuffer() {
    ActionHelper::SwitchToNextBuffer();
    InvalidateAll();
    return true;
}

bool EditorView::OnActionPreviousBuffer() {
    ActionHelper::SwitchToPreviousBuffer();
    InvalidateAll();
    return true;
}

bool EditorView::OnActionCycleActiveBuffer() {
    auto idxCurrent = Editor::Instance().GetActiveModelIndex();
    auto idxNext = Editor::Instance().NextModelIndex(idxCurrent);
    if (idxCurrent == idxNext) {
        return true;
    }
    Editor::Instance().SetActiveModelFromIndex(idxNext);
//    auto nextModel = Editor::Instance().GetModelFromIndex(idxNext);
//    RuntimeConfig::Instance().SetActiveEditorModel(nextModel);
//    RuntimeConfig::Instance().GetRootView().Initialize();
    InvalidateAll();
    return true;
}
bool EditorView::OnNextSearchResult() {
    if (!editorModel->HaveSearchResults()) {
        return false;
    }
    editorModel->NextSearchResult();
    return true;
}
bool EditorView::OnPrevSearchResult() {
    if (!editorModel->HaveSearchResults()) {
        return false;
    }
    editorModel->PrevSearchResult();
    return true;
}

void EditorView::SetWindowCursor(const Cursor &cursor) {
    if ((Editor::Instance().GetState() == Editor::ViewState) && (editorModel != nullptr)) {
        window->SetCursor(editorModel->GetCursor());
    } else {
        // The editor view is NOT in 'command' but rather the 'owner' of the quick-cmd input view..
        ViewBase::SetWindowCursor(cursor);
    }
}

// returns the center and right side information for the status bar
std::pair<std::u32string, std::u32string> EditorView::GetStatusBarInfo() {
    std::u32string statusCenter = U"";
    std::u32string statusRight = U"";
    // If we have a model - draw details...
    auto node = Editor::Instance().GetWorkspaceNodeForActiveModel();
    if (node == nullptr) {
        return {statusCenter, statusRight};
    }
    // Hmm - why can't I use the editorModel-> here????
    auto model = node->GetModel();
    if (model == nullptr) {
        return {statusCenter, statusRight};
    }

    // Resolve center information
    if (model->GetTextBuffer()->GetBufferState() == TextBuffer::kBuffer_Changed) {
        statusCenter += U"* ";
    }
    if (model->GetTextBuffer()->IsReadOnly()) {
        statusCenter += U"R/O ";
    }

    statusCenter += node->GetDisplayNameU32();
    statusCenter += U" | ";
    if (!model->GetTextBuffer()->CanEdit()) {
        statusCenter += U"[locked] | ";
    }
    statusCenter += model->GetTextBuffer()->HaveLanguage() ? model->GetTextBuffer()->GetLanguage().Identifier() : U"none";

    // resolve right status
    auto &lineCursor = model->GetLineCursor();

    // Show Line:Row or more 'x/y' -> Configureation!
    auto strtmp = fmt::format(U"l: {}, c({}:{})",
                              strutil::itou32(lineCursor.idxActiveLine),
                              strutil::itou32(lineCursor.cursor.position.y),
                              strutil::itou32(lineCursor.cursor.position.x));

    statusRight += strtmp;

    return {statusCenter, statusRight};
}
