//
// Created by gnilk on 14.02.23.
//

#include "RuntimeConfig.h"
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

void EditorView::DrawViewContents() {
    auto &ctx = ViewBase::ContentAreaDrawContext();

    // Draw from line array between these..
    ctx.DrawLines(viewData.editController.Lines(), viewData.viewTopLine, viewData.viewBottomLine);


    // Update cursor screen position, need to translate to screen coords..
    Cursor screenCursor;
    screenCursor.position = ctx.ToScreen(cursor.position);

    auto screen = RuntimeConfig::Instance().Screen();
    screen->SetCursor(screenCursor);

}

void EditorView::OnKeyPress(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) {
    if (viewData.editController.HandleKeyPress(viewData.idxActiveLine, keyPress)) {
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
    auto dimensions = Dimensions();

    auto currentLine = viewData.editController.LineAt(viewData.idxActiveLine);

    // save current line - as it will update with navigation
    // we need it when we update the selection status...
    auto idxLineBeforeNavigation = viewData.idxActiveLine;

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
        case kKey_PageUp :
            OnNavigateUp(dimensions.Height()-2);
            break;
        case kKey_PageDown :
            OnNavigateDown(dimensions.Height()-2);
            break;
        case Keyboard::kKeyCode_Return :
            viewData.editController.NewLine(viewData.idxActiveLine, cursor);
            screen->InvalidateAll();
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

    if (viewData.idxActiveLine > ContentRect().Height()-2) {
        if (!(cursor.position.y < ContentRect().Height()-2)) {
            viewData.viewTopLine += rows;
            viewData.viewBottomLine += rows;
        }
    }

    cursor.position.y = viewData.idxActiveLine - viewData.viewTopLine;
    if (cursor.position.y >= ContentRect().Height()-2) {
        cursor.position.y = ContentRect().Height()-2;
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
    }

    currentLine = lines[viewData.idxActiveLine];
    currentLine->SetActive(true);
}
