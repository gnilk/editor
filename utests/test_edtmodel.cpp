//
// Created by gnilk on 07.11.23.
//
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/TextBuffer.h"


using namespace gedit;

extern "C" {
DLL_EXPORT int test_edtmodel(ITesting *t);
DLL_EXPORT int test_edtmodel_create(ITesting *t);
// 'empty' - are all on an empty textbuffer
DLL_EXPORT int test_edtmodel_empty_linefunc(ITesting *t);
DLL_EXPORT int test_edtmodel_empty_selfunc(ITesting *t);
// 'text' - is with a bunch of text
DLL_EXPORT int test_edtmodel_text_linefunc(ITesting *t);
DLL_EXPORT int test_edtmodel_text_selfunc(ITesting *t);
}

// Define some common actions
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


DLL_EXPORT int test_edtmodel(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);
    // Ensure we test with a known vertical navigation model..
    Config::Instance()["editorview"].SetBool("pgupdown_content_first", true);
    return kTR_Pass;
}

static EditorModel::Ref CreateEmptyModel(ITesting *t) {
    auto textBuffer = TextBuffer::CreateEmptyBuffer();
    TR_ASSERT(t, textBuffer != nullptr);
    auto model = EditorModel::Create(textBuffer);
    TR_ASSERT(t, model != nullptr);
    return model;
}
static void FillEmptyModel(EditorModel::Ref model, size_t nLines, size_t lineLength) {
    // Remove first line - we don't want this to interfere
    model->GetTextBuffer()->DeleteLineAt(0);

    for(size_t i = 0; i<nLines;++i) {
        std::string str(lineLength, std::to_string(i).at(0));
        model->GetTextBuffer()->AddLineUTF8(str.c_str());
    }
}

DLL_EXPORT int test_edtmodel_create(ITesting *t) {
    // Don't use 'CreateEmptyModel' - this one does a bit more agressive testing of model and the textbuffer
    auto textBuffer = TextBuffer::CreateEmptyBuffer();
    TR_ASSERT(t, textBuffer != nullptr);
    auto model = EditorModel::Create(textBuffer);
    TR_ASSERT(t, model != nullptr);
    // The first line should always be available
    TR_ASSERT(t, model->Lines().size() == 1);
    TR_ASSERT(t, model->GetTextBuffer() == textBuffer);
    TR_ASSERT(t, model->IsActive() == false);   // this model has not been activated by the editor

    return kTR_Pass;
}

DLL_EXPORT int test_edtmodel_empty_linefunc(ITesting *t) {
    auto model = CreateEmptyModel(t);
    // The first line should always be available
    TR_ASSERT(t, model->Lines().size() == 1);
    TR_ASSERT(t, model->LineAt(2) == nullptr);
    TR_ASSERT(t, model->ActiveLine() != nullptr);
    TR_ASSERT(t, model->GetLineCursorRef()->idxActiveLine == 0);
    TR_ASSERT(t, model->GetLineCursorRef()->cursor.position.x == 0);

    return kTR_Pass;

}

DLL_EXPORT int test_edtmodel_empty_selfunc(ITesting *t) {
    auto model = CreateEmptyModel(t);
    // The first line should always be available
    TR_ASSERT(t, model->IsSelectionActive() == false);

    // Create and cancel a selection
    model->BeginSelection();
    TR_ASSERT(t, model->IsSelectionActive() == true);
    model->CancelSelection();
    TR_ASSERT(t, model->IsSelectionActive() == false);


    model->BeginSelection();
    TR_ASSERT(t, model->IsSelectionActive() == true);
    model->OnAction(actionLineDown);
    // this should cancel the selection as the shift modifier isn't pressed...
    TR_ASSERT(t, model->IsSelectionActive() == false);

    // This should trigger a full line marking of the single line of text we have (empty)
    model->BeginSelection();
    TR_ASSERT(t, model->IsSelectionActive() == true);
    model->OnAction(actionShiftLineDown);
    // this should cancel the selection as the shift modifier isn't pressed...
    TR_ASSERT(t, model->IsSelectionActive() == true);
    model->CancelSelection();
    TR_ASSERT(t, model->IsSelectionActive() == false);

    return kTR_Pass;
}

DLL_EXPORT int test_edtmodel_text_linefunc(ITesting *t) {
    auto model = CreateEmptyModel(t);
    // The 'view' rect (this is the size of the visible area of the text buffer)
    // it is used to calculate the actual viewing area for the renderer
    // needed for navigation testing since cursor updates will move it around..
    // this also defines the height of a 'page' when dealing with page-down/up
    gedit::Rect rect(20,20);
    model->OnViewInit(rect);

    // Insert 40 lines with 40 chars
    FillEmptyModel(model, 40, 40);
    // The first line should always be available
    TR_ASSERT(t, model->Lines().size() == 40);  // Initial line is still there..
    TR_ASSERT(t, model->ActiveLine() != nullptr);
    TR_ASSERT(t, model->ActiveLine()->Length() == 40);
    TR_ASSERT(t, model->GetLineCursorRef()->idxActiveLine == 0);
    TR_ASSERT(t, model->GetLineCursorRef()->cursor.position.x == 0);

    model->OnAction(actionLineDown);
    TR_ASSERT(t, model->ActiveLine() != nullptr);
    TR_ASSERT(t, model->GetLineCursorRef()->idxActiveLine == 1);
    TR_ASSERT(t, model->GetLineCursorRef()->cursor.position.y == 1);

    model->OnAction(actionPageDown);
    TR_ASSERT(t, model->ActiveLine() != nullptr);
    // We move 'height-1' - keeping at least one line of the previous visual chunk present on the screen
    TR_ASSERT(t, model->GetLineCursorRef()->idxActiveLine == 20);
    // THIS depends on the current view model - we lock this to 'content-first' (CLion/Sublime) for this test
    TR_ASSERT(t, model->GetLineCursorRef()->cursor.position.y == 1);

    // This will move us back to where we were at (one line down)
    model->OnAction(actionPageUp);
    TR_ASSERT(t, model->ActiveLine() != nullptr);
    TR_ASSERT(t, model->GetLineCursorRef()->idxActiveLine == 1);
    TR_ASSERT(t, model->GetLineCursorRef()->cursor.position.y == 1);

    // We are one line down - moving a whole page up should put us on top - clipping to boundary
    model->OnAction(actionPageUp);
    TR_ASSERT(t, model->ActiveLine() != nullptr);
    TR_ASSERT(t, model->GetLineCursorRef()->idxActiveLine == 0);
    TR_ASSERT(t, model->GetLineCursorRef()->cursor.position.y == 0);

    return kTR_Pass;
}

DLL_EXPORT int test_edtmodel_text_selfunc(ITesting *t) {
    auto model = CreateEmptyModel(t);

    gedit::Rect rect(20,20);
    model->OnViewInit(rect);

    // Insert 40 lines with 40 chars
    FillEmptyModel(model, 40, 40);


    // This will start the selection
    model->OnAction(actionShiftLineDown);   // select one line
    TR_ASSERT(t, model->ActiveLine() != nullptr);
    TR_ASSERT(t, model->GetLineCursorRef()->idxActiveLine == 1);
    TR_ASSERT(t, model->GetLineCursorRef()->cursor.position.y == 1);

    // Selection should now be active
    TR_ASSERT(t, model->IsSelectionActive());
    auto &selection = model->GetSelection();
    TR_ASSERT(t, selection.IsActive());
    TR_ASSERT(t, selection.GetStart().y == 0);
    TR_ASSERT(t, selection.GetEnd().y == 1);

    // Continue selection
    model->OnAction(actionShiftLineDown);   // select one line
    TR_ASSERT(t, selection.IsActive());
    TR_ASSERT(t, selection.GetStart().y == 0);
    TR_ASSERT(t, selection.GetEnd().y == 2);

    // Test if we can copy it
    auto &clipboard = Editor::Instance().GetClipBoard();
    clipboard.CopyFromBuffer(model->GetTextBuffer(), selection.GetStart(), selection.GetEnd());
    auto item = clipboard.Top();
    TR_ASSERT(t, item->GetLineCount() == 2);


    return kTR_Pass;
}
