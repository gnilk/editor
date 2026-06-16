//
// Created by gnilk on 07.11.23.
//
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/TextBuffer.h"
#include "Core/Editor/Controllers/EditController.h"
#include "Core/Language/AutoPairCache.h"
#include "Core/Language/IndentCache.h"


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
DLL_EXPORT int test_document_viewmode(ITesting *t);

// 'autopair' - the AutoPairEngine wired through EditController (insert-pair / skip-over / backspace-pair)
DLL_EXPORT int test_document_autopair_insertpair(ITesting *t);
DLL_EXPORT int test_document_autopair_skipover(ITesting *t);
DLL_EXPORT int test_document_autopair_nopair_midword(ITesting *t);
DLL_EXPORT int test_document_autopair_backspace(ITesting *t);
DLL_EXPORT int test_document_autopair_wrap(ITesting *t);
DLL_EXPORT int test_document_autopair_wrap_cursor(ITesting *t);

// 'indent' - the IndentEngine wired through Document::NewLine (newline auto-indent + '{|}' expansion)
DLL_EXPORT int test_document_indent_newline(ITesting *t);
DLL_EXPORT int test_document_indent_dedent(ITesting *t);
DLL_EXPORT int test_document_indent_expand(ITesting *t);
DLL_EXPORT int test_document_indent_electric(ITesting *t);
DLL_EXPORT int test_document_indent_electric_midline(ITesting *t);
DLL_EXPORT int test_document_indent_electric_emptyline(ITesting *t);

// 'reformat' - ReindentLineRange + the ReformatLine/ReformatBlock actions (RF.2)
DLL_EXPORT int test_document_reformat_line(ITesting *t);
DLL_EXPORT int test_document_reformat_cursor(ITesting *t);
DLL_EXPORT int test_document_reformat_commentbraces(ITesting *t);
DLL_EXPORT int test_document_reformat_commentshift(ITesting *t);
DLL_EXPORT int test_document_reformat_range(ITesting *t);
DLL_EXPORT int test_document_reformat_undo(ITesting *t);
// block-surround (RF.3 / H.18b)
DLL_EXPORT int test_document_reformat_surround(ITesting *t);
DLL_EXPORT int test_document_reformat_surround_cursor(ITesting *t);
DLL_EXPORT int test_document_reformat_surround_undo(ITesting *t);
// enclosing-block finder (RF.2b)
DLL_EXPORT int test_document_reformat_block_enclosing(ITesting *t);

// 'cut' - whole-line cut/paste preserves the trailing newline (open-bugs.md #4 / feel-check E.18)
DLL_EXPORT int test_document_cut_paste_linewise(ITesting *t);
DLL_EXPORT int test_document_cut_paste_linewise_undo(ITesting *t);
DLL_EXPORT int test_document_cut_paste_charwise(ITesting *t);
DLL_EXPORT int test_document_cut_paste_funcbody(ITesting *t);

}

// Define some common actions, this will trigger side-effects in the document
static EditorAction actionLineDown = {gedit::kAction::kActionLineDown};
static EditorAction actionPageDown = {gedit::kAction::kActionPageDown};
static EditorAction actionLineUp = {gedit::kAction::kActionLineUp};
static EditorAction actionPageUp = {gedit::kAction::kActionPageUp};
static EditorAction actionShiftLineDown =
        {
                .action = gedit::kAction::kActionLineDown,
                .actionModifier = kActionModifier::kActionModifierSelection,
                .modifierMask = Keyboard::ShiftMask()
        };
static EditorAction actionShiftLineUp =
        {
                .action = gedit::kAction::kActionLineUp,
                .actionModifier = kActionModifier::kActionModifierSelection,
                .modifierMask = Keyboard::ShiftMask()
        };
static EditorAction actionUndo = {gedit::kAction::kActionUndo};
static EditorAction actionShiftLineRight =
        {
                .action = gedit::kAction::kActionLineRight,
                .actionModifier = kActionModifier::kActionModifierSelection,
                .modifierMask = Keyboard::ShiftMask()
        };
static EditorAction actionCut = {gedit::kAction::kActionCutToClipboard};
static EditorAction actionPaste = {gedit::kAction::kActionPasteFromClipboard};
static EditorAction actionShiftLineEnd =
        {
                .action = gedit::kAction::kActionLineEnd,
                .actionModifier = kActionModifier::kActionModifierSelection,
                .modifierMask = Keyboard::ShiftMask()
        };


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
// second. P2.1/P2.2 move that state into DocumentViewState/EditState referenced by Document; these tests
// must stay green across that move. (Two separate Document objects stand in for the two open docs;
// there is no document-switch API yet - the view's `document = GetActiveDocument()` re-point IS the
// switch.)

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

// H.2 of the HexView spike: view mode (Text/Hex) is per-view state on the ViewState, switched by the
// explicit kActionViewModeText / kActionViewModeHex actions. The render path is H.3; here we pin that
// the state defaults to Text and flips cleanly. (The action->view dispatch lives in EditorView.)
DLL_EXPORT int test_document_viewmode(ITesting *t) {
    auto document = CreateEmptyDocument(t);
    // Default presentation is text.
    TR_ASSERT(t, document->GetViewMode() == DocumentViewMode::kText);

    document->SetViewMode(DocumentViewMode::kHex);
    TR_ASSERT(t, document->GetViewMode() == DocumentViewMode::kHex);

    document->SetViewMode(DocumentViewMode::kText);
    TR_ASSERT(t, document->GetViewMode() == DocumentViewMode::kText);
    return kTR_Pass;
}

// --- Auto-pairing wired through EditController ---------------------------------------------------------
// Drives the real edit path (controller->HandleKeyPress / OnKeyPress) with a hermetic pair table keyed by
// the document's own language config name. The decision logic itself is covered by test_autopair; these
// pin the *application* of an Action to the buffer + cursor.

static KeyPress KP(char32_t ch) {
    KeyPress kp = {};
    kp.isKeyValid = true;
    kp.isSpecialKey = false;
    kp.key = ch;
    return kp;
}

// An empty document whose language has a loaded pair table for "(" / "{".
static Document::Ref CreateAutoPairDoc(ITesting *t) {
    auto document = CreateEmptyDocument(t);
    gedit::Rect rect(40, 40);
    document->OnViewInit(rect);

    auto langName = document->GetTextBuffer()->GetLanguage().GetConfigNodeName();
    TR_ASSERT(t, !langName.empty());
    std::string yaml =
        "autopairs:\n  " + langName + ":\n"
        "    auto_close_before: [eol, whitespace, \")\"]\n"
        "    suppress_in: [comment]\n"
        "    pairs:\n"
        "      - { open: \"(\", close: \")\" }\n"
        "      - { open: \"{\", close: \"}\" }\n";
    TR_ASSERT(t, AutoPairCache::Instance().LoadFromString(yaml));
    return document;
}

// Type '(' at end-of-line -> "()" with the cursor between.
DLL_EXPORT int test_document_autopair_insertpair(ITesting *t) {
    auto document = CreateAutoPairDoc(t);
    auto controller = EditController::Create(document);
    auto &lc = document->GetLineCursor();

    auto kp = KP(U'(');
    controller->HandleKeyPress(lc.cursor, lc.idxActiveLine, kp);

    TR_ASSERT(t, document->ActiveLine()->Buffer() == U"()");
    TR_ASSERT(t, lc.cursor.position.x == 1);   // between the pair
    AutoPairCache::Instance().Clear();
    return kTR_Pass;
}

// Typing the closer when it already sits to the right steps over it (no insert).
DLL_EXPORT int test_document_autopair_skipover(ITesting *t) {
    auto document = CreateAutoPairDoc(t);
    auto controller = EditController::Create(document);
    auto &lc = document->GetLineCursor();

    auto open = KP(U'(');
    controller->HandleKeyPress(lc.cursor, lc.idxActiveLine, open);  // "()", cursor at 1
    auto close = KP(U')');
    controller->HandleKeyPress(lc.cursor, lc.idxActiveLine, close); // step over

    TR_ASSERT(t, document->ActiveLine()->Buffer() == U"()");        // unchanged
    TR_ASSERT(t, lc.cursor.position.x == 2);                        // moved past the closer
    AutoPairCache::Instance().Clear();
    return kTR_Pass;
}

// No auto-close in the middle of a word (next char is not an invited close-before char).
DLL_EXPORT int test_document_autopair_nopair_midword(ITesting *t) {
    auto document = CreateAutoPairDoc(t);
    auto controller = EditController::Create(document);
    auto &lc = document->GetLineCursor();

    document->ActiveLine()->Append(std::u32string(U"ab"));
    lc.cursor.position.x = 0;                                       // before 'a'

    auto open = KP(U'(');
    controller->HandleKeyPress(lc.cursor, lc.idxActiveLine, open);

    TR_ASSERT(t, document->ActiveLine()->Buffer() == U"(ab");      // just the '(' inserted
    TR_ASSERT(t, lc.cursor.position.x == 1);
    AutoPairCache::Instance().Clear();
    return kTR_Pass;
}

// Backspace between an empty pair deletes both halves.
DLL_EXPORT int test_document_autopair_backspace(ITesting *t) {
    auto document = CreateAutoPairDoc(t);
    auto controller = EditController::Create(document);
    auto &lc = document->GetLineCursor();

    auto open = KP(U'(');
    controller->HandleKeyPress(lc.cursor, lc.idxActiveLine, open);  // "()", cursor at 1

    KeyPress bsp = {};
    bsp.isKeyValid = true;
    bsp.isSpecialKey = true;
    bsp.specialKey = Keyboard::kKeyCode_Backspace;
    controller->OnKeyPress(bsp);

    TR_ASSERT(t, document->ActiveLine()->Buffer() == U"");          // both gone
    TR_ASSERT(t, lc.cursor.position.x == 0);
    AutoPairCache::Instance().Clear();
    return kTR_Pass;
}

// Typing an opener over a selection surrounds it: "ab" -> "(ab)", cursor after the closer.
DLL_EXPORT int test_document_autopair_wrap(ITesting *t) {
    auto document = CreateAutoPairDoc(t);
    auto controller = EditController::Create(document);
    auto &lc = document->GetLineCursor();

    document->ActiveLine()->Append(std::u32string(U"ab"));
    lc.cursor.position.x = 0;

    // Select "ab" via two shift-rights (the realistic selection path).
    document->OnAction(actionShiftLineRight);
    document->OnAction(actionShiftLineRight);
    TR_ASSERT(t, document->IsSelectionActive());

    auto open = KP(U'(');
    controller->OnKeyPress(open);

    TR_ASSERT(t, document->ActiveLine()->Buffer() == U"(ab)");
    TR_ASSERT(t, !document->IsSelectionActive());
    TR_ASSERT(t, lc.cursor.position.x == 4);        // just after the ')'
    AutoPairCache::Instance().Clear();
    return kTR_Pass;
}

// Inline-wrap (single-line selection) must leave the caret on-screen too: position.y is the SCREEN row, so
// on a scrolled view parking it at the absolute line index puts it off-screen. Same defect class as the
// block-surround cursor fix, in EditController::TryWrapSelection's inline branch.
DLL_EXPORT int test_document_autopair_wrap_cursor(ITesting *t) {
    auto document = CreateAutoPairDoc(t);
    FillEmptyDocument(document, 40, 40);
    auto controller = EditController::Create(document);

    document->OnViewInit(gedit::Rect(40, 10));     // short view...
    auto &lc = document->GetLineCursor();
    lc.viewTopLine = 20;                           // ...scrolled down
    lc.viewBottomLine = 30;
    lc.idxActiveLine = 25;
    lc.cursor.position = { 0, 25 - 20 };

    // Select two chars on the single active line, then wrap with '(' -> inline branch (end.y == start.y).
    document->OnAction(actionShiftLineRight);
    document->OnAction(actionShiftLineRight);
    TR_ASSERT(t, document->IsSelectionActive());

    auto open = KP(U'(');
    controller->OnKeyPress(open);

    TR_ASSERT(t, !document->IsSelectionActive());
    TR_ASSERT(t, lc.cursor.position.y == (int)lc.idxActiveLine - (int)lc.viewTopLine);
    AutoPairCache::Instance().Clear();
    return kTR_Pass;
}

// --- Auto-indent wired through Document::NewLine -------------------------------------------------------
// Drives the real newline path with a hermetic indent table keyed by the document's language config name.
// The decision logic is covered by test_indent; these pin the *application* of an Action to buffer + cursor.

static Document::Ref CreateIndentDoc(ITesting *t) {
    auto document = CreateEmptyDocument(t);
    gedit::Rect rect(40, 40);
    document->OnViewInit(rect);

    auto langName = document->GetTextBuffer()->GetLanguage().GetConfigNodeName();
    TR_ASSERT(t, !langName.empty());
    std::string yaml =
        "indent:\n  " + langName + ":\n"
        "    indent_after: [\"{\", \"[\", \"(\"]\n"
        "    dedent_on:    [\"}\", \"]\", \")\"]\n";
    TR_ASSERT(t, IndentCache::Instance().LoadFromString(yaml));
    return document;
}

// Build an indent-configured document seeded with the given (UTF-8) lines and parsed once.
static Document::Ref CreateReformatDoc(ITesting *t, std::initializer_list<const char *> lines) {
    auto document = CreateIndentDoc(t);
    auto tb = document->GetTextBuffer();
    tb->DeleteLineAt(0);    // drop the initial empty line
    for (auto l : lines) {
        tb->AddLineUTF8(l);
    }
    tb->Reparse();
    return document;
}

// ReformatLine re-derives the current line's indent: a mis-indented body line under an opener snaps to one
// level; the anchor line above and the closer below are untouched.
DLL_EXPORT int test_document_reformat_line(ITesting *t) {
    auto document = CreateReformatDoc(t, {"if (x) {", "            mess();", "}"});
    auto &lc = document->GetLineCursor();
    lc.idxActiveLine = 1;
    lc.cursor.position.x = 0;

    document->OnAction({gedit::kAction::kActionReformatLine});

    TR_ASSERT(t, document->LineAt(1)->Buffer() == U"    mess();");
    TR_ASSERT(t, document->LineAt(0)->Buffer() == U"if (x) {");
    TR_ASSERT(t, document->LineAt(2)->Buffer() == U"}");
    IndentCache::Instance().Clear();
    return kTR_Pass;
}

// After a reformat the caret follows its content: a caret that sat in the indentation rides to the new start
// of text (under-indent case), and one within the text keeps the same character (over-indent case).
DLL_EXPORT int test_document_reformat_cursor(ITesting *t) {
    // Under-indent: caret at col 0 in front of flush-left text -> lands at the new start of text (col 4).
    {
        auto document = CreateReformatDoc(t, {"if (x) {", "mess();", "}"});
        auto &lc = document->GetLineCursor();
        lc.idxActiveLine = 1;
        lc.cursor.position.x = 0;
        document->OnAction({gedit::kAction::kActionReformatLine});
        TR_ASSERT(t, document->LineAt(1)->Buffer() == U"    mess();");
        TR_ASSERT(t, document->GetLineCursor().cursor.position.x == 4);
        IndentCache::Instance().Clear();
    }
    // Over-indent: caret on the 's' of "mess" (col 14 over 12 leading) rides to the same 's' (col 6).
    {
        auto document = CreateReformatDoc(t, {"if (x) {", "            mess();", "}"});
        auto &lc = document->GetLineCursor();
        lc.idxActiveLine = 1;
        lc.cursor.position.x = 14;
        document->OnAction({gedit::kAction::kActionReformatLine});
        TR_ASSERT(t, document->LineAt(1)->Buffer() == U"    mess();");
        TR_ASSERT(t, document->GetLineCursor().cursor.position.x == 6);
        IndentCache::Instance().Clear();
    }
    return kTR_Pass;
}

// C.10: braces inside comments/strings must not throw off the reindent walk. Needs a real CPP buffer so the
// tokenizer stamps comment classes + block-comment state depth - a trailing-comment '{' is masked, and a
// block-comment interior line is frozen (byte-faithful), so the code after the comment keeps its true level.
DLL_EXPORT int test_document_reformat_commentbraces(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);
    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto node = workspace->NewDocument("reformat_comments.cpp");
    TR_ASSERT(t, node != nullptr);
    auto document = node->GetDocument();
    auto buffer = document->GetTextBuffer();

    auto langName = buffer->GetLanguage().GetConfigNodeName();
    std::string yaml =
        "indent:\n  " + langName + ":\n"
        "    suppress_in:  [string, comment]\n"
        "    indent_after: [\"{\", \"[\", \"(\"]\n"
        "    dedent_on:    [\"}\", \"]\", \")\"]\n";
    TR_ASSERT(t, IndentCache::Instance().LoadFromString(yaml));

    buffer->DeleteLineAt(0);    // drop the initial empty line
    buffer->AddLineUTF8("void f() {");      // 0
    buffer->AddLineUTF8("    a();");        // 1
    buffer->AddLineUTF8("    d(); // x {"); // 2  trailing-comment brace - must NOT open
    buffer->AddLineUTF8("    b();");        // 3
    buffer->AddLineUTF8("    /*");          // 4
    buffer->AddLineUTF8("    this {");      // 5  block-comment interior - frozen, '{' must NOT open
    buffer->AddLineUTF8("    */");          // 6  frozen
    buffer->AddLineUTF8("    c();");        // 7
    buffer->AddLineUTF8("}");               // 8
    buffer->Reparse();

    document->ReindentLineRange(1, 8);

    TR_ASSERT(t, document->LineAt(1)->Buffer() == U"    a();");
    TR_ASSERT(t, document->LineAt(3)->Buffer() == U"    b();");   // trailing '{' did not open
    TR_ASSERT(t, document->LineAt(5)->Buffer() == U"    this {"); // frozen: bytes untouched
    TR_ASSERT(t, document->LineAt(7)->Buffer() == U"    c();");   // block-comment '{' did not open
    TR_ASSERT(t, document->LineAt(8)->Buffer() == U"}");

    TR_ASSERT(t, workspace->RemoveDocument(node->GetDocument()));
    IndentCache::Instance().Clear();
    return kTR_Pass;
}

// A block comment inside a reformatted range is shifted as a rigid unit to follow its code: the '/*' opener
// lands at code level and the interior + '*/' closer move by the SAME delta (so internal art is preserved).
DLL_EXPORT int test_document_reformat_commentshift(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);
    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto node = workspace->NewDocument("reformat_commentshift.cpp");
    TR_ASSERT(t, node != nullptr);
    auto document = node->GetDocument();
    auto buffer = document->GetTextBuffer();

    auto langName = buffer->GetLanguage().GetConfigNodeName();
    std::string yaml =
        "indent:\n  " + langName + ":\n"
        "    suppress_in:  [string, comment]\n"
        "    indent_after: [\"{\", \"[\", \"(\"]\n"
        "    dedent_on:    [\"}\", \"]\", \")\"]\n";
    TR_ASSERT(t, IndentCache::Instance().LoadFromString(yaml));

    buffer->DeleteLineAt(0);    // drop the initial empty line, then add the (flush-left) source verbatim
    buffer->AddLineUTF8("static void func() {");  // 0
    buffer->AddLineUTF8("printf(\"Hello\\n\");");  // 1
    buffer->AddLineUTF8("if (i > x) {");          // 2
    buffer->AddLineUTF8("/*");                     // 3
    buffer->AddLineUTF8("hello {");                // 4 block-comment interior
    buffer->AddLineUTF8("*/");                     // 5 block-comment closer
    buffer->AddLineUTF8("printf(\"dummy\\n\");");  // 6
    buffer->AddLineUTF8("}");                       // 7
    buffer->AddLineUTF8("}");                       // 8
    buffer->Reparse();

    document->ReindentLineRange(0, 8);

    TR_ASSERT(t, document->LineAt(0)->Buffer() == U"static void func() {");
    TR_ASSERT(t, document->LineAt(1)->Buffer() == U"    printf(\"Hello\\n\");");
    TR_ASSERT(t, document->LineAt(2)->Buffer() == U"    if (i > x) {");
    TR_ASSERT(t, document->LineAt(3)->Buffer() == U"        /*");
    TR_ASSERT(t, document->LineAt(4)->Buffer() == U"        hello {");   // shifted with the opener (+8)
    TR_ASSERT(t, document->LineAt(5)->Buffer() == U"        */");        // closer shifted too
    TR_ASSERT(t, document->LineAt(6)->Buffer() == U"        printf(\"dummy\\n\");");
    TR_ASSERT(t, document->LineAt(7)->Buffer() == U"    }");
    TR_ASSERT(t, document->LineAt(8)->Buffer() == U"}");

    TR_ASSERT(t, workspace->RemoveDocument(node->GetDocument()));
    IndentCache::Instance().Clear();
    return kTR_Pass;
}

// ReindentLineRange re-derives a whole mis-indented block: body lines to one level, the closer back to zero.
DLL_EXPORT int test_document_reformat_range(ITesting *t) {
    auto document = CreateReformatDoc(t, {"void f() {", "  a();", "      b();", "}"});

    document->ReindentLineRange(1, 3);

    TR_ASSERT(t, document->LineAt(0)->Buffer() == U"void f() {");
    TR_ASSERT(t, document->LineAt(1)->Buffer() == U"    a();");
    TR_ASSERT(t, document->LineAt(2)->Buffer() == U"    b();");
    TR_ASSERT(t, document->LineAt(3)->Buffer() == U"}");
    IndentCache::Instance().Clear();
    return kTR_Pass;
}

// One reformat is one undo item: undo restores the original (mis-indented) line verbatim.
DLL_EXPORT int test_document_reformat_undo(ITesting *t) {
    auto document = CreateReformatDoc(t, {"void f() {", "      a();", "}"});

    document->ReindentLineRange(1, 1);
    TR_ASSERT(t, document->LineAt(1)->Buffer() == U"    a();");

    document->OnAction({gedit::kAction::kActionUndo});
    TR_ASSERT(t, document->LineAt(1)->Buffer() == U"      a();");
    IndentCache::Instance().Clear();
    return kTR_Pass;
}

// Block-surround (H.18b): wrapping a multi-line body with '{' puts the braces on their OWN lines and nests
// the body one level - the result the inline faithful wrap could not produce.
DLL_EXPORT int test_document_reformat_surround(ITesting *t) {
    auto document = CreateReformatDoc(t, {"void f() {", "a();", "b();"});

    document->SurroundLineRangeWithBlock(1, 2, U'{', U'}');

    TR_ASSERT(t, document->GetTextBuffer()->NumLines() == 5);
    TR_ASSERT(t, document->LineAt(0)->Buffer() == U"void f() {");
    TR_ASSERT(t, document->LineAt(1)->Buffer() == U"    {");
    TR_ASSERT(t, document->LineAt(2)->Buffer() == U"        a();");
    TR_ASSERT(t, document->LineAt(3)->Buffer() == U"        b();");
    TR_ASSERT(t, document->LineAt(4)->Buffer() == U"    }");
    IndentCache::Instance().Clear();
    return kTR_Pass;
}

// After a block-surround the caret must stay valid and ON-SCREEN: position.y is screen-relative
// (idxActiveLine - viewTopLine), so on a SCROLLED view a surround that parks it with an absolute y leaves
// the caret off-screen ("cursor gone"). Build a tall doc + short scrolled view so the two coords diverge.
DLL_EXPORT int test_document_reformat_surround_cursor(ITesting *t) {
    auto document = CreateIndentDoc(t);
    auto tb = document->GetTextBuffer();
    tb->DeleteLineAt(0);
    tb->AddLineUTF8("void f() {");
    for (int i = 0; i < 28; i++) {
        tb->AddLineUTF8("x();");
    }
    tb->AddLineUTF8("}");
    tb->Reparse();

    document->OnViewInit(gedit::Rect(40, 10));      // a 10-row view...
    auto &lc = document->GetLineCursor();
    lc.viewTopLine = 20;                            // ...scrolled down so viewTopLine != 0
    lc.viewBottomLine = 30;
    lc.idxActiveLine = 22;
    lc.cursor.position = { 0, 22 - 20 };

    document->SurroundLineRangeWithBlock(22, 23, U'{', U'}');

    auto &after = document->GetLineCursor();
    TR_ASSERT(t, after.idxActiveLine >= (size_t)after.viewTopLine);
    TR_ASSERT(t, after.idxActiveLine <= (size_t)after.viewBottomLine);
    // The invariant the renderer relies on: position.y is the SCREEN row, not the absolute line index.
    TR_ASSERT(t, after.cursor.position.y == (int)after.idxActiveLine - (int)after.viewTopLine);
    IndentCache::Instance().Clear();
    return kTR_Pass;
}

// The whole block-surround is ONE undo item (kReplaceRange): undo removes the 2 added brace lines and
// restores the original body verbatim, in a single step.
DLL_EXPORT int test_document_reformat_surround_undo(ITesting *t) {
    auto document = CreateReformatDoc(t, {"void f() {", "a();", "b();"});

    document->SurroundLineRangeWithBlock(1, 2, U'{', U'}');
    TR_ASSERT(t, document->GetTextBuffer()->NumLines() == 5);

    document->OnAction({gedit::kAction::kActionUndo});
    TR_ASSERT(t, document->GetTextBuffer()->NumLines() == 3);
    TR_ASSERT(t, document->LineAt(0)->Buffer() == U"void f() {");
    TR_ASSERT(t, document->LineAt(1)->Buffer() == U"a();");
    TR_ASSERT(t, document->LineAt(2)->Buffer() == U"b();");
    IndentCache::Instance().Clear();
    return kTR_Pass;
}

// ReformatBlock with no selection finds the enclosing '{ }' and reformats the whole block. Cursor sits on a
// body line of the outer block; the nested inner block is reindented relative to it.
DLL_EXPORT int test_document_reformat_block_enclosing(ITesting *t) {
    auto document = CreateReformatDoc(t, {"void f() {", "a();", "if (y) {", "b();", "}", "}"});
    auto &lc = document->GetLineCursor();
    lc.idxActiveLine = 1;               // a body line of the outer block, no selection
    lc.cursor.position.x = 0;

    document->OnAction({gedit::kAction::kActionReformatBlock});

    TR_ASSERT(t, document->LineAt(0)->Buffer() == U"void f() {");
    TR_ASSERT(t, document->LineAt(1)->Buffer() == U"    a();");
    TR_ASSERT(t, document->LineAt(2)->Buffer() == U"    if (y) {");
    TR_ASSERT(t, document->LineAt(3)->Buffer() == U"        b();");
    TR_ASSERT(t, document->LineAt(4)->Buffer() == U"    }");
    TR_ASSERT(t, document->LineAt(5)->Buffer() == U"}");
    IndentCache::Instance().Clear();
    return kTR_Pass;
}

// Enter at the end of a line that opens a block indents the new line one level (4 spaces).
DLL_EXPORT int test_document_indent_newline(ITesting *t) {
    auto document = CreateIndentDoc(t);
    auto &lc = document->GetLineCursor();

    document->ActiveLine()->Append(std::u32string(U"if (x) {"));
    lc.cursor.position.x = 8;                                  // end of line
    lc.idxActiveLine = document->NewLine(lc.idxActiveLine, lc.cursor);

    TR_ASSERT(t, document->ActiveLine()->Buffer() == U"    "); // new line indented one level
    TR_ASSERT(t, lc.cursor.position.x == 4);
    IndentCache::Instance().Clear();
    return kTR_Pass;
}

// Enter just before a closer (the closer moves down to the new line) dedents that line.
DLL_EXPORT int test_document_indent_dedent(ITesting *t) {
    auto document = CreateIndentDoc(t);
    auto &lc = document->GetLineCursor();

    document->ActiveLine()->Append(std::u32string(U"    }"));
    lc.cursor.position.x = 4;                                  // before the '}'
    lc.idxActiveLine = document->NewLine(lc.idxActiveLine, lc.cursor);

    TR_ASSERT(t, document->ActiveLine()->Buffer() == U"}");    // closer dropped to level 0
    TR_ASSERT(t, lc.cursor.position.x == 0);
    IndentCache::Instance().Clear();
    return kTR_Pass;
}

// Enter between an opener and its closer expands into three lines, cursor on the indented middle line.
DLL_EXPORT int test_document_indent_expand(ITesting *t) {
    auto document = CreateIndentDoc(t);
    auto &lc = document->GetLineCursor();

    document->ActiveLine()->Append(std::u32string(U"{}"));
    auto idxOpen = lc.idxActiveLine;
    lc.cursor.position.x = 1;                                  // between '{' and '}'
    lc.idxActiveLine = document->NewLine(lc.idxActiveLine, lc.cursor);

    TR_ASSERT(t, document->LineAt(idxOpen)->Buffer() == U"{");
    TR_ASSERT(t, lc.idxActiveLine == idxOpen + 1);             // cursor on the middle line
    TR_ASSERT(t, document->ActiveLine()->Buffer() == U"    "); // middle empty line, indented
    TR_ASSERT(t, lc.cursor.position.x == 4);
    TR_ASSERT(t, document->LineAt(idxOpen + 2)->Buffer() == U"}");
    IndentCache::Instance().Clear();
    return kTR_Pass;
}

// Typing a closer as the first non-blank char of a line snaps its indent back a level, then inserts it.
DLL_EXPORT int test_document_indent_electric(ITesting *t) {
    auto document = CreateIndentDoc(t);
    auto controller = EditController::Create(document);
    auto &lc = document->GetLineCursor();

    document->ActiveLine()->Append(std::u32string(U"        "));   // 8 spaces == level 2
    lc.cursor.position.x = 8;

    auto kp = KP(U'}');
    controller->HandleKeyPress(lc.cursor, lc.idxActiveLine, kp);

    TR_ASSERT(t, document->ActiveLine()->Buffer() == U"    }");    // dedented to level 1, then '}'
    TR_ASSERT(t, lc.cursor.position.x == 5);
    IndentCache::Instance().Clear();
    return kTR_Pass;
}

// A closer typed mid-line (not the first non-blank char) does NOT dedent - just inserts normally.
DLL_EXPORT int test_document_indent_electric_midline(ITesting *t) {
    auto document = CreateIndentDoc(t);
    auto controller = EditController::Create(document);
    auto &lc = document->GetLineCursor();

    document->ActiveLine()->Append(std::u32string(U"    x"));
    lc.cursor.position.x = 5;

    auto kp = KP(U'}');
    controller->HandleKeyPress(lc.cursor, lc.idxActiveLine, kp);

    TR_ASSERT(t, document->ActiveLine()->Buffer() == U"    x}");   // unchanged indent
    TR_ASSERT(t, lc.cursor.position.x == 6);
    IndentCache::Instance().Clear();
    return kTR_Pass;
}

// Regression (GUI feel-check 2026-06-11): typing a closer on an EMPTY line with the cursor parked
// past its end must not crash. Electric dedent early-returns (no leading run to shrink), so the closer
// falls through to the default insert - which used to call line->Insert(cursor.x) with cursor.x past
// the (zero-length) buffer => std::out_of_range => terminate. The insert chokepoint now clamps to the
// line end (append), so the closer lands and the cursor follows.
DLL_EXPORT int test_document_indent_electric_emptyline(ITesting *t) {
    auto document = CreateIndentDoc(t);
    auto controller = EditController::Create(document);
    auto &lc = document->GetLineCursor();

    // ActiveLine is empty; cursor sits at col 8 (reachable in the live app via auto-indented blank lines).
    lc.cursor.position.x = 8;

    auto kp = KP(U'}');
    controller->HandleKeyPress(lc.cursor, lc.idxActiveLine, kp);   // must not throw

    TR_ASSERT(t, document->ActiveLine()->Buffer() == U"}");        // closer appended, no crash
    TR_ASSERT(t, lc.cursor.position.x == 1);
    IndentCache::Instance().Clear();
    return kTR_Pass;
}

// ---------------------------------------------------------------------------------------------------
// Whole-line cut/paste preserves the trailing newline (open-bugs.md #4 / feel-check E.18).
// A selection that starts at column 0 and ends at the END of its last line is a WHOLE-LINE selection;
// the cut/copy normalizes its end to column 0 of the next line (Document::SelectionEndForCopy), so the
// cut removes whole lines and a paste-back restores them exactly - no extra blank line, no join.
// FillEmptyDocument makes lines "00000".."55555" (each 5 chars).
// ---------------------------------------------------------------------------------------------------

// NOTE: paste is driven through ClipBoard::PasteToBuffer directly (as the clipboard tests do) - the
// Document::PasteFromClipboard action path derefs the live screen (UpdateClipboardData), which the
// headless test harness has no instance of. The CUT - where the whole-line normalization lives - still
// runs through the real Document::OnAction path.
DLL_EXPORT int test_document_cut_paste_linewise(ITesting *t) {
    auto document = CreateEmptyDocument(t);
    gedit::Rect rect(20, 20);
    document->OnViewInit(rect);
    FillEmptyDocument(document, 6, 5);

    // Select whole lines 0..3, ending at the END of line 3 ("33333" -> col 5).
    document->OnAction(actionShiftLineDown);    // -> line 1
    document->OnAction(actionShiftLineDown);    // -> line 2
    document->OnAction(actionShiftLineDown);    // -> line 3
    document->OnAction(actionShiftLineEnd);     // extend to EOL of line 3

    auto &sel = document->GetSelection();
    TR_ASSERT(t, sel.GetStart().x == 0);
    TR_ASSERT(t, sel.GetStart().y == 0);
    TR_ASSERT(t, sel.GetEnd().x == 5);          // EOL of "33333", NOT col 0 of the next line
    TR_ASSERT(t, sel.GetEnd().y == 3);

    document->OnAction(actionCut);
    // Whole lines removed - line 4 pulled up, no blank remnant.
    TR_ASSERT(t, document->Lines().size() == 2);
    TR_ASSERT(t, document->LineAt(0)->Buffer() == U"44444");
    TR_ASSERT(t, document->LineAt(1)->Buffer() == U"55555");

    // Paste back at the caret (where the cut left it, col 0 of line 0).
    auto &clipboard = Editor::Instance().GetClipBoard();
    clipboard.PasteToBuffer(document->GetTextBuffer(), {0, 0});

    // Restored exactly - the outer line ("44444") did NOT join the last pasted line.
    TR_ASSERT(t, document->Lines().size() == 6);
    TR_ASSERT(t, document->LineAt(0)->Buffer() == U"00000");
    TR_ASSERT(t, document->LineAt(1)->Buffer() == U"11111");
    TR_ASSERT(t, document->LineAt(2)->Buffer() == U"22222");
    TR_ASSERT(t, document->LineAt(3)->Buffer() == U"33333");
    TR_ASSERT(t, document->LineAt(4)->Buffer() == U"44444");
    TR_ASSERT(t, document->LineAt(5)->Buffer() == U"55555");
    return kTR_Pass;
}

// One undo after a whole-line cut restores the original lines in a single step.
DLL_EXPORT int test_document_cut_paste_linewise_undo(ITesting *t) {
    auto document = CreateEmptyDocument(t);
    gedit::Rect rect(20, 20);
    document->OnViewInit(rect);
    FillEmptyDocument(document, 6, 5);

    document->OnAction(actionShiftLineDown);    // -> line 1
    document->OnAction(actionShiftLineDown);    // -> line 2
    document->OnAction(actionShiftLineDown);    // -> line 3
    document->OnAction(actionShiftLineEnd);     // extend to EOL of line 3

    document->OnAction(actionCut);
    TR_ASSERT(t, document->Lines().size() == 2);

    document->OnAction(actionUndo);
    TR_ASSERT(t, document->Lines().size() == 6);
    TR_ASSERT(t, document->LineAt(0)->Buffer() == U"00000");
    TR_ASSERT(t, document->LineAt(3)->Buffer() == U"33333");
    TR_ASSERT(t, document->LineAt(4)->Buffer() == U"44444");
    TR_ASSERT(t, document->LineAt(5)->Buffer() == U"55555");
    return kTR_Pass;
}

// EOF boundary: a whole-line-looking selection ending at the buffer's LAST line is NOT normalized
// (there is no trailing newline at EOF), so it stays charwise - and still round-trips exactly.
DLL_EXPORT int test_document_cut_paste_charwise(ITesting *t) {
    auto document = CreateEmptyDocument(t);
    gedit::Rect rect(20, 20);
    document->OnViewInit(rect);
    FillEmptyDocument(document, 6, 5);

    // Move (no selection) to line 3, then select whole lines 3..5 ending at EOL of the last line.
    document->OnAction(actionLineDown);
    document->OnAction(actionLineDown);
    document->OnAction(actionLineDown);
    document->OnAction(actionShiftLineDown);    // -> line 4
    document->OnAction(actionShiftLineDown);    // -> line 5
    document->OnAction(actionShiftLineEnd);     // extend to EOL of line 5 (last line)

    auto &sel = document->GetSelection();
    TR_ASSERT(t, sel.GetStart().y == 3);
    TR_ASSERT(t, sel.GetEnd().x == 5);
    TR_ASSERT(t, sel.GetEnd().y == 5);

    document->OnAction(actionCut);
    auto &clipboard = Editor::Instance().GetClipBoard();
    clipboard.PasteToBuffer(document->GetTextBuffer(), {0, 3});  // caret left at col 0 of line 3
    // Round-trips exactly via the charwise path (no normalization at EOF, no double blank line).
    TR_ASSERT(t, document->Lines().size() == 6);
    TR_ASSERT(t, document->LineAt(2)->Buffer() == U"22222");
    TR_ASSERT(t, document->LineAt(3)->Buffer() == U"33333");
    TR_ASSERT(t, document->LineAt(4)->Buffer() == U"44444");
    TR_ASSERT(t, document->LineAt(5)->Buffer() == U"55555");
    return kTR_Pass;
}

// The exact feel-check E.18 shape: a function body whose two statements are whole-line selected, cut,
// and pasted back. The selection is made the natural GUI way - cursor on the first body line, two
// Shift+Down - so it ends at COLUMN 0 of the closing-brace line (end.x == 0). Regression guard: the
// closing '}' must stay on its own line, not get joined as 'bar();}'.
DLL_EXPORT int test_document_cut_paste_funcbody(ITesting *t) {
    auto document = CreateEmptyDocument(t);
    gedit::Rect rect(40, 40);
    document->OnViewInit(rect);

    auto buffer = document->GetTextBuffer();
    buffer->DeleteLineAt(0);                        // drop the seeded empty line
    buffer->AddLineUTF8("static void func() {");    // 0
    buffer->AddLineUTF8("    foo();");              // 1
    buffer->AddLineUTF8("    bar();");              // 2
    buffer->AddLineUTF8("}");                       // 3

    // Cursor onto the foo line, then Shift+Down twice to whole-line select foo + bar. The caret lands
    // at column 0 of the '}' line, so the selection ends at (0, 3).
    document->OnAction(actionLineDown);             // -> line 1 (foo)
    document->OnAction(actionShiftLineDown);        // select foo  -> caret line 2
    document->OnAction(actionShiftLineDown);        // select foo+bar -> caret line 3 (col 0)

    auto &sel = document->GetSelection();
    TR_ASSERT(t, sel.GetStart().x == 0);
    TR_ASSERT(t, sel.GetStart().y == 1);
    TR_ASSERT(t, sel.GetEnd().x == 0);              // ends at col 0 of the '}' line
    TR_ASSERT(t, sel.GetEnd().y == 3);

    document->OnAction(actionCut);
    // foo + bar removed, '}' pulled up directly under the signature.
    TR_ASSERT(t, document->Lines().size() == 2);
    TR_ASSERT(t, document->LineAt(0)->Buffer() == U"static void func() {");
    TR_ASSERT(t, document->LineAt(1)->Buffer() == U"}");

    // Paste back where the cut left the caret (col 0 of line 1).
    auto &clipboard = Editor::Instance().GetClipBoard();
    clipboard.PasteToBuffer(document->GetTextBuffer(), {0, 1});

    TR_ASSERT(t, document->Lines().size() == 4);
    TR_ASSERT(t, document->LineAt(0)->Buffer() == U"static void func() {");
    TR_ASSERT(t, document->LineAt(1)->Buffer() == U"    foo();");
    TR_ASSERT(t, document->LineAt(2)->Buffer() == U"    bar();");
    TR_ASSERT(t, document->LineAt(3)->Buffer() == U"}");      // NOT joined as "    bar();}"
    return kTR_Pass;
}
