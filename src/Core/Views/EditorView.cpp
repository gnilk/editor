//
// Created by gnilk on 14.02.23.
//

#include "Core/Config/Config.h"
#include "Core/RuntimeConfig.h"
#include "EditorView.h"

using namespace gedit;

void EditorView::InitView()  {
    logger = gnilk::Logger::GetLogger("EditorView");

    auto screen = RuntimeConfig::Instance().Screen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_Border);

    auto &rect = window->GetContentDC().GetRect();

    // This is the visible area...
    viewData.viewTopLine = 0;
    viewData.viewBottomLine = rect.Height();
    viewData.editController.SetTextBufferChangedHandler([this]()->void {
       auto textBuffer = viewData.editController.GetTextBuffer();
       window->SetCaption(textBuffer->Name());
    });


    // We own the view-data but let's share it - this allows other views to READ it..
    if (GetParentView() != nullptr) {
        GetParentView()->SetSharedData(&viewData);
    }

    bUseCLionPageNav = Config::Instance()["editor"].GetBool("pgupdown_content_first", true);
}

void EditorView::OnResized() {
    // Update the view Bottom line - as this affects how many lines we draw...
    viewData.viewBottomLine = GetContentRect().Height();
    ViewBase::OnResized();
}


void EditorView::DrawViewContents() {
    auto &dc = window->GetContentDC(); //ViewBase::ContentAreaDrawContext();
    dc.DrawLines(viewData.editController.Lines(), viewData.viewTopLine, viewData.viewBottomLine);
    return;
    // Draw from line array between these..
    if (IsInvalid()) {
        logger->Debug("Redrawing everything");
        dc.DrawLines(viewData.editController.Lines(), viewData.viewTopLine, viewData.viewBottomLine);
    } else {
        dc.DrawLine(viewData.editController.LineAt(viewData.idxActiveLine), cursor.position.y);
    }
}

void EditorView::OnKeyPress(const KeyPress &keyPress) {
    if (!keyPress.isKeyValid) return;

    if (viewData.editController.HandleKeyPress(cursor, viewData.idxActiveLine, keyPress)) {
        InvalidateView();
        return;
    }
    if (UpdateNavigation(keyPress)) {
        return;
    }
    // It was not to us..
    ViewBase::OnKeyPress(keyPress);
}

bool EditorView::UpdateNavigation(const KeyPress &keyPress) {

    auto viewRect = GetContentRect();
    auto currentLine = viewData.editController.LineAt(viewData.idxActiveLine);

    // Need to consider how to update the current line
    // the 'OnNavigateDown()' could return it, or we just leave it as is...

    switch (keyPress.key) {
        case kKey_Down:
            OnNavigateDownVSCode(1);
            currentLine = viewData.editController.LineAt(viewData.idxActiveLine);
            cursor.position.x = cursor.wantedColumn;
            if (cursor.position.x > currentLine->Length()) {
                cursor.position.x = currentLine->Length();
            }
            break;
        case kKey_Up :
            OnNavigateUpVSCode(1);
            currentLine = viewData.editController.LineAt(viewData.idxActiveLine);
            cursor.position.x = cursor.wantedColumn;
            if (cursor.position.x > currentLine->Length()) {
                cursor.position.x = currentLine->Length();
            }
            break;
        case kKey_Left :
            cursor.position.x--;
            if (cursor.position.x < 0) {
                cursor.position.x = 0;
            }
            cursor.wantedColumn = cursor.position.x;
            break;
        case kKey_Right :
//            if (keyPress.IsCtrlPressed()) {
//                cursor.activeColumn+=4;
//            } else {
//                cursor.activeColumn++;
//            }
            cursor.position.x++;
            if (cursor.position.x > currentLine->Length()) {
                cursor.position.x = currentLine->Length();
            }
            cursor.wantedColumn = cursor.position.x;
            break;
        case kKey_PageUp :
            /*
             * Page Up/Down navigation works differently depending on your editor
             * CLion/Sublime:
             *      The content/text moves and cursor stays in position
             *      ALT+Up/Down, the cursor moves within the view area, content/text stays
             * VSCode:
             *      The cursor moves to next to last-visible line
             *      ALT+Up/Down the view area moves but cursor/activeline stays
             */
            if (!bUseCLionPageNav) {
                OnNavigateUpVSCode(viewRect.Height() - 1);
            } else {
                OnNavigateUpCLion(viewRect.Height() - 1);
            }
            break;
        case kKey_PageDown :
            if (!bUseCLionPageNav) {
                OnNavigateDownVSCode(viewRect.Height() - 1);
            } else {
                OnNavigateDownCLion(viewRect.Height() - 1);
            }
            break;
            // Return is a bit "stupid"...
        case kKey_Return :
            viewData.editController.NewLine(viewData.idxActiveLine, cursor);
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
    auto currentLine = viewData.editController.LineAt(viewData.idxActiveLine);
    currentLine->SetActive(false);
    auto &lines = viewData.editController.Lines();

    viewData.idxActiveLine+=rows;
    if (viewData.idxActiveLine >= lines.size()) {
        viewData.idxActiveLine = lines.size()-1;
    }

    currentLine = lines[viewData.idxActiveLine];
    currentLine->SetActive(true);

    if (viewData.idxActiveLine > GetContentRect().Height()-1) {
        if (!(cursor.position.y < GetContentRect().Height()-1)) {
            logger->Debug("Clipping top/bottom lines");
            viewData.viewTopLine += rows;
            viewData.viewBottomLine += rows;
            // Request full redraw next time, as this caused a scroll...
            InvalidateAll();
            //nativeWindow->Scroll(1);
        }
    }
    logger->Debug("OnNavDownVScode, rows=%d, new active line=%d", rows, viewData.idxActiveLine);
    logger->Debug("                 viewTopLine=%d, viewBottomLine=%d", viewData.viewTopLine, viewData.viewBottomLine);


    cursor.position.y = viewData.idxActiveLine - viewData.viewTopLine;
    if (cursor.position.y > GetContentRect().Height()-1) {
        cursor.position.y = GetContentRect().Height()-1;
    }


    //logger->Debug("OnNavigateDown, activeLine=%d, rows=%d, ypos=%d, height=%d", viewData.idxActiveLine, rows, cursor.position.y, ContentRect().Height());
}

void EditorView::OnNavigateUpVSCode(int rows) {
    auto currentLine = viewData.editController.LineAt(viewData.idxActiveLine);
    currentLine->SetActive(false);
    auto &lines = viewData.editController.Lines();

    viewData.idxActiveLine -= rows;
    if (viewData.idxActiveLine < 0) {
        viewData.idxActiveLine = 0;
    }

    cursor.position.y -= rows;
    if (cursor.position.y < 0) {
        int delta = 0 - cursor.position.y;
        cursor.position.y = 0;
        viewData.viewTopLine -= delta;
        viewData.viewBottomLine -= delta;
        if (viewData.viewTopLine < 0) {
            viewData.viewTopLine = 0;
            viewData.viewBottomLine = GetContentRect().Height();
        }
        // Request full redraw (this caused a scroll)
        InvalidateAll();
    }

    currentLine = lines[viewData.idxActiveLine];
    currentLine->SetActive(true);
}

// CLion/Sublime style of navigation on pageup/down - this first moves the content and then adjust cursor
// This moves content first and cursor rather stays
void EditorView::OnNavigateDownCLion(int rows) {
    auto currentLine = viewData.editController.LineAt(viewData.idxActiveLine);
    currentLine->SetActive(false);

    bool forceCursorToLastLine = false;
    int nRowsToMove = rows;
    int maxRows = viewData.editController.Lines().size() - 1;

    // Note: We might want to revisit this clipping, if we want a margin to the bottom...
    // Maybe we should adjust for the visible margin at the end
    if ((viewData.viewTopLine+nRowsToMove) > maxRows) {
        nRowsToMove = 0;
        forceCursorToLastLine = true;
    }
    logger->Debug("OnNavDownCLion");
    logger->Debug("  nRowsToMove=%d, forceCursor=%s, nLines=%d, maxRows=%d",
                  nRowsToMove, forceCursorToLastLine?"Y":"N", (int)viewData.editController.Lines().size(), maxRows);
    logger->Debug("  Before, topLine=%d, bottomLine=%d, activeLine=%d, cursor.y=%d", viewData.viewTopLine, viewData.viewBottomLine, viewData.idxActiveLine, cursor.position.y);


    // Reposition the view
    int activeLineDelta = viewData.idxActiveLine - viewData.viewTopLine;
    viewData.viewTopLine += nRowsToMove;
    viewData.viewBottomLine += nRowsToMove;

    // In case we would have moved beyond the visible part, let's enforce the cursor position..
    if (forceCursorToLastLine) {
        cursor.position.y = viewData.editController.Lines().size() - viewData.viewTopLine - 1;
        viewData.idxActiveLine = viewData.editController.Lines().size()-1;
        logger->Debug("       force to last!");
    } else {
        viewData.idxActiveLine = viewData.viewTopLine + activeLineDelta;
        cursor.position.y = viewData.idxActiveLine - viewData.viewTopLine;
        if (cursor.position.y > GetContentRect().Height() - 1) {
            cursor.position.y = GetContentRect().Height() - 1;
        }
    }
    logger->Debug("  After, topLine=%d, bottomLine=%d, activeLine=%d, cursor.y=%d", viewData.viewTopLine, viewData.viewBottomLine, viewData.idxActiveLine, cursor.position.y);
    InvalidateAll();
}

void EditorView::OnNavigateUpCLion(int rows) {
    auto currentLine = viewData.editController.LineAt(viewData.idxActiveLine);
    currentLine->SetActive(false);

    int nRowsToMove = rows;
    int maxRows = viewData.editController.Lines().size() - 1;
    bool forceCursorToFirstLine = false;

    if ((viewData.viewTopLine - nRowsToMove) < 0) {
        forceCursorToFirstLine = true;
        nRowsToMove = 0;
    }


    logger->Debug("OnNavUpCLion");
    logger->Debug("  nRowsToMove=%d, forceCursor=%s, nLines=%d, maxRows=%d",
                  nRowsToMove, forceCursorToFirstLine?"Y":"N", (int)viewData.editController.Lines().size(), maxRows);
    logger->Debug("  Before, topLine=%d, bottomLine=%d, activeLine=%d, cursor.y=%d", viewData.viewTopLine, viewData.viewBottomLine, viewData.idxActiveLine, cursor.position.y);


    // Reposition the view
    viewData.viewTopLine -= nRowsToMove;
    viewData.viewBottomLine -= nRowsToMove;

    // In case we would have moved beyond the visible part, let's enforce the cursor position..
    if (forceCursorToFirstLine) {
        cursor.position.y = 0;
        viewData.idxActiveLine = 0;
        viewData.viewTopLine = 0;
        viewData.viewBottomLine = GetContentRect().Height();
        logger->Debug("       force to first!");
    } else {
        viewData.idxActiveLine -= nRowsToMove;
        cursor.position.y = viewData.idxActiveLine - viewData.viewTopLine;
        if (cursor.position.y < 0) {
            cursor.position.y = 0;
        }
    }
    logger->Debug("  After, topLine=%d, bottomLine=%d, activeLine=%d, cursor.y=%d", viewData.viewTopLine, viewData.viewBottomLine, viewData.idxActiveLine, cursor.position.y);
    InvalidateAll();

}
