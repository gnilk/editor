//
// Created by gnilk on 07.11.23.
//
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/TextBuffer.h"


using namespace gedit;

extern "C" {
DLL_EXPORT int test_document(ITesting *t);
DLL_EXPORT int test_document_create(ITesting *t);
// 'empty' - are all on an empty textbuffer
DLL_EXPORT int test_document_empty_linefunc(ITesting *t);
DLL_EXPORT int test_document_empty_selfunc(ITesting *t);
// 'text' - is with a bunch of text
DLL_EXPORT int test_document_text_linefunc(ITesting *t);
DLL_EXPORT int test_document_text_selfunc(ITesting *t);

// 'ins' - insert action in to the document
DLL_EXPORT int test_document_ins_keypress(ITesting *t);

DLL_EXPORT int test_document_delete_text(ITesting *t);

// 'switch' - per-document state survives roving the single view across N documents (Phase 2 contract)
DLL_EXPORT int test_document_switch_preserves_cursor(ITesting *t);
DLL_EXPORT int test_document_switch_preserves_selection(ITesting *t);
DLL_EXPORT int test_document_switch_preserves_undo(ITesting *t);

}

// Define some common actions, this will trigger side-effects in the document
static KeyPressAction actionLineDown = {gedit::kAction::kActionLineDown};
static KeyPressAction actionPageDown = {gedit::kAction::kActionPageDown};
static KeyPressAction actionLineUp = {gedit::kAction::kActionLineUp};
static KeyPressAction actionPageUp = {gedit::kAction::kActionPageUp};
static KeyPressAction actionShiftLineDown =
        {
                .action = gedit::kAction::kActionLineDown,
                .actionModifier = kActionModifier::kActionModifierSelection,
                .modifierMask = Keyboard::ShiftMask()
        };
static KeyPressAction actionShiftLineUp =
        {
                .action = gedit::kAction::kActionLineUp,
                .actionModifier = kActionModifier::kActionModifierSelection,
                .modifierMask = Keyboard::ShiftMask()
        };
static KeyPressAction actionUndo = {gedit::kAction::kActionUndo};


DLL_EXPORT int test_document(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);
    // Ensure we test with a known vertical navigation document..
    Config::Instance()["editorview"].SetBool("pgupdown_content_first", true);
    return kTR_Pass;
}

static Document::Ref CreateEmptyDocument(ITesting *t) {
    auto textBuffer = TextBuffer::CreateEmptyBuffer();
    TR_ASSERT(t, textBuffer != nullptr);
    auto document = Document::Create(textBuffer);
    TR_ASSERT(t, document != nullptr);
    return document;
}

// fill the text buffer in the document with predictable content..
static void FillEmptyDocument(Document::Ref document, size_t nLines, size_t lineLength) {
    // Remove first line - we don't want this to interfere
    document->GetTextBuffer()->DeleteLineAt(0);

    for(size_t i = 0; i<nLines;++i) {
        std::string str(lineLength, std::to_string(i).at(0));
        document->GetTextBuffer()->AddLineUTF8(str.c_str());
    }
}

DLL_EXPORT int test_document_create(ITesting *t) {
    // Don't use 'CreateEmptyDocument' - this one does a bit more agressive testing of document and the textbuffer
    auto textBuffer = TextBuffer::CreateEmptyBuffer();
    TR_ASSERT(t, textBuffer != nullptr);
    auto document = Document::Create(textBuffer);
    TR_ASSERT(t, document != nullptr);
    // The first line should always be available
    TR_ASSERT(t, document->Lines().size() == 1);
    TR_ASSERT(t, document->GetTextBuffer() == textBuffer);
    TR_ASSERT(t, document->GetTextBuffer()->HaveLanguage());

    return kTR_Pass;
}

DLL_EXPORT int test_document_empty_linefunc(ITesting *t) {
    auto document = CreateEmptyDocument(t);
    // The first line should always be available
    TR_ASSERT(t, document->Lines().size() == 1);
    TR_ASSERT(t, document->LineAt(2) == nullptr);
    TR_ASSERT(t, document->ActiveLine() != nullptr);
    TR_ASSERT(t, document->GetLineCursorRef()->idxActiveLine == 0);
    TR_ASSERT(t, document->GetLineCursorRef()->cursor.position.x == 0);

    return kTR_Pass;

}

DLL_EXPORT int test_document_empty_selfunc(ITesting *t) {
    auto document = CreateEmptyDocument(t);
    // The first line should always be available
    TR_ASSERT(t, document->IsSelectionActive() == false);

    // Create and cancel a selection
    document->BeginSelection();
    TR_ASSERT(t, document->IsSelectionActive() == true);
    document->CancelSelection();
    TR_ASSERT(t, document->IsSelectionActive() == false);


    document->BeginSelection();
    TR_ASSERT(t, document->IsSelectionActive() == true);
    document->OnAction(actionLineDown);
    // this should cancel the selection as the shift modifier isn't pressed...
    TR_ASSERT(t, document->IsSelectionActive() == false);

    // This should trigger a full line marking of the single line of text we have (empty)
    document->BeginSelection();
    TR_ASSERT(t, document->IsSelectionActive() == true);
    document->OnAction(actionShiftLineDown);
    // this should cancel the selection as the shift modifier isn't pressed...
    TR_ASSERT(t, document->IsSelectionActive() == true);
    document->CancelSelection();
    TR_ASSERT(t, document->IsSelectionActive() == false);

    return kTR_Pass;
}

DLL_EXPORT int test_document_text_linefunc(ITesting *t) {
    auto document = CreateEmptyDocument(t);
    // The 'view' rect (this is the size of the visible area of the text buffer)
    // it is used to calculate the actual viewing area for the renderer
    // needed for navigation testing since cursor updates will move it around..
    // this also defines the height of a 'page' when dealing with page-down/up
    gedit::Rect rect(20,20);
    document->OnViewInit(rect);

    // Insert 40 lines with 40 chars
    FillEmptyDocument(document, 40, 40);
    // The first line should always be available
    TR_ASSERT(t, document->Lines().size() == 40);  // Initial line is still there..
    TR_ASSERT(t, document->ActiveLine() != nullptr);
    TR_ASSERT(t, document->ActiveLine()->Length() == 40);
    TR_ASSERT(t, document->GetLineCursorRef()->idxActiveLine == 0);
    TR_ASSERT(t, document->GetLineCursorRef()->cursor.position.x == 0);

    document->OnAction(actionLineDown);
    TR_ASSERT(t, document->ActiveLine() != nullptr);
    TR_ASSERT(t, document->GetLineCursorRef()->idxActiveLine == 1);
    TR_ASSERT(t, document->GetLineCursorRef()->cursor.position.y == 1);

    document->OnAction(actionPageDown);
    TR_ASSERT(t, document->ActiveLine() != nullptr);

    // Content-first (CLion/Sublime) navigation: a page is 'height-1' rows - one line of overlap is
    // kept from the previous visual chunk. The view scrolls by that amount while the caret keeps its
    // on-screen row, so the active line advances by exactly 'height-1'. We were one line down
    // (active line 1, screen row 1) -> active line 1+19 = 20, screen row unchanged at 1.
    TR_ASSERT(t, document->GetLineCursorRef()->idxActiveLine == 20);
    TR_ASSERT(t, document->GetLineCursorRef()->cursor.position.y == 1);

    // PageUp is the exact inverse of PageDown, so this returns us to where we were (one line down)
    document->OnAction(actionPageUp);
    TR_ASSERT(t, document->ActiveLine() != nullptr);
    TR_ASSERT(t, document->GetLineCursorRef()->idxActiveLine == 1);
    TR_ASSERT(t, document->GetLineCursorRef()->cursor.position.y == 1);

    // We are one line down - moving a whole page up should put us on top - clipping to boundary
    document->OnAction(actionPageUp);
    TR_ASSERT(t, document->ActiveLine() != nullptr);
    TR_ASSERT(t, document->GetLineCursorRef()->idxActiveLine == 0);
    TR_ASSERT(t, document->GetLineCursorRef()->cursor.position.y == 0);

    return kTR_Pass;
}

DLL_EXPORT int test_document_text_selfunc(ITesting *t) {
    auto document = CreateEmptyDocument(t);

    gedit::Rect rect(20,20);
    document->OnViewInit(rect);

    // Insert 40 lines with 40 chars
    FillEmptyDocument(document, 40, 40);


    // This will start the selection
    document->OnAction(actionShiftLineDown);   // select one line
    TR_ASSERT(t, document->ActiveLine() != nullptr);
    TR_ASSERT(t, document->GetLineCursorRef()->idxActiveLine == 1);
    TR_ASSERT(t, document->GetLineCursorRef()->cursor.position.y == 1);

    // Selection should now be active
    TR_ASSERT(t, document->IsSelectionActive());
    auto &selection = document->GetSelection();
    TR_ASSERT(t, selection.IsActive());
    TR_ASSERT(t, selection.GetStart().y == 0);
    TR_ASSERT(t, selection.GetEnd().y == 1);

    // Continue selection
    document->OnAction(actionShiftLineDown);   // select one line
    TR_ASSERT(t, selection.IsActive());
    TR_ASSERT(t, selection.GetStart().y == 0);
    TR_ASSERT(t, selection.GetEnd().y == 2);

    // Test if we can copy it
    auto &clipboard = Editor::Instance().GetClipBoard();
    clipboard.CopyFromBuffer(document->GetTextBuffer(), selection.GetStart(), selection.GetEnd());
    auto item = clipboard.Top();
    TR_ASSERT(t, item->GetLineCount() == 2);

    // This should cancel the selection
    document->OnAction(actionLineDown);
    TR_ASSERT(t, document->IsSelectionActive() == false);

    return kTR_Pass;
}

DLL_EXPORT int test_document_ins_keypress(ITesting *t) {
    auto document = CreateEmptyDocument(t);

    gedit::Rect rect(20,20);
    document->OnViewInit(rect);

    // Insert 40 lines with 40 chars
    FillEmptyDocument(document, 40, 40);

    auto controller = EditController::Create(document);

    static KeyPress keyPress = {
            .isKeyValid = true,
            .isSpecialKey = false,
            .modifiers = 0,
            .key = U'A',
            .specialKey = 0
    };

    auto szLineBefore = document->ActiveLine()->Length();
//    controller->DefaultEditLine(document->GetCursor(), document->ActiveLine(), keyPress, false);
    auto &lc = document->GetLineCursor();
    controller->HandleKeyPress(lc.cursor, lc.idxActiveLine, keyPress);
    auto szLineAfter = document->ActiveLine()->Length();
    TR_ASSERT(t, szLineAfter > szLineBefore);
    TR_ASSERT(t, szLineAfter == (szLineBefore + 1));

    return kTR_Pass;
}

DLL_EXPORT int test_document_delete_text(ITesting *t) {
    auto document = CreateEmptyDocument(t);

    gedit::Rect rect(20,20);
    document->OnViewInit(rect);

    // Insert 40 lines with 40 chars
    FillEmptyDocument(document, 40, 40);


    static KeyPress keyPressDelete = {
        .isKeyValid = true,
        .isSpecialKey = true,
        .modifiers = 0,
        .key = 0,
        .specialKey = Keyboard::kKeyCode_DeleteForward
};
    auto controller = EditController::Create(document);

    controller->OnAction(actionPageDown);
    controller->OnAction(actionPageDown);
    controller->OnAction(actionPageDown);
    controller->OnAction(actionLineUp);
    controller->OnAction(actionLineUp);
    controller->OnAction(actionLineUp);   // we should be on line 23 now

    auto lcBefore = document->GetLineCursor();

    // Select two lines
    controller->OnAction(actionShiftLineDown);
    controller->OnAction(actionShiftLineDown);

    auto lc = document->GetLineCursor();
    //controller->HandleKeyPress(lc.cursor, lc.idxActiveLine, keyPressDelete);
    controller->OnKeyPress(keyPressDelete);

    auto lcAfter = document->GetLineCursor();

    // Deleting a forward selection leaves the caret on the selection's start line, so the *active
    // line* (buffer coordinate) is preserved.
    TR_ASSERT(t, lcBefore.idxActiveLine == lcAfter.idxActiveLine);

    // The *screen row* (cursor.position.y) is NOT necessarily preserved: here we delete near EOF, so
    // the viewport re-anchors (it cannot render past the new last line) and the caret's on-screen row
    // shifts. The real invariant is that the caret stays visible on its active line, i.e. the screen
    // row equals the active line minus the top of the view.
    TR_ASSERT(t, lcAfter.cursor.position.y == (int)(lcAfter.idxActiveLine) - lcAfter.viewTopLine);

    return kTR_Pass;
}

// --- Phase 2 contract ---------------------------------------------------------------------------
// The single EditorView roves over N open documents, re-pointing at whichever is active. So
// per-document editing state (cursor, selection, undo) must persist per document while the view
// moves between them. These cases pin that behavior: operate on one document, "switch" by operating
// on a second, switch back -> the first document's state is exactly as left, independent of the
// second. P2.1/P2.2 move that state into ViewState/EditState referenced by Document; these tests
// must stay green across that move. (Two separate Document objects stand in for the two open docs;
// there is no document-switch API yet - the view's `document = GetActiveDocument()` re-point IS the
// switch, see docs/workspace-refactor-plan.md.)

DLL_EXPORT int test_document_switch_preserves_cursor(ITesting *t) {
    auto docA = CreateEmptyDocument(t);
    auto docB = CreateEmptyDocument(t);
    gedit::Rect rect(20,20);
    docA->OnViewInit(rect);
    docB->OnViewInit(rect);
    FillEmptyDocument(docA, 40, 40);
    FillEmptyDocument(docB, 40, 40);

    // Position the caret in A (the active document)
    docA->OnAction(actionLineDown);
    docA->OnAction(actionLineDown);
    docA->OnAction(actionLineDown);
    auto savedA = docA->GetLineCursor();    // snapshot copy
    TR_ASSERT(t, savedA.idxActiveLine == 3);

    // 'Switch' to B and move it somewhere different - proves the two carets are independent
    docB->OnAction(actionPageDown);
    TR_ASSERT(t, docB->GetLineCursor().idxActiveLine != savedA.idxActiveLine);

    // 'Switch' back to A - its caret is exactly where we left it
    auto nowA = docA->GetLineCursor();
    TR_ASSERT(t, nowA.idxActiveLine == savedA.idxActiveLine);
    TR_ASSERT(t, nowA.cursor.position.x == savedA.cursor.position.x);
    TR_ASSERT(t, nowA.cursor.position.y == savedA.cursor.position.y);

    return kTR_Pass;
}

DLL_EXPORT int test_document_switch_preserves_selection(ITesting *t) {
    auto docA = CreateEmptyDocument(t);
    auto docB = CreateEmptyDocument(t);
    gedit::Rect rect(20,20);
    docA->OnViewInit(rect);
    docB->OnViewInit(rect);
    FillEmptyDocument(docA, 40, 40);
    FillEmptyDocument(docB, 40, 40);

    // Make a selection in A
    docA->OnAction(actionShiftLineDown);
    docA->OnAction(actionShiftLineDown);
    TR_ASSERT(t, docA->IsSelectionActive());
    auto savedStart = docA->GetSelection().GetStart();  // Point copies
    auto savedEnd = docA->GetSelection().GetEnd();

    // B has its own (empty) selection state - operating on B must not touch A's selection
    TR_ASSERT(t, docB->IsSelectionActive() == false);
    docB->OnAction(actionLineDown);
    TR_ASSERT(t, docB->IsSelectionActive() == false);

    // 'Switch' back to A - the selection is intact
    TR_ASSERT(t, docA->IsSelectionActive());
    TR_ASSERT(t, docA->GetSelection().GetStart().x == savedStart.x);
    TR_ASSERT(t, docA->GetSelection().GetStart().y == savedStart.y);
    TR_ASSERT(t, docA->GetSelection().GetEnd().x == savedEnd.x);
    TR_ASSERT(t, docA->GetSelection().GetEnd().y == savedEnd.y);

    return kTR_Pass;
}

// NOTE (Phase 2 finding): undo capture is coupled to the *Editor's active document*, not to the
// document the controller is bound to - UndoItemSingle::Initialize snapshots the line via
// Editor::Instance().GetActiveDocument() (UndoHistory.cpp). So this contract must drive the REAL
// switch (Editor::SetActiveDocument), which is what the running app does. That global coupling is a
// smell P2.2 (EditState extraction) should resolve: the history is per-document and should capture
// from `this` document, not the ambient active one.
DLL_EXPORT int test_document_switch_preserves_undo(ITesting *t) {
    auto &workspace = *Editor::Instance().GetWorkspace();
    auto prevActive = Editor::Instance().GetActiveDocument();

    auto docA = CreateEmptyDocument(t);
    auto docB = CreateEmptyDocument(t);
    gedit::Rect rect(20,20);
    docA->OnViewInit(rect);
    docB->OnViewInit(rect);
    FillEmptyDocument(docA, 40, 40);
    FillEmptyDocument(docB, 40, 40);

    // Register both as open so the active-document switch (below) takes effect.
    workspace.AddOpenDocument(docA);
    workspace.AddOpenDocument(docB);

    auto ctrlA = EditController::Create(docA);
    auto ctrlB = EditController::Create(docB);

    static KeyPress keyA = {
            .isKeyValid = true, .isSpecialKey = false, .modifiers = 0, .key = U'A', .specialKey = 0 };
    static KeyPress keyB = {
            .isKeyValid = true, .isSpecialKey = false, .modifiers = 0, .key = U'B', .specialKey = 0 };

    // Make A the active document, then edit it (registers one undo item in A's history)
    Editor::Instance().SetActiveDocument(docA);
    auto lenABefore = docA->ActiveLine()->Length();
    auto &lcA = docA->GetLineCursor();
    ctrlA->HandleKeyPress(lcA.cursor, lcA.idxActiveLine, keyA);
    TR_ASSERT(t, docA->ActiveLine()->Length() == lenABefore + 1);

    // Switch to B (the single view re-points) and edit it - B has its own separate undo history
    Editor::Instance().SetActiveDocument(docB);
    auto lenBBefore = docB->ActiveLine()->Length();
    auto &lcB = docB->GetLineCursor();
    ctrlB->HandleKeyPress(lcB.cursor, lcB.idxActiveLine, keyB);
    TR_ASSERT(t, docB->ActiveLine()->Length() == lenBBefore + 1);

    // Switch back to A and undo - A's edit reverts; B is untouched (per-document undo history)
    Editor::Instance().SetActiveDocument(docA);
    docA->OnAction(actionUndo);
    TR_ASSERT(t, docA->ActiveLine()->Length() == lenABefore);
    TR_ASSERT(t, docB->ActiveLine()->Length() == lenBBefore + 1);

    // Restore the singleton's workspace state so we don't pollute later cases.
    workspace.RemoveOpenDocument(docA);
    workspace.RemoveOpenDocument(docB);
    Editor::Instance().SetActiveDocument(prevActive);

    return kTR_Pass;
}
