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
DLL_EXPORT int test_edtmodel_linefunc(ITesting *t);
DLL_EXPORT int test_edtmodel_selfunc(ITesting *t);
}

DLL_EXPORT int test_edtmodel(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);
    return kTR_Pass;

}

DLL_EXPORT int test_edtmodel_create(ITesting *t) {
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

DLL_EXPORT int test_edtmodel_linefunc(ITesting *t) {
    auto textBuffer = TextBuffer::CreateEmptyBuffer();
    TR_ASSERT(t, textBuffer != nullptr);
    auto model = EditorModel::Create(textBuffer);
    TR_ASSERT(t, model != nullptr);
    // The first line should always be available
    TR_ASSERT(t, model->Lines().size() == 1);
    TR_ASSERT(t, model->LineAt(2) == nullptr);
    TR_ASSERT(t, model->ActiveLine() != nullptr);
    TR_ASSERT(t, model->GetLineCursorRef()->idxActiveLine == 0);
    TR_ASSERT(t, model->GetLineCursorRef()->cursor.position.x == 0);

    return kTR_Pass;

}

DLL_EXPORT int test_edtmodel_selfunc(ITesting *t) {
    auto textBuffer = TextBuffer::CreateEmptyBuffer();
    TR_ASSERT(t, textBuffer != nullptr);
    auto model = EditorModel::Create(textBuffer);
    TR_ASSERT(t, model != nullptr);
    // The first line should always be available
    TR_ASSERT(t, model->IsSelectionActive() == false);

    // Create and cancel a selection
    model->BeginSelection();
    TR_ASSERT(t, model->IsSelectionActive() == true);
    model->CancelSelection();
    TR_ASSERT(t, model->IsSelectionActive() == false);

    KeyPressAction actionLineDown = {gedit::kAction::kActionLineDown};
    KeyPressAction actionShiftLineDown =
            {
            .action = gedit::kAction::kActionLineDown,
            .actionModifier = kActionModifier::kActionModifierSelection,
            .modifierMask = Keyboard::ShiftMask()
            };

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
