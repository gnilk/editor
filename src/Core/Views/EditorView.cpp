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
// TEMP
#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_error.h>

using namespace gedit;

void EditorView::InitView()  {
    logger = gnilk::Logger::GetLogger("EditorView");
    logger->Debug("InitView!");

    auto screen = RuntimeConfig::Instance().Screen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    window->SetCaption("EditorView");

    auto &rect = window->GetContentDC().GetRect();

    editorModel = RuntimeConfig::Instance().ActiveEditorModel();
    if (editorModel == nullptr) {
        logger->Error("EditorModel is null - no active textbuffer");
        exit(1);
    }

    // This is the visible area...
    editorModel->viewTopLine = 0;
    editorModel->viewBottomLine = rect.Height();
    editorModel->GetEditController()->SetTextBufferChangedHandler([this]()->void {
       auto textBuffer = editorModel->GetEditController()->GetTextBuffer();
       window->SetCaption(textBuffer->GetName());
    });


    bUseCLionPageNav = Config::Instance()["editor"].GetBool("pgupdown_content_first", true);
}

void EditorView::ReInitView() {
    auto screen = RuntimeConfig::Instance().Screen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    auto &rect = window->GetContentDC().GetRect();

    editorModel = RuntimeConfig::Instance().ActiveEditorModel();
    if (editorModel == nullptr) {
        logger->Error("EditorModel is null - no active textbuffer");
        exit(1);
    }

    logger->Debug("ReInitView, current model: %s", editorModel->GetTextBuffer()->GetName().c_str());

    // This is the visible area...
    editorModel->viewTopLine = 0;
    editorModel->viewBottomLine = rect.Height();

    bUseCLionPageNav = Config::Instance()["editor"].GetBool("pgupdown_content_first", true);
}

void EditorView::OnResized() {
    // Update the view Bottom line - as this affects how many lines we draw...
    editorModel->viewBottomLine = GetContentRect().Height();
    ViewBase::OnResized();
}

void EditorView::DrawViewContents() {
    auto &dc = window->GetContentDC(); //ViewBase::ContentAreaDrawContext();
    logger->Debug("DrawViewContents, dc Height=%d, topLine=%d, bottomLine=%d", dc.GetRect().Height(), editorModel->viewTopLine, editorModel->viewBottomLine);

    auto selection = editorModel->GetSelection();
    dc.ClearOverlays();

    // Add in the result from search if any...
    if (editorModel->searchResults.size() > 0) {
        logger->Debug("Overlays from search results");
        logger->Debug("  SR0: x=%d, len=%d line=%d", editorModel->searchResults[0].cursor_x, editorModel->searchResults[0].length, editorModel->searchResults[0].idxLine);
        for(auto &result : editorModel->searchResults) {
            DrawContext::Overlay overlay;
            overlay.Set(Point(result.cursor_x,result.idxLine),
                        Point(result.cursor_x + result.length, result.idxLine));
            overlay.isActive = true;
            dc.AddOverlay(overlay);
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
        // Maximize editor content view...
        MaximizeContentHeight();
    }
}

void EditorView::OnKeyPress(const KeyPress &keyPress) {
    // Unless we can edit - we do nothing
    if (!editorModel->GetTextBuffer()->CanEdit()) return;

    if (editorModel->GetEditController()->HandleKeyPress(editorModel->cursor, editorModel->idxActiveLine, keyPress)) {
        editorModel->GetEditController()->UpdateSyntaxForBuffer();
        // Cancel selection if we had one...
        if (editorModel->IsSelectionActive()) {
            editorModel->CancelSelection();
        }
        InvalidateView();
        return;
    }

    if (editorModel->HandleKeyPress(keyPress)) {
        InvalidateView();
        return;
    }

    // This handles regular backspace/delete/home/end (which are default actions for any single-line editing)
    if (editorModel->GetEditController()->HandleSpecialKeyPress(editorModel->cursor, editorModel->idxActiveLine, keyPress)) {
        InvalidateView();
        return;
    }


    // It was not to us..
    ViewBase::OnKeyPress(keyPress);
}

//
// Add actions here - all except human-readable inserting of text
//
bool EditorView::OnAction(const KeyPressAction &kpAction) {
    if (kpAction.actionModifier == kActionModifier::kActionModifierSelection) {
        if (!editorModel->IsSelectionActive()) {
            logger->Debug("Shift pressed, selection inactive - BeginSelection");
            editorModel->BeginSelection();
        }
    }

    // This is convoluted - will be dealt with when copy/paste works...
    if (kpAction.action == kAction::kActionCopyToClipboard) {
        logger->Debug("Set text to clip board");
        std::string buffer;
        auto selection = editorModel->GetSelection();
        editorModel->GetTextBuffer()->CopyRegionToString(buffer, selection.GetStart(), selection.GetEnd());
        logger->Debug("%s", buffer.c_str());
        if (SDL_SetClipboardText(buffer.c_str()) != 0) {
            logger->Error("SDL_SetClipboardText, error=%s", SDL_GetError());
        }
    } else if (kpAction.action == kAction::kActionPasteFromClipboard) {
        if (SDL_HasClipboardText() == SDL_TRUE) {
            auto clipBoardText = SDL_GetClipboardText();
            editorModel->GetEditController()->Paste(editorModel->idxActiveLine, clipBoardText);
            logger->Debug("Past from clipboard: '%s'", clipBoardText);
        } else {
            logger->Debug("No text in clipboard");
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
    auto endpos = currentLine->Length();
    editorModel->cursor.position.x = endpos;
    editorModel->cursor.wantedColumn = endpos;
    return true;
}

bool EditorView::OnActionCommitLine() {
    editorModel->GetEditController()->NewLine(editorModel->idxActiveLine, editorModel->cursor);
    OnNavigateDownVSCode(1);
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
    auto logger = gnilk::Logger::GetLogger("EditorView");
    logger->Debug("GotoFirstLine (def: CMD+Home), resetting cursor and view data!");
    editorModel->cursor.position.x = 0;
    editorModel->cursor.position.y = 0;
    editorModel->idxActiveLine = 0;
    editorModel->viewTopLine = 0;
    editorModel->viewBottomLine = GetContentRect().Height();

    return true;
}
bool EditorView::OnActionGotoLastLine() {
    auto logger = gnilk::Logger::GetLogger("EditorView");
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
    if (editorModel->cursor.position.x > currentLine->Length()) {
        editorModel->cursor.position.x = currentLine->Length();
    }
    editorModel->cursor.wantedColumn = editorModel->cursor.position.x;
    return true;
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
        OnNavigateDownVSCode(viewRect.Height() - 1);
    } else {
        OnNavigateDownCLion(viewRect.Height() - 1);
    }
    return true;
}

bool EditorView::OnActionPageUp() {
    if (!bUseCLionPageNav) {
        OnNavigateUpVSCode(viewRect.Height() - 1);
    } else {
        OnNavigateUpCLion(viewRect.Height() - 1);
    }
    return true;
}

bool EditorView::OnActionLineDown(const KeyPressAction &kpAction) {
    auto currentLine = editorModel->GetEditController()->LineAt(editorModel->idxActiveLine);

    OnNavigateDownVSCode(1);
    currentLine = editorModel->LineAt(editorModel->idxActiveLine);
    editorModel->cursor.position.x = editorModel->cursor.wantedColumn;
    if (editorModel->cursor.position.x > currentLine->Length()) {
        editorModel->cursor.position.x = currentLine->Length();
    }
    return true;
}
bool EditorView::OnActionLineUp() {
    auto currentLine = editorModel->GetEditController()->LineAt(editorModel->idxActiveLine);
    OnNavigateUpVSCode(1);
    currentLine = editorModel->LineAt(editorModel->idxActiveLine);
    editorModel->cursor.position.x = editorModel->cursor.wantedColumn;
    if (editorModel->cursor.position.x > currentLine->Length()) {
        editorModel->cursor.position.x = currentLine->Length();
    }
    return true;
}
bool EditorView::OnActionGotoTopLine() {
    auto logger = gnilk::Logger::GetLogger("EditorView");
    logger->Debug("GotoTopLine (def: PageUp+CMDKey) cursor=(%d:%d)", editorModel->cursor.position.x, editorModel->cursor.position.y);
    editorModel->cursor.position.y = 0;
    editorModel->idxActiveLine = editorModel->viewTopLine;
    logger->Debug("GotoTopLine, new cursor=(%d:%d)", editorModel->cursor.position.x, editorModel->cursor.position.y);
    return true;
}
bool EditorView::OnActionGotoBottomLine() {
    auto logger = gnilk::Logger::GetLogger("EditorView");
    logger->Debug("GotoBottomLine (def: PageDown+CMDKey), cursor=(%d:%d)", editorModel->cursor.position.x, editorModel->cursor.position.y);
    editorModel->cursor.position.y = GetContentRect().Height()-1;
    editorModel->idxActiveLine = editorModel->viewBottomLine-1;
    logger->Debug("GotoBottomLine, new  cursor=(%d:%d)", editorModel->cursor.position.x, editorModel->cursor.position.y);
    return true;
}

// FIXME: This can be made a static function in some kind of helper...
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
    auto nextModel = Editor::Instance().GetModelFromIndex(idxNext);
    RuntimeConfig::Instance().SetActiveEditorModel(nextModel);
    RuntimeConfig::Instance().GetRootView().Initialize();
    InvalidateAll();
    return true;
}


bool EditorView::UpdateNavigation(const KeyPress &keyPress) {

    auto viewRect = GetContentRect();
    auto currentLine = editorModel->GetEditController()->LineAt(editorModel->idxActiveLine);

    // Need to consider how to update the current line
    // the 'OnNavigateDown()' could return it, or we just leave it as is...

    switch (keyPress.specialKey) {
        case Keyboard::kKeyCode_Home :
            // CMD+Home = reset view to 0,0
            if (keyPress.IsCommandPressed()) {
/*
                auto logger = gnilk::Logger::GetLogger("EditorView");
                logger->Debug("CMD+Home, resetting cursor and view data!");
                editorModel->GetEditController()->LineAt(editorModel->idxActiveLine)->SetActive(false);
                cursor.position.x = 0;
                cursor.position.y = 0;
                editorModel->idxActiveLine = 0;
                editorModel->viewTopLine = 0;
                editorModel->viewBottomLine = GetContentRect().Height();
                editorModel->LineAt(editorModel->idxActiveLine)->SetActive(true);
                */
            }
            break;
        case Keyboard::kKeyCode_End :
            if (keyPress.IsCommandPressed()) {
                /*
                auto logger = gnilk::Logger::GetLogger("EditorView");
                logger->Debug("CMD+End, set cursor to last line!");

                editorModel->LineAt(editorModel->idxActiveLine)->SetActive(false);

                cursor.position.x = 0;
                cursor.position.y = GetContentRect().Height()-1;
                editorModel->idxActiveLine = editorModel->Lines().size()-1;
                editorModel->viewBottomLine = editorModel->Lines().size();
                editorModel->viewTopLine = editorModel->viewBottomLine - GetContentRect().Height();

                editorModel->LineAt(editorModel->idxActiveLine)->SetActive(true);
*/
            }
            break;
        case Keyboard::kKeyCode_PageUp :
            /*
             * Page Up/Down navigation works differently depending on your editor
             * CLion/Sublime:
             *      The content/text moves and cursor stays in position
             *      ALT+Up/Down, the cursor moves within the view area, content/text stays
             * VSCode:
             *      The cursor moves to next to last-visible line
             *      ALT+Up/Down the view area moves but cursor/activeline stays
             */
            /*
            if (keyPress.IsCommandPressed()) {
                auto logger = gnilk::Logger::GetLogger("EditorView");
                logger->Debug("PageUp+CMDKey, cursor=(%d:%d)", cursor.position.x, cursor.position.y);
                cursor.position.y = 0;
                editorModel->idxActiveLine = editorModel->viewTopLine;
                logger->Debug("PageUp+CMDKey, cursor=(%d:%d)", cursor.position.x, cursor.position.y);
            }
             */
            break;
        case Keyboard::kKeyCode_PageDown :
            /*
            if (keyPress.IsCommandPressed()) {
                auto logger = gnilk::Logger::GetLogger("EditorView");
                logger->Debug("PageDown+CMDKey, cursor=(%d:%d)", cursor.position.x, cursor.position.y);
                cursor.position.y = GetContentRect().Height()-1;
                editorModel->idxActiveLine = editorModel->viewBottomLine-1;
                logger->Debug("PageDown+CMDKey, cursor=(%d:%d)", cursor.position.x, cursor.position.y);
            }
             */
            break;
            // Return is a bit "stupid"...
        case Keyboard::kKeyCode_Return :
            break;
        default:
            // Not navigation
            return false;
    }

//    // Do selection handling
//    if (isShiftPressed) {
//        if (!selection.IsActive()) {
//            selection.Begin(idxLineBeforeNavigation);
//        }
//        selection.Continue(idxActiveLine);
//    } else if (selection.IsActive()) {
//        selection.SetActive(false);
//        ClearSelectedLines();
//        screen->InvalidateAll();
//    }

    return true;

}
//
// This implements VSCode style of downwards navigation
// Cursor if moved first then content (i.e if standing on first-line, the cursor is moved to the bottom line on first press)
void EditorView::OnNavigateDownVSCode(int rows) {
    auto &lines = editorModel->Lines();

    editorModel->idxActiveLine+=rows;
    if (editorModel->idxActiveLine >= lines.size()) {
        editorModel->idxActiveLine = lines.size()-1;
    }

    if (editorModel->idxActiveLine > GetContentRect().Height()-1) {
        if (!(editorModel->cursor.position.y < GetContentRect().Height()-1)) {
            if ((editorModel->viewBottomLine + rows) < lines.size()) {
                logger->Debug("Clipping top/bottom lines");
                editorModel->viewTopLine += rows;
                editorModel->viewBottomLine += rows;
            }
            InvalidateAll();
        }
    }
    logger->Debug("OnNavDownVScode, rows=%d, new active line=%d", rows, editorModel->idxActiveLine);
    logger->Debug("                 viewTopLine=%d, viewBottomLine=%d", editorModel->viewTopLine, editorModel->viewBottomLine);


    editorModel->cursor.position.y = editorModel->idxActiveLine - editorModel->viewTopLine;
    if (editorModel->cursor.position.y > GetContentRect().Height()-1) {
        editorModel->cursor.position.y = GetContentRect().Height()-1;
    }
    //logger->Debug("OnNavigateDown, activeLine=%d, rows=%d, ypos=%d, height=%d", viewData.idxActiveLine, rows, cursor.position.y, ContentRect().Height());
}

void EditorView::OnNavigateUpVSCode(int rows) {
    editorModel->idxActiveLine -= rows;
    if (editorModel->idxActiveLine < 0) {
        editorModel->idxActiveLine = 0;
    }

    editorModel->cursor.position.y -= rows;
    if (editorModel->cursor.position.y < 0) {
        int delta = 0 - editorModel->cursor.position.y;
        editorModel->cursor.position.y = 0;
        editorModel->viewTopLine -= delta;
        editorModel->viewBottomLine -= delta;
        if (editorModel->viewTopLine < 0) {
            editorModel->viewTopLine = 0;
            editorModel->viewBottomLine = GetContentRect().Height();
        }
        // Request full redraw (this caused a scroll)
        InvalidateAll();
    }
}

// CLion/Sublime style of navigation on pageup/down - this first moves the content and then adjust cursor
// This moves content first and cursor rather stays
void EditorView::OnNavigateDownCLion(int rows) {
    bool forceCursorToLastLine = false;
    int nRowsToMove = rows;
    int maxRows = editorModel->Lines().size() - 1;

    logger->Debug("OnNavDownCLion");

    // Note: We might want to revisit this clipping, if we want a margin to the bottom...
    // Maybe we should adjust for the visible margin at the end
    if ((editorModel->viewTopLine+nRowsToMove) > maxRows) {
        logger->Debug("  Move beyond last line!");
        nRowsToMove = 0;
        forceCursorToLastLine = true;
    }
    if ((editorModel->viewBottomLine+nRowsToMove) > maxRows) {
        logger->Debug("  Clip nRowsToMove");
        nRowsToMove = editorModel->Lines().size() - editorModel->viewBottomLine;
        // If this results to zero, we are exactly at the bottom...
        if (!nRowsToMove) {
            forceCursorToLastLine = true;
        }
    }
    logger->Debug("  nRowsToMove=%d, forceCursor=%s, nLines=%d, maxRows=%d",
                  nRowsToMove, forceCursorToLastLine?"Y":"N", (int)editorModel->Lines().size(), maxRows);
    logger->Debug("  Before, topLine=%d, bottomLine=%d, activeLine=%d, cursor.y=%d", editorModel->viewTopLine, editorModel->viewBottomLine, editorModel->idxActiveLine, editorModel->cursor.position.y);


    // Reposition the view
    int activeLineDelta = editorModel->idxActiveLine - editorModel->viewTopLine;
    editorModel->viewTopLine += nRowsToMove;
    editorModel->viewBottomLine += nRowsToMove;

    // In case we would have moved beyond the visible part, let's enforce the cursor position..
    if (forceCursorToLastLine) {
        editorModel->cursor.position.y = editorModel->Lines().size() - editorModel->viewTopLine - 1;
        editorModel->idxActiveLine = editorModel->Lines().size()-1;
        logger->Debug("       force to last!");
    } else {
        editorModel->idxActiveLine = editorModel->viewTopLine + activeLineDelta;
        editorModel->cursor.position.y = editorModel->idxActiveLine - editorModel->viewTopLine;
        if (editorModel->cursor.position.y > GetContentRect().Height() - 1) {
            editorModel->cursor.position.y = GetContentRect().Height() - 1;
        }
    }
    logger->Debug("  After, topLine=%d, bottomLine=%d, activeLine=%d, cursor.y=%d", editorModel->viewTopLine, editorModel->viewBottomLine, editorModel->idxActiveLine, editorModel->cursor.position.y);
    InvalidateAll();
}

void EditorView::OnNavigateUpCLion(int rows) {
    int nRowsToMove = rows;
    int maxRows = editorModel->Lines().size() - 1;
    bool forceCursorToFirstLine = false;

    if ((editorModel->viewTopLine - nRowsToMove) < 0) {
        forceCursorToFirstLine = true;
        nRowsToMove = 0;
    }


    logger->Debug("OnNavUpCLion");
    logger->Debug("  nRowsToMove=%d, forceCursor=%s, nLines=%d, maxRows=%d",
                  nRowsToMove, forceCursorToFirstLine?"Y":"N", (int)editorModel->Lines().size(), maxRows);
    logger->Debug("  Before, topLine=%d, bottomLine=%d, activeLine=%d, cursor.y=%d", editorModel->viewTopLine, editorModel->viewBottomLine, editorModel->idxActiveLine, editorModel->cursor.position.y);


    // Reposition the view
    editorModel->viewTopLine -= nRowsToMove;
    editorModel->viewBottomLine -= nRowsToMove;

    // In case we would have moved beyond the visible part, let's enforce the cursor position..
    if (forceCursorToFirstLine) {
        editorModel->cursor.position.y = 0;
        editorModel->idxActiveLine = 0;
        editorModel->viewTopLine = 0;
        editorModel->viewBottomLine = GetContentRect().Height();
        logger->Debug("       force to first!");
    } else {
        editorModel->idxActiveLine -= nRowsToMove;
        editorModel->cursor.position.y = editorModel->idxActiveLine - editorModel->viewTopLine;
        if (editorModel->cursor.position.y < 0) {
            editorModel->cursor.position.y = 0;
        }
    }
    logger->Debug("  After, topLine=%d, bottomLine=%d, activeLine=%d, cursor.y=%d", editorModel->viewTopLine, editorModel->viewBottomLine, editorModel->idxActiveLine, editorModel->cursor.position.y);
    InvalidateAll();

}



void EditorView::SetWindowCursor(const Cursor &cursor) {
    if (Editor::Instance().GetState() == Editor::EditState) {
        window->SetCursor(editorModel->cursor);
    } else {
        // The editor view is NOT in 'command' but rather the 'owner' of the quick-cmd input view..
        ViewBase::SetWindowCursor(cursor);
    }
}
