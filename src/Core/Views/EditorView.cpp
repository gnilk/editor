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
#include <fmt/core.h>

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


    // This is the visible area...
    viewTopLine = 0;
    viewBottomLine = rect.Height();
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

    HandleResize(editorModel->cursor, viewRect);
    UpdateModelFromNavigation(true);

}

void EditorView::OnResized() {
    // Update the view Bottom line - as this affects how many lines we draw...
    viewBottomLine = GetContentRect().Height();
    UpdateModelFromNavigation(true);
    ViewBase::OnResized();
}

void EditorView::DrawViewContents() {
    // Nothing to draw we don't have a model...
    if (editorModel == nullptr) {
        return;
    }

    auto &dc = window->GetContentDC(); //ViewBase::ContentAreaDrawContext();
    logger->Debug("DrawViewContents, dc Height=%d, topLine=%d, bottomLine=%d", dc.GetRect().Height(), editorModel->viewTopLine, editorModel->viewBottomLine);

    auto selection = editorModel->GetSelection();
    dc.ClearOverlays();

    // Add in the result from search if any...
    if (editorModel->searchResults.size() > 0) {
        logger->Debug("Overlays from search results");
        logger->Debug("  SR0: x=%d, len=%d line=%d", editorModel->searchResults[0].cursor_x, editorModel->searchResults[0].length, editorModel->searchResults[0].idxLine);
        for(auto &result : editorModel->searchResults) {
            // Perhaps a have a function to translate this - surprised I don't have one..
            if ((result.idxLine >= editorModel->viewTopLine) && (result.idxLine < editorModel->viewBottomLine)) {
                DrawContext::Overlay overlay;
                auto yPos = result.idxLine - editorModel->viewTopLine;
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
        dc.AddOverlay(overlay);

        // ---- start test
        // Test overlay transform

//        logger->Debug("Transform overlay from:");
//        logger->Debug("  (%d:%d) - (%d:%d)", overlay.start.x, overlay.start.y, overlay.end.x, overlay.end.y);
//        logger->Debug("  viewTopLine: %d, bottomLine: %d", editorModel->viewTopLine, editorModel->viewBottomLine);

        int dy = selection.GetEnd().y - selection.GetStart().y;
//        logger->Debug("dy = %d", dy);
        overlay.start.y -= editorModel->viewTopLine;
        overlay.end.y = overlay.start.y + dy;

        dc.AddOverlay(overlay);

        logger->Debug("Transform overlay to:");
        logger->Debug("  (%d:%d) - (%d:%d)", overlay.start.x, overlay.start.y, overlay.end.x, overlay.end.y);
        // ---- End test

    }

    LineRender lineRender(dc);
    // Consider refactoring this function call...
    lineRender.DrawLines(editorModel->GetEditController()->Lines(), editorModel->viewTopLine, editorModel->viewBottomLine, editorModel->GetSelection());
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

    // Let the controller have a go - this is regular editing and so forth
    if (editorModel->GetEditController()->HandleKeyPress(editorModel->cursor, idxActiveLine, keyPress)) {
        UpdateModelFromNavigation(true);
        InvalidateView();
        return;
    }

    // This handles regular backspace/delete/home/end (which are default actions for any single-line editing)
    if (editorModel->GetEditController()->HandleSpecialKeyPress(editorModel->cursor, idxActiveLine, keyPress)) {
        UpdateModelFromNavigation(true);
        InvalidateView();
        return;
    }


    // It was not to us..
    ViewBase::OnKeyPress(keyPress);
}

void EditorView::HandleKeyPressWithSelection(const KeyPress &keyPress) {

    auto &selection = editorModel->GetSelection();
    idxActiveLine = selection.GetStart().y;
    editorModel->cursor.position = selection.GetStart();
    editorModel->cursor.position.y -= viewTopLine;   // Translate to screen coords..

    // Save here - because 'UpdateModelFromNavigiation' updates the wanted column - bad/good?
    auto tmpCursor = editorModel->cursor;
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
            if (editorModel->GetEditController()->HandleKeyPress(editorModel->cursor, idxActiveLine, keyPress)) {
                // revert the last insert
                editorModel->GetEditController()->Undo(editorModel->cursor);
                // delete the selection (buffer is now fine)
                editorModel->DeleteSelection();

                // Restore the cursor where it should be and repeat the keypress handling again...
                editorModel->cursor = tmpCursor;
                editorModel->GetEditController()->HandleKeyPress(editorModel->cursor, idxActiveLine, keyPress);
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

        idxActiveLine = selection.GetStart().y;
        editorModel->cursor.position = selection.GetStart();
        editorModel->cursor.position.y -= viewTopLine;   // Translate to screen coords..

        editorModel->DeleteSelection();
        editorModel->CancelSelection();
        UpdateModelFromNavigation(false);

    } else if (kpAction.action == kAction::kActionPasteFromClipboard) {
        logger->Debug("Paste from clipboard");
        auto &clipboard = Editor::Instance().GetClipBoard();
        if (clipboard.Top() != nullptr) {
            auto nLines = clipboard.Top()->GetLineCount();
            auto ptWhere = editorModel->cursor.position;
            ptWhere.y += (int)viewTopLine;
            clipboard.PasteToBuffer(editorModel->GetTextBuffer(), ptWhere);

            editorModel->GetTextBuffer()->ReparseRegion(editorModel->idxActiveLine, editorModel->idxActiveLine + nLines);
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
    editorModel->GetEditController()->Undo(editorModel->cursor);
    return true;
}

bool EditorView::OnActionLineHome() {
    editorModel->cursor.position.x = 0;
    editorModel->cursor.wantedColumn = 0;
    return true;
}

bool EditorView::OnActionLineEnd() {
    auto currentLine = editorModel->GetEditController()->LineAt(editorModel->idxActiveLine);
    if (currentLine == nullptr) {
        return true;
    }
    auto endpos = currentLine->Length();
    editorModel->cursor.position.x = endpos;
    editorModel->cursor.wantedColumn = endpos;
    return true;
}

bool EditorView::OnActionCommitLine() {
    logger->Debug("OnActionCommitLine, Before: idxActive=%zu", idxActiveLine);
    editorModel->GetEditController()->NewLine(editorModel->idxActiveLine, editorModel->cursor);
    OnNavigateDownVSCode(editorModel->cursor, 1, viewRect, editorModel->Lines().size());
    UpdateModelFromNavigation(true);
    logger->Debug("OnActionCommitLine, After: idxActive=%zu", idxActiveLine);

    InvalidateView();
    return true;
}

bool EditorView::OnActionWordRight() {
    auto currentLine = editorModel->GetEditController()->LineAt(editorModel->idxActiveLine);
    auto attrib = currentLine->AttributeAt(editorModel->cursor.position.x);
    attrib++;
    editorModel->cursor.position.x = attrib->idxOrigString;

    return true;
}

bool EditorView::OnActionWordLeft() {
    auto currentLine = editorModel->GetEditController()->LineAt(editorModel->idxActiveLine);
    auto attrib = currentLine->AttributeAt(editorModel->cursor.position.x);
    if (editorModel->cursor.position.x == attrib->idxOrigString) {
        attrib--;
    }
    editorModel->cursor.position.x = attrib->idxOrigString;
    return true;
}

bool EditorView::OnActionGotoFirstLine() {
    logger->Debug("GotoFirstLine (def: CMD+Home), resetting cursor and view data!");
    editorModel->cursor.position.x = 0;
    editorModel->cursor.position.y = 0;
    editorModel->idxActiveLine = 0;
    editorModel->viewTopLine = 0;
    editorModel->viewBottomLine = GetContentRect().Height();

    return true;
}
bool EditorView::OnActionGotoLastLine() {
    logger->Debug("GotoLastLine (def: CMD+End), set cursor to last line!");

    editorModel->cursor.position.x = 0;
    editorModel->cursor.position.y = GetContentRect().Height()-1;
    editorModel->idxActiveLine = editorModel->Lines().size()-1;
    editorModel->viewBottomLine = editorModel->Lines().size();
    editorModel->viewTopLine = editorModel->viewBottomLine - GetContentRect().Height();

    return true;
}


bool EditorView::OnActionStepLeft() {
    editorModel->cursor.position.x--;
    if (editorModel->cursor.position.x < 0) {
        editorModel->cursor.position.x = 0;
    }
    editorModel->cursor.wantedColumn = editorModel->cursor.position.x;
    return true;
}
bool EditorView::OnActionStepRight() {
    auto currentLine = editorModel->GetEditController()->LineAt(editorModel->idxActiveLine);
    editorModel->cursor.position.x++;
    if (editorModel->cursor.position.x > (int)currentLine->Length()) {
        editorModel->cursor.position.x = (int)currentLine->Length();
    }
    editorModel->cursor.wantedColumn = editorModel->cursor.position.x;
    return true;
}

void EditorView::UpdateModelFromNavigation(bool updateCursor) {
    if (editorModel == nullptr) {
        return;
    }
    editorModel->idxActiveLine = idxActiveLine;
    editorModel->viewTopLine = viewTopLine;
    editorModel->viewBottomLine = viewBottomLine;

    if (!updateCursor) {
        return;
    }

    auto currentLine = editorModel->LineAt(editorModel->idxActiveLine);
    editorModel->cursor.position.x = editorModel->cursor.wantedColumn;
    if (editorModel->cursor.position.x > (int) currentLine->Length()) {
        editorModel->cursor.position.x = (int) currentLine->Length();
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
        OnNavigateDownVSCode(editorModel->cursor, viewRect.Height() - 1, viewRect, editorModel->Lines().size());
    } else {
        OnNavigateDownCLion(editorModel->cursor, viewRect.Height() - 1, viewRect, editorModel->Lines().size());
    }
    UpdateModelFromNavigation(true);
    return true;
}

bool EditorView::OnActionPageUp() {
    if (!bUseCLionPageNav) {
        OnNavigateUpVSCode(editorModel->cursor, viewRect.Height() - 1, viewRect, editorModel->Lines().size());
    } else {
        OnNavigateUpCLion(editorModel->cursor, viewRect.Height() - 1, viewRect, editorModel->Lines().size());
    }
    UpdateModelFromNavigation(true);
    return true;
}

bool EditorView::OnActionLineDown(const KeyPressAction &kpAction) {
    auto currentLine = editorModel->GetEditController()->LineAt(editorModel->idxActiveLine);
    if (currentLine == nullptr) {
        return true;
    }
    OnNavigateDownVSCode(editorModel->cursor, 1, viewRect, editorModel->Lines().size());
    UpdateModelFromNavigation(true);
    return true;
}
bool EditorView::OnActionLineUp() {
    auto currentLine = editorModel->GetEditController()->LineAt(editorModel->idxActiveLine);
    if (currentLine == nullptr) {
        return true;
    }
//    OnNavigateUpVSCode(1);
    OnNavigateUpVSCode(editorModel->cursor, 1, viewRect, editorModel->Lines().size());

//    currentLine = editorModel->LineAt(editorModel->idxActiveLine);
//    editorModel->cursor.position.x = editorModel->cursor.wantedColumn;
//    if (editorModel->cursor.position.x > (int)currentLine->Length()) {
//        editorModel->cursor.position.x = (int)currentLine->Length();
//    }
    UpdateModelFromNavigation(true);
    return true;
}
bool EditorView::OnActionGotoTopLine() {
    logger->Debug("GotoTopLine (def: PageUp+CMDKey) cursor=(%d:%d)", editorModel->cursor.position.x, editorModel->cursor.position.y);
    editorModel->cursor.position.y = 0;
    editorModel->idxActiveLine = editorModel->viewTopLine;
    logger->Debug("GotoTopLine, new cursor=(%d:%d)", editorModel->cursor.position.x, editorModel->cursor.position.y);
    return true;
}
bool EditorView::OnActionGotoBottomLine() {
    logger->Debug("GotoBottomLine (def: PageDown+CMDKey), cursor=(%d:%d)", editorModel->cursor.position.x, editorModel->cursor.position.y);
    editorModel->cursor.position.y = GetContentRect().Height()-1;
    editorModel->idxActiveLine = editorModel->viewBottomLine-1;
    logger->Debug("GotoBottomLine, new  cursor=(%d:%d)", editorModel->cursor.position.x, editorModel->cursor.position.y);
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
        window->SetCursor(editorModel->cursor);
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
    auto activeLine = model->GetTextBuffer()->LineAt(model->idxActiveLine);
    char tmp[32];

    // FIXME: Need a better snprintf for std::<t>string
    // use: fmt?

    //auto strtmp = fmt::format(U"l: {}, c({}:{})", strutil::itou32(idxActiveLine),strutil::itou32(model->cursor.position.x), strutil::itou32(model->cursor.position.x));
    auto strtmp = U"l: " + strutil::itou32(idxActiveLine) + U", c(" + strutil::itou32(model->cursor.position.x) + U":" + strutil::itou32(model->cursor.position.x) + U")";


//    snprintf(tmp,32,"l: %zu, c(%d:%d)",
//             idxActiveLine,
//             model->cursor.position.x, model->cursor.position.y);

//    snprintf(tmp, 32, "Id: %d, Ln: %d, Col: %d",
//             activeLine==nullptr?0:activeLine->Indent(),        // Line can be nullptr for 0 byte files..
//             model->cursor.position.y,
//             model->cursor.position.x);
    statusRight += strtmp;


    return {statusCenter, statusRight};
}
