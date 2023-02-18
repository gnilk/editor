//
// Created by gnilk on 14.02.23.
//

#include "Core/RuntimeConfig.h"
#include "EditorView.h"

using namespace gedit;

void EditorView::Begin() {
    ViewBase::Begin();  // This is a good idea..
    logger = gnilk::Logger::GetLogger("EditorView");
    // This is the visible area...
    viewData.viewTopLine = 0;
    viewData.viewBottomLine = ContentRect().Height();
    viewData.editController.SetTextBufferChangedHandler([this]()->void {
       auto textBuffer = viewData.editController.GetTextBuffer();
       this->SetCaption(textBuffer->Name());
    });


    // We own the view-data but let's share it - this allows other views to READ it..
    if (ParentView() != nullptr) {
        ParentView()->SetSharedData(&viewData);
    }


}

void EditorView::OnResized() {
    // Update the view Bottom line - as this affects how many lines we draw...
    viewData.viewBottomLine = ContentRect().Height();
    ViewBase::OnResized();
}


void EditorView::DrawViewContents() {
    auto &ctx = ViewBase::ContentAreaDrawContext();

    // Draw from line array between these..
    if (IsInvalid()) {
        logger->Debug("Redrawing everything");
        ctx.DrawLines(viewData.editController.Lines(), viewData.viewTopLine, viewData.viewBottomLine);
    } else {
        ctx.DrawLine(viewData.editController.LineAt(viewData.idxActiveLine), cursor.position.y);
    }
}

void EditorView::OnKeyPress(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) {
    if (!keyPress.isKeyValid) return;

    if (viewData.editController.HandleKeyPress(cursor, viewData.idxActiveLine, keyPress)) {
        return;
    }
    if (UpdateNavigation(keyPress)) {
        return;
    }
    // It was not to us..
    ViewBase::OnKeyPress(keyPress);
}

bool EditorView::UpdateNavigation(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) {
    auto screen = RuntimeConfig::Instance().Screen();
    auto viewRect = ViewRect();

    auto currentLine = viewData.editController.LineAt(viewData.idxActiveLine);

    // save current line - as it will update with navigation
    // we need it when we update the selection status...
    //auto idxLineBeforeNavigation = viewData.idxActiveLine;

    switch (keyPress.key) {
        case kKey_Down:
            OnNavigateDown(1);
            cursor.position.x = cursor.wantedColumn;
            if (cursor.position.x > currentLine->Length()) {
                cursor.position.x = currentLine->Length();
            }
            break;
        case kKey_Up :
            OnNavigateUp(1);
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
            OnNavigateUp(viewRect.Height());
            break;
        case kKey_PageDown :
            OnNavigateDown(viewRect.Height());
            break;
        case kKey_Return :
            viewData.editController.NewLine(viewData.idxActiveLine, cursor);
            OnNavigateDown(1);
            InvalidateAll();
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

void EditorView::OnNavigateDown(int rows) {
    auto currentLine = viewData.editController.LineAt(viewData.idxActiveLine);
    currentLine->SetActive(false);
    auto &lines = viewData.editController.Lines();

    viewData.idxActiveLine+=rows;
    if (viewData.idxActiveLine >= lines.size()) {
        viewData.idxActiveLine = lines.size()-1;
    }

    currentLine = lines[viewData.idxActiveLine];
    currentLine->SetActive(true);

    if (viewData.idxActiveLine > ContentRect().Height()-1) {
        if (!(cursor.position.y < ContentRect().Height()-1)) {
            viewData.viewTopLine += rows;
            viewData.viewBottomLine += rows;
            // Request full redraw next time, as this caused a scroll...
            InvalidateAll();
        }
    }

    cursor.position.y = viewData.idxActiveLine - viewData.viewTopLine;
    if (cursor.position.y > ContentRect().Height()-1) {
        cursor.position.y = ContentRect().Height()-1;
    }


    //logger->Debug("OnNavigateDown, activeLine=%d, rows=%d, ypos=%d, height=%d", viewData.idxActiveLine, rows, cursor.position.y, ContentRect().Height());
}

void EditorView::OnNavigateUp(int rows) {
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
            viewData.viewBottomLine = ContentRect().Height();
        }
        // Request full redraw (this caused a scroll)
        InvalidateAll();
    }

    currentLine = lines[viewData.idxActiveLine];
    currentLine->SetActive(true);
}
