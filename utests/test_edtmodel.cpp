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
static KeyPressAction actionLineUp = {gedit::kAction::kActionLineUp};
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
    return kTR_Pass;
}

DLL_EXPORT int test_edtmodel_text_selfunc(ITesting *t) {
    auto model = CreateEmptyModel(t);
    return kTR_Pass;
}
