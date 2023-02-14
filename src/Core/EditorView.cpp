//
// Created by gnilk on 14.02.23.
//

#include "RuntimeConfig.h"
#include "EditorView.h"

using namespace gedit;

void EditorView::Begin() {
    editorMode.Begin();
}

void EditorView::DrawViewContents() {

    auto &ctx = ViewBase::ContentAreaDrawContext();

    ctx.DrawLines(editorMode.Lines(), 0);

//    DrawLines();
//    if (editorMode.CurrentLine() != nullptr) {
//        ctx.DrawStringAt(0,0, editorMode.CurrentLine()->Buffer().data());
//    }
}

void EditorView::OnKeyPress(gedit::NCursesKeyboardDriverNew::KeyPress keyPress) {
    if (keyPress.isKeyValid) {
        int breakme;
        breakme = 1;
    }
    editorMode.HandleKeyPress(keyPress);
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
