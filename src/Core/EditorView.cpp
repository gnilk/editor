//
// Created by gnilk on 14.02.23.
//

#include "RuntimeConfig.h"
#include "EditorView.h"

using namespace gedit;

void EditorView::Begin() {
    //editorMode.Begin();
}

void EditorView::DrawViewContents() {
    auto &ctx = ViewBase::ContentAreaDrawContext();
    ctx.DrawLines(editController.Lines(),0);


    // Update cursor screen position, need to translate to screen coords..
    Cursor screenCursor;
    screenCursor.position = ctx.ToScreen(cursor.position);

    auto screen = RuntimeConfig::Instance().Screen();
    screen->SetCursor(screenCursor);

}

void EditorView::OnKeyPress(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) {
    if (keyPress.isKeyValid) {
        int breakme;
        breakme = 1;
    }
    if (editController.HandleKeyPress(idxActiveLine, keyPress)) {
        return;
    }
    UpdateNavigation(keyPress);
}

void EditorView::UpdateNavigation(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) {
    auto screen = RuntimeConfig::Instance().Screen();
    auto dimensions = Dimensions();

    auto currentLine = editController.LineAt(idxActiveLine);

    // save current line - as it will update with navigation
    // we need it when we update the selection status...
    auto idxLineBeforeNavigation = idxActiveLine;

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
            editController.NewLine(idxActiveLine, cursor);
            screen->InvalidateAll();
            break;
        default:
            // Not navigation
            return;
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

    return;

}

void EditorView::OnNavigateDown(int rows) {
    auto currentLine = editController.LineAt(idxActiveLine);
    currentLine->SetActive(false);
    auto &lines = editController.Lines();

    idxActiveLine+=rows;
    if (idxActiveLine >= lines.size()) {
        idxActiveLine = lines.size()-1;
    }

    currentLine = lines[idxActiveLine];
    currentLine->SetActive(true);

    cursor.position.y = idxActiveLine;
    if (cursor.position.y > ContentRect().Height()) {
        cursor.position.y = ContentRect().Height();
    }
}

void EditorView::OnNavigateUp(int rows) {
    auto currentLine = editController.LineAt(idxActiveLine);
    currentLine->SetActive(false);
    auto &lines = editController.Lines();

    idxActiveLine -= rows;
    if (idxActiveLine < 0) {
        idxActiveLine = 0;
    }

    cursor.position.y = idxActiveLine;
    if (cursor.position.y > ContentRect().Height()) {
        cursor.position.y = ContentRect().Height();
    }

    currentLine = lines[idxActiveLine];
    currentLine->SetActive(true);
}




void EditorView::DrawLines() {
    auto screen = RuntimeConfig::Instance().Screen();
    auto &lines = editorMode.Lines();


//    if (selection.IsActive()) {
//
//        ClearSelectedLines();
//        screen->InvalidateAll();
//
//        int idxStart = selection.idxStartLine;
//        int idxEnd = selection.idxEndLine;
//        if (idxStart > idxEnd) {
//            std::swap(idxStart, idxEnd);
//        }
//        for(int i=idxStart;i<idxEnd;i++) {
//            lines[i]->SetSelected(true);
//        }
//    }

//    screen->DrawGutter(idxActiveLine);
//    screen->SetCursorColumn(cursor.activeColumn);

    // This should go directly to screen...
    screen->DrawLines(lines,0);

    // FIXME: Status bar should have '<buffer>:<filename> | <type> | <indent size> | ..  perhaps..
    // like: 0:config.yml

    // Bottom bar: status
//    auto indent = currentLine->Indent();
    char tmp[256];
//    snprintf(tmp, 256, "Goat Editor v0.1 - lc: %d (%s)- al: %d - ts: %d - s: %s (%d - %d)",
//             (int)lastChar.data.code, keyname((int)lastChar.rawCode), idxActiveLine, indent,
//             selection.IsActive()?"y":"n", selection.idxStartLine, selection.idxEndLine);
    snprintf(tmp, 256,"Goat Editor v0.1 - rendering with Views");
    // TODO: Pad this to end of screen
    screen->DrawBottomBar(tmp);


    // Top bar: Active buffers bar
    screen->DrawTopBar("1:file.txt | 2:main.cpp | 3:readme.md | 4:CMakeLists.txt | 5:dummy.xyz");

}
