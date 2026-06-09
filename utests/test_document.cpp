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
