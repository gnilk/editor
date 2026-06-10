//
// Created by gnilk on 07.11.23.
//
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/TextBuffer.h"
#include "Core/Controllers/EditController.h"


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
DLL_EXPORT int test_document_switch_preserves_scroll(ITesting *t);
DLL_EXPORT int test_document_switch_preserves_undo(ITesting *t);
// undo must act on the EDITED document, not whichever document happens to be active
DLL_EXPORT int test_document_undo_independent_of_active(ITesting *t);

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

    // Resolved actions now go straight to the document (the controller no longer proxies OnAction).
    document->OnAction(actionPageDown);
    document->OnAction(actionPageDown);
    document->OnAction(actionPageDown);
    document->OnAction(actionLineUp);
    document->OnAction(actionLineUp);
    document->OnAction(actionLineUp);   // we should be on line 23 now

    auto lcBefore = document->GetLineCursor();

    // Select two lines
    document->OnAction(actionShiftLineDown);
    document->OnAction(actionShiftLineDown);

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

// A document switch in the running app re-seeds the view geometry: the single EditorView re-points
// at the now-active document and calls document->OnViewInit (EditorView::ReInitView). That re-seed
// must NOT discard the document's saved scroll position - a caret scrolled deep into the buffer has
// to stay visible after the switch. Unlike the cursor/selection cases above (which model a "switch"
// as merely operating on the other Document object), this case replicates the REAL switch by
// re-calling OnViewInit, which is where the bug lives. Pre-fix, OnViewInit forced viewTopLine=0, so
// the active line fell outside the viewport and the caret drew off the top of the view.
DLL_EXPORT int test_document_switch_preserves_scroll(ITesting *t) {
    auto document = CreateEmptyDocument(t);
    gedit::Rect rect(20,20);   // width,height - height 20 => a viewport of 20 lines
    document->OnViewInit(rect);
    FillEmptyDocument(document, 200, 40);

    // Scroll deep into the buffer: drive the active line well past one screen height so the viewport
    // has to scroll (viewTopLine > 0) to keep the caret visible.
    for (int i = 0; i < 60; ++i) {
        document->OnAction(actionLineDown);
    }
    auto &lc = document->GetLineCursor();
    TR_ASSERT(t, lc.idxActiveLine == 60);
    TR_ASSERT(t, lc.IsInside(lc.idxActiveLine));   // sanity: caret is visible before the switch
    auto savedTop = lc.viewTopLine;
    TR_ASSERT(t, savedTop > 0);                     // we genuinely scrolled

    // Replicate the real document-switch re-point: the view re-seeds geometry on the now-active doc.
    document->OnViewInit(rect);

    // The saved scroll anchor must survive the re-seed, and the active line must STILL be visible
    // (the caret cannot draw off the viewport).
    auto &lc2 = document->GetLineCursor();
    TR_ASSERT(t, lc2.idxActiveLine == 60);
    TR_ASSERT(t, lc2.viewTopLine == savedTop);
    TR_ASSERT(t, lc2.IsInside(lc2.idxActiveLine));

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

// Switching the active document away and back preserves each document's own undo history. This
// drives the real Editor::SetActiveDocument switch (what the running app does). NOTE: undo capture
// used to be coupled to the Editor's *active* document (UndoHistory reached GetActiveDocument);
// that coupling has since been removed - undo now captures from the owning document's view-state +
// buffer (see test_document_undo_independent_of_active). The active-switch here still exercises the
// genuine switch path.
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

// Undo must operate on the document being edited, regardless of which document is currently the
// Editor's active one. (Discriminating test for the undo-decoupling fix: on the pre-fix code the
// undo SNAPSHOT was captured via Editor::GetActiveDocument(), so editing a non-active document and
// undoing restores the WRONG document's line content into it.)
DLL_EXPORT int test_document_undo_independent_of_active(ITesting *t) {
    auto &workspace = *Editor::Instance().GetWorkspace();
    auto prevActive = Editor::Instance().GetActiveDocument();

    auto docEdited = CreateEmptyDocument(t);   // the one we actually edit
    auto docActive = CreateEmptyDocument(t);   // a DIFFERENT, active decoy with a different shape
    gedit::Rect rect(20,20);
    docEdited->OnViewInit(rect);
    docActive->OnViewInit(rect);
    FillEmptyDocument(docEdited, 40, 40);      // active line length 40
    FillEmptyDocument(docActive, 10, 5);       // active line length 5 - distinct, exposes cross-capture

    workspace.AddOpenDocument(docEdited);
    workspace.AddOpenDocument(docActive);
    Editor::Instance().SetActiveDocument(docActive);   // the decoy is active, not the edited doc

    auto ctrl = EditController::Create(docEdited);
    static KeyPress keyX = {
            .isKeyValid = true, .isSpecialKey = false, .modifiers = 0, .key = U'X', .specialKey = 0 };

    auto lenBefore = docEdited->ActiveLine()->Length();
    auto &lc = docEdited->GetLineCursor();
    ctrl->HandleKeyPress(lc.cursor, lc.idxActiveLine, keyX);
    TR_ASSERT(t, docEdited->ActiveLine()->Length() == lenBefore + 1);

    // Undo the edited document while a different document is active - it must revert docEdited's line
    // to its own previous content (length 40), not the active decoy's (length 5).
    docEdited->OnAction(actionUndo);
    TR_ASSERT(t, docEdited->ActiveLine()->Length() == lenBefore);

    workspace.RemoveOpenDocument(docEdited);
    workspace.RemoveOpenDocument(docActive);
    Editor::Instance().SetActiveDocument(prevActive);

    return kTR_Pass;
}
