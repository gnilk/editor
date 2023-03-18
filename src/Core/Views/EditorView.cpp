//
// Created by gnilk on 14.02.23.
//

#include "Core/Config/Config.h"
#include "Core/RuntimeConfig.h"
#include "EditorView.h"

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
       window->SetCaption(textBuffer->Name());
    });


    // We own the view-data but let's share it - this allows other views to READ it..
//    if (GetParentView() != nullptr) {
//        GetParentView()->SetSharedData(&viewData);
//    }

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

//    // This is the visible area...
    editorModel->viewTopLine = 0;
    editorModel->viewBottomLine = rect.Height();
//    viewData.editController.SetTextBufferChangedHandler([this]()->void {
//        auto textBuffer = viewData.editController.GetTextBuffer();
//        window->SetCaption(textBuffer->Name());
//    });


    // We own the view-data but let's share it - this allows other views to READ it..
//    if (GetParentView() != nullptr) {
//        GetParentView()->SetSharedData(&viewData);
//    }

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
    dc.DrawLines(editorModel->GetEditController()->Lines(), editorModel->viewTopLine, editorModel->viewBottomLine);
    return;
    // Draw from line array between these..
    if (IsInvalid()) {
        logger->Debug("Redrawing everything");
        dc.DrawLines(editorModel->GetEditController()->Lines(), editorModel->viewTopLine, editorModel->viewBottomLine);
    } else {
        dc.DrawLine(editorModel->LineAt(editorModel->idxActiveLine), cursor.position.y);
    }
}

void EditorView::OnActivate(bool isActive) {
    logger->Debug("OnActive, isActive: %s", isActive?"yes":"no");
    if (!isActive) {
        // reset height of view..
        // We should have this configureable - restore or reset (I can imagine some people will hate a reset)
        ResetContentHeight();
    } else {
        // Maximize editor content view...
        MaximizeContentHeight();
    }
}



void EditorView::OnKeyPress(const KeyPress &keyPress) {

    if (editorModel->GetEditController()->HandleKeyPress(editorModel->cursor, editorModel->idxActiveLine, keyPress)) {
        InvalidateView();
        return;
    }
    if (UpdateNavigation(keyPress)) {
        editorModel->cursor = cursor;
        return;
    }

    // It was not to us..
    ViewBase::OnKeyPress(keyPress);
}

bool EditorView::UpdateNavigation(const KeyPress &keyPress) {

    auto viewRect = GetContentRect();
    auto currentLine = editorModel->GetEditController()->LineAt(editorModel->idxActiveLine);

    // Need to consider how to update the current line
    // the 'OnNavigateDown()' could return it, or we just leave it as is...

    switch (keyPress.specialKey) {
        case Keyboard::kKeyCode_Home :
            if (keyPress.IsCommandPressed()) {
                auto logger = gnilk::Logger::GetLogger("EditorView");
                logger->Debug("CMD+Home, resetting cursor and view data!");
                editorModel->GetEditController()->LineAt(editorModel->idxActiveLine)->SetActive(false);
                cursor.position.x = 0;
                cursor.position.y = 0;
                editorModel->idxActiveLine = 0;
                editorModel->viewTopLine = 0;
                editorModel->viewBottomLine = GetContentRect().Height();
                editorModel->LineAt(editorModel->idxActiveLine)->SetActive(true);
            }
            break;
        case Keyboard::kKeyCode_End :
            if (keyPress.IsCommandPressed()) {
                auto logger = gnilk::Logger::GetLogger("EditorView");
                logger->Debug("CMD+End, set cursor to last line!");

                editorModel->LineAt(editorModel->idxActiveLine)->SetActive(false);

                cursor.position.x = 0;
                cursor.position.y = GetContentRect().Height()-1;
                editorModel->idxActiveLine = editorModel->Lines().size()-1;
                editorModel->viewBottomLine = editorModel->Lines().size();
                editorModel->viewTopLine = editorModel->viewBottomLine - GetContentRect().Height();

                editorModel->LineAt(editorModel->idxActiveLine)->SetActive(true);

            }
            break;
        case Keyboard::kKeyCode_DownArrow:
            OnNavigateDownVSCode(1);
            currentLine = editorModel->LineAt(editorModel->idxActiveLine);
            cursor.position.x = cursor.wantedColumn;
            if (cursor.position.x > currentLine->Length()) {
                cursor.position.x = currentLine->Length();
            }
            break;
        case Keyboard::kKeyCode_UpArrow:
            OnNavigateUpVSCode(1);
            currentLine = editorModel->LineAt(editorModel->idxActiveLine);
            cursor.position.x = cursor.wantedColumn;
            if (cursor.position.x > currentLine->Length()) {
                cursor.position.x = currentLine->Length();
            }
            break;
        case Keyboard::kKeyCode_LeftArrow :
            cursor.position.x--;
            if (cursor.position.x < 0) {
                cursor.position.x = 0;
            }
            cursor.wantedColumn = cursor.position.x;
            break;
        case Keyboard::kKeyCode_RightArrow:
            cursor.position.x++;
            if (cursor.position.x > currentLine->Length()) {
                cursor.position.x = currentLine->Length();
            }
            cursor.wantedColumn = cursor.position.x;
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
            if (keyPress.IsCommandPressed()) {
                auto logger = gnilk::Logger::GetLogger("EditorView");
                logger->Debug("PageUp+CMDKey, cursor=(%d:%d)", cursor.position.x, cursor.position.y);
                cursor.position.y = 0;
                editorModel->idxActiveLine = editorModel->viewTopLine;
                logger->Debug("PageUp+CMDKey, cursor=(%d:%d)", cursor.position.x, cursor.position.y);
            } else {
                if (!bUseCLionPageNav) {
                    OnNavigateUpVSCode(viewRect.Height() - 1);
                } else {
                    OnNavigateUpCLion(viewRect.Height() - 1);
                }
            }
            break;
        case Keyboard::kKeyCode_PageDown :
            if (keyPress.IsCommandPressed()) {
                auto logger = gnilk::Logger::GetLogger("EditorView");
                logger->Debug("PageDown+CMDKey, cursor=(%d:%d)", cursor.position.x, cursor.position.y);
                cursor.position.y = GetContentRect().Height()-1;
                editorModel->idxActiveLine = editorModel->viewBottomLine-1;
                logger->Debug("PageDown+CMDKey, cursor=(%d:%d)", cursor.position.x, cursor.position.y);
            } else {
                if (!bUseCLionPageNav) {
                    OnNavigateDownVSCode(viewRect.Height() - 1);
                } else {
                    OnNavigateDownCLion(viewRect.Height() - 1);
                }
            }
            break;
            // Return is a bit "stupid"...
        case Keyboard::kKeyCode_Return :
            editorModel->GetEditController()->NewLine(editorModel->idxActiveLine, cursor);
            OnNavigateDownVSCode(1);
            InvalidateView();
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
    auto currentLine = editorModel->LineAt(editorModel->idxActiveLine);
    currentLine->SetActive(false);
    auto &lines = editorModel->Lines();

    editorModel->idxActiveLine+=rows;
    if (editorModel->idxActiveLine >= lines.size()) {
        editorModel->idxActiveLine = lines.size()-1;
    }

    currentLine = lines[editorModel->idxActiveLine];
    currentLine->SetActive(true);

    if (editorModel->idxActiveLine > GetContentRect().Height()-1) {
        if (!(cursor.position.y < GetContentRect().Height()-1)) {
            if ((editorModel->viewBottomLine + rows) < lines.size()) {
                logger->Debug("Clipping top/bottom lines");
                editorModel->viewTopLine += rows;
                editorModel->viewBottomLine += rows;
            }
            // Request full redraw next time, as this caused a scroll...
            InvalidateAll();
            //nativeWindow->Scroll(1);
        }
    }
    logger->Debug("OnNavDownVScode, rows=%d, new active line=%d", rows, editorModel->idxActiveLine);
    logger->Debug("                 viewTopLine=%d, viewBottomLine=%d", editorModel->viewTopLine, editorModel->viewBottomLine);


    cursor.position.y = editorModel->idxActiveLine - editorModel->viewTopLine;
    if (cursor.position.y > GetContentRect().Height()-1) {
        cursor.position.y = GetContentRect().Height()-1;
    }


    //logger->Debug("OnNavigateDown, activeLine=%d, rows=%d, ypos=%d, height=%d", viewData.idxActiveLine, rows, cursor.position.y, ContentRect().Height());
}

void EditorView::OnNavigateUpVSCode(int rows) {
    auto currentLine = editorModel->LineAt(editorModel->idxActiveLine);
    currentLine->SetActive(false);
    auto &lines = editorModel->Lines();

    editorModel->idxActiveLine -= rows;
    if (editorModel->idxActiveLine < 0) {
        editorModel->idxActiveLine = 0;
    }

    cursor.position.y -= rows;
    if (cursor.position.y < 0) {
        int delta = 0 - cursor.position.y;
        cursor.position.y = 0;
        editorModel->viewTopLine -= delta;
        editorModel->viewBottomLine -= delta;
        if (editorModel->viewTopLine < 0) {
            editorModel->viewTopLine = 0;
            editorModel->viewBottomLine = GetContentRect().Height();
        }
        // Request full redraw (this caused a scroll)
        InvalidateAll();
    }

    currentLine = lines[editorModel->idxActiveLine];
    currentLine->SetActive(true);
}

// CLion/Sublime style of navigation on pageup/down - this first moves the content and then adjust cursor
// This moves content first and cursor rather stays
void EditorView::OnNavigateDownCLion(int rows) {
    auto currentLine = editorModel->LineAt(editorModel->idxActiveLine);
    currentLine->SetActive(false);

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
    logger->Debug("  Before, topLine=%d, bottomLine=%d, activeLine=%d, cursor.y=%d", editorModel->viewTopLine, editorModel->viewBottomLine, editorModel->idxActiveLine, cursor.position.y);


    // Reposition the view
    int activeLineDelta = editorModel->idxActiveLine - editorModel->viewTopLine;
    editorModel->viewTopLine += nRowsToMove;
    editorModel->viewBottomLine += nRowsToMove;

    // In case we would have moved beyond the visible part, let's enforce the cursor position..
    if (forceCursorToLastLine) {
        cursor.position.y = editorModel->Lines().size() - editorModel->viewTopLine - 1;
        editorModel->idxActiveLine = editorModel->Lines().size()-1;
        logger->Debug("       force to last!");
    } else {
        editorModel->idxActiveLine = editorModel->viewTopLine + activeLineDelta;
        cursor.position.y = editorModel->idxActiveLine - editorModel->viewTopLine;
        if (cursor.position.y > GetContentRect().Height() - 1) {
            cursor.position.y = GetContentRect().Height() - 1;
        }
    }
    logger->Debug("  After, topLine=%d, bottomLine=%d, activeLine=%d, cursor.y=%d", editorModel->viewTopLine, editorModel->viewBottomLine, editorModel->idxActiveLine, cursor.position.y);
    InvalidateAll();
}

void EditorView::OnNavigateUpCLion(int rows) {
    auto currentLine = editorModel->LineAt(editorModel->idxActiveLine);
    currentLine->SetActive(false);

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
    logger->Debug("  Before, topLine=%d, bottomLine=%d, activeLine=%d, cursor.y=%d", editorModel->viewTopLine, editorModel->viewBottomLine, editorModel->idxActiveLine, cursor.position.y);


    // Reposition the view
    editorModel->viewTopLine -= nRowsToMove;
    editorModel->viewBottomLine -= nRowsToMove;

    // In case we would have moved beyond the visible part, let's enforce the cursor position..
    if (forceCursorToFirstLine) {
        cursor.position.y = 0;
        editorModel->idxActiveLine = 0;
        editorModel->viewTopLine = 0;
        editorModel->viewBottomLine = GetContentRect().Height();
        logger->Debug("       force to first!");
    } else {
        editorModel->idxActiveLine -= nRowsToMove;
        cursor.position.y = editorModel->idxActiveLine - editorModel->viewTopLine;
        if (cursor.position.y < 0) {
            cursor.position.y = 0;
        }
    }
    logger->Debug("  After, topLine=%d, bottomLine=%d, activeLine=%d, cursor.y=%d", editorModel->viewTopLine, editorModel->viewBottomLine, editorModel->idxActiveLine, cursor.position.y);
    InvalidateAll();

}
