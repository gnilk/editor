//
// Find (docs/modal_fixes.md step 4): case-insensitive search, the FindModal over a document (narrowing,
// preview, commit, cancel, no hits) and the generic QueryListModal list (scrolling, editing).
//
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/Document.h"
#include "Core/TextBuffer.h"
#include "Core/Keyboard.h"
#include "Core/UI/UIHost.h"
#include "Core/UI/Views/RootView.h"
#include "Core/Editor/Modals/FindModal.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_find(ITesting *t);
    DLL_EXPORT int test_find_caseinsensitive(ITesting *t);
    DLL_EXPORT int test_find_narrows(ITesting *t);
    DLL_EXPORT int test_find_preview(ITesting *t);
    DLL_EXPORT int test_find_commit(ITesting *t);
    DLL_EXPORT int test_find_cancel(ITesting *t);
    DLL_EXPORT int test_find_nohits(ITesting *t);
    DLL_EXPORT int test_find_scroll(ITesting *t);
    DLL_EXPORT int test_find_selection(ITesting *t);
    DLL_EXPORT int test_find_paste(ITesting *t);
}

namespace {
    // A document 'nLines' long - every 10th line has 'Needle', the others 'hay' - in a 30 row view
    Document::Ref MakeDocument(int nLines) {
        auto textBuffer = TextBuffer::CreateEmptyBuffer();
        auto document = Document::Create(textBuffer);
        // The empty buffer starts with one empty line
        document->GetTextBuffer()->DeleteLineAt(0);
        for (int i = 0; i < nLines; i++) {
            auto text = ((i % 10) == 5) ? ("\tint Needle" + std::to_string(i) + ";") : ("hay " + std::to_string(i));
            document->GetTextBuffer()->AddLineUTF8(text.c_str());
        }
        document->OnViewInit(gedit::Rect(80, 30));
        return document;
    }

    // A modal host (100x100 headless screen)
    struct Fixture {
        Fixture() {
            rootView.Initialize();
        }
        // Shows Find over 'document' with 'query' typed; returns the modal (owned by the root)
        FindModal *ShowFind(Document::Ref document, const std::u32string &query) {
            auto modal = FindModal::Create(document);
            auto findModal = modal.get();
            rootView.ShowModal(std::move(modal));
            findModal->SetQuery(query);
            return findModal;
        }
        RootView rootView;
    };

    EditorAction UIAction(kUIAction uiAction) {
        EditorAction action;
        action.uiAction = uiAction;
        return action;
    }

    KeyPress Key(char32_t key) {
        KeyPress keyPress;
        keyPress.isKeyValid = true;
        keyPress.key = key;
        return keyPress;
    }

    KeyPress Backspace() {
        KeyPress keyPress;
        keyPress.isKeyValid = true;
        keyPress.isSpecialKey = true;
        keyPress.specialKey = Keyboard::kKeyCode_Backspace;
        return keyPress;
    }
}

DLL_EXPORT int test_find(ITesting *t) {
    // Headless screen (test_main) - views need it to initialize
    if (UIHost::Instance().GetScreen() == nullptr) {
        Editor::Instance().OpenScreen();
    }
    return kTR_Pass;
}

// Case-insensitive only when asked - the quick-command search ('/') stays case-sensitive
DLL_EXPORT int test_find_caseinsensitive(ITesting *t) {
    auto document = MakeDocument(30);
    TR_ASSERT(t, document->SearchFor(U"needle") == 0);
    TR_ASSERT(t, document->SearchFor(U"Needle") == 3);
    TR_ASSERT(t, document->SearchFor(U"nEEDLE", false) == 3);
    TR_ASSERT(t, document->GetSearchResults()[0].idxLine == 5);
    // The column is where the hit starts (after the tab)
    TR_ASSERT(t, document->GetSearchResults()[0].cursor_x == 5);
    return kTR_Pass;
}

// Typing narrows the list; backspace widens it again; rows are 'line: text' without the indent
DLL_EXPORT int test_find_narrows(ITesting *t) {
    Fixture fixture;
    auto document = MakeDocument(100);
    auto findModal = fixture.ShowFind(document, U"");
    TR_ASSERT(t, findModal->GetNumRows() == 0);

    fixture.rootView.HandleKeyPress(Key('n'));
    fixture.rootView.HandleKeyPress(Key('e'));
    TR_ASSERT(t, findModal->GetQuery() == U"ne");
    TR_ASSERT(t, findModal->GetNumRows() == 10);

    // 'needle1' - lines 15, 105.. -> only 15 in 100 lines
    for (auto ch : std::u32string(U"edle1")) {
        fixture.rootView.HandleKeyPress(Key(ch));
    }
    TR_ASSERT(t, findModal->GetNumRows() == 1);
    TR_ASSERT(t, document->GetSearchResults()[0].idxLine == 15);

    fixture.rootView.HandleKeyPress(Backspace());
    TR_ASSERT(t, findModal->GetQuery() == U"needle");
    TR_ASSERT(t, findModal->GetNumRows() == 10);
    fixture.rootView.OnAction(UIAction(kUIAction::kUIActionCloseModal));
    return kTR_Pass;
}

// Moving in the list moves the editor's cursor to the hit, in the lower part of the view (the dialog
// covers the top)
DLL_EXPORT int test_find_preview(ITesting *t) {
    Fixture fixture;
    auto document = MakeDocument(200);
    auto findModal = fixture.ShowFind(document, U"needle");
    TR_ASSERT(t, findModal->GetSelectedRow() == 0);
    TR_ASSERT(t, document->GetLineCursor().idxActiveLine == 5);

    // Every step - not only where the plain refocus happens to land
    auto &lineCursor = document->GetLineCursor();
    for (int i = 0; i < 8; i++) {
        fixture.rootView.OnAction(UIAction(kUIAction::kUIActionLineDown));
        auto rowNow = (int)lineCursor.idxActiveLine - lineCursor.viewTopLine;
        TR_ASSERT(t, (rowNow >= lineCursor.Height() / 2) && (rowNow < lineCursor.Height()));
    }
    TR_ASSERT(t, findModal->GetSelectedRow() == 8);
    TR_ASSERT(t, lineCursor.idxActiveLine == 85);
    TR_ASSERT(t, document->GetCursor().position.x == 5);
    auto row = (int)lineCursor.idxActiveLine - lineCursor.viewTopLine;
    TR_ASSERT(t, row >= lineCursor.Height() / 2);
    TR_ASSERT(t, row < lineCursor.Height());
    TR_ASSERT(t, document->GetCursor().position.y == row);

    fixture.rootView.OnAction(UIAction(kUIAction::kUIActionLineUp));
    TR_ASSERT(t, lineCursor.idxActiveLine == 75);
    fixture.rootView.OnAction(UIAction(kUIAction::kUIActionCloseModal));
    return kTR_Pass;
}

// Return: closed, the cursor stays on the hit, the results stay and Next continues from it
DLL_EXPORT int test_find_commit(ITesting *t) {
    Fixture fixture;
    auto document = MakeDocument(100);
    fixture.ShowFind(document, U"needle");
    fixture.rootView.OnAction(UIAction(kUIAction::kUIActionLineDown));
    fixture.rootView.OnAction(UIAction(kUIAction::kUIActionLineDown));
    fixture.rootView.OnAction(UIAction(kUIAction::kUIActionCommitLine));

    TR_ASSERT(t, !fixture.rootView.HasModal());
    TR_ASSERT(t, document->GetLineCursor().idxActiveLine == 25);
    TR_ASSERT(t, document->HaveSearchResults());
    TR_ASSERT(t, document->GetSearchHitIndex() == 2);

    document->NextSearchResult();
    TR_ASSERT(t, document->GetLineCursor().idxActiveLine == 35);
    return kTR_Pass;
}

// Esc: the cursor and the view go back exactly, the results are gone
DLL_EXPORT int test_find_cancel(ITesting *t) {
    Fixture fixture;
    auto document = MakeDocument(200);
    document->SetCursorPosition(12, 2);
    auto &lineCursor = document->GetLineCursor();
    auto viewTop = lineCursor.viewTopLine;
    auto cursorY = document->GetCursor().position.y;

    fixture.ShowFind(document, U"needle");
    for (int i = 0; i < 12; i++) {
        fixture.rootView.OnAction(UIAction(kUIAction::kUIActionLineDown));
    }
    TR_ASSERT(t, lineCursor.idxActiveLine == 125);
    TR_ASSERT(t, lineCursor.viewTopLine != viewTop);

    fixture.rootView.OnAction(UIAction(kUIAction::kUIActionCloseModal));
    TR_ASSERT(t, !fixture.rootView.HasModal());
    TR_ASSERT(t, lineCursor.idxActiveLine == 12);
    TR_ASSERT(t, document->GetCursor().position.x == 2);
    TR_ASSERT(t, lineCursor.viewTopLine == viewTop);
    TR_ASSERT(t, lineCursor.viewBottomLine == viewTop + 30);
    TR_ASSERT(t, document->GetCursor().position.y == cursorY);
    TR_ASSERT(t, !document->HaveSearchResults());
    return kTR_Pass;
}

// No hits: the cursor goes back to where it was, Return does nothing (the dialog stays)
DLL_EXPORT int test_find_nohits(ITesting *t) {
    Fixture fixture;
    auto document = MakeDocument(100);
    document->SetCursorPosition(3, 1);
    auto findModal = fixture.ShowFind(document, U"needle");
    TR_ASSERT(t, document->GetLineCursor().idxActiveLine == 5);

    fixture.rootView.HandleKeyPress(Key('x'));
    TR_ASSERT(t, findModal->GetNumRows() == 0);
    TR_ASSERT(t, findModal->GetSelectedRow() == -1);
    TR_ASSERT(t, document->GetLineCursor().idxActiveLine == 3);

    fixture.rootView.OnAction(UIAction(kUIAction::kUIActionCommitLine));
    TR_ASSERT(t, fixture.rootView.HasModal());
    fixture.rootView.OnAction(UIAction(kUIAction::kUIActionLineDown));
    TR_ASSERT(t, findModal->GetSelectedRow() == -1);
    fixture.rootView.OnAction(UIAction(kUIAction::kUIActionCloseModal));
    return kTR_Pass;
}

// More hits than rows: Page/Line moves clamp at the ends and the selection stays inside the list
DLL_EXPORT int test_find_scroll(ITesting *t) {
    Fixture fixture;
    auto document = MakeDocument(2000);
    auto findModal = fixture.ShowFind(document, U"needle");
    TR_ASSERT(t, findModal->GetNumRows() == 200);

    fixture.rootView.OnAction(UIAction(kUIAction::kUIActionLineUp));
    TR_ASSERT(t, findModal->GetSelectedRow() == 0);
    fixture.rootView.OnAction(UIAction(kUIAction::kUIActionPageDown));
    auto page = findModal->GetSelectedRow();
    TR_ASSERT(t, page > 1);
    for (int i = 0; i < 100; i++) {
        fixture.rootView.OnAction(UIAction(kUIAction::kUIActionPageDown));
    }
    TR_ASSERT(t, findModal->GetSelectedRow() == 199);
    TR_ASSERT(t, document->GetLineCursor().idxActiveLine == 1995);
    fixture.rootView.OnAction(UIAction(kUIAction::kUIActionPageUp));
    TR_ASSERT(t, findModal->GetSelectedRow() == 199 - page);
    fixture.rootView.OnAction(UIAction(kUIAction::kUIActionCloseModal));
    return kTR_Pass;
}

// Find starts with a selection on one line; a selection over several lines gives nothing
DLL_EXPORT int test_find_selection(ITesting *t) {
    auto document = MakeDocument(30);
    TR_ASSERT(t, FindModal::SelectedText(document).empty());

    document->SetCursorPosition(5, 5);
    document->BeginSelection();
    auto &selection = const_cast<Selection &>(document->GetSelection());
    selection.SetStart({5, 5});
    selection.SetEnd({11, 5});
    TR_ASSERT(t, FindModal::SelectedText(document) == U"Needle");

    selection.SetEnd({2, 6});
    TR_ASSERT(t, FindModal::SelectedText(document).empty());
    document->CancelSelection();
    return kTR_Pass;
}

// Paste inserts at the field's cursor (only the first line) and narrows like typing - the clipboard is
// filled from 'outside', the round-trip the GUI takes (E.18)
DLL_EXPORT int test_find_paste(ITesting *t) {
    auto document = MakeDocument(100);
    Fixture fixture;
    auto findModal = fixture.ShowFind(document, U"5;");
    TR_ASSERT(t, findModal->GetNumRows() == 10);    // every 'Needle<n>5;' line

    Editor::Instance().GetClipBoard().CopyFromExternal("NEEDLE2\nsecond line");
    fixture.rootView.OnAction(UIAction(kUIAction::kUIActionLineHome));
    EditorAction paste;
    paste.action = kAction::kActionPasteFromClipboard;
    TR_ASSERT(t, fixture.rootView.OnAction(paste));

    TR_ASSERT(t, findModal->GetQuery() == U"NEEDLE25;");
    TR_ASSERT(t, findModal->GetNumRows() == 1);
    TR_ASSERT(t, document->GetLineCursor().idxActiveLine == 25);

    // The cursor is after the pasted text - typing continues there
    fixture.rootView.HandleKeyPress(Key(' '));
    TR_ASSERT(t, findModal->GetQuery() == U"NEEDLE2 5;");
    TR_ASSERT(t, findModal->GetNumRows() == 0);
    fixture.rootView.OnAction(UIAction(kUIAction::kUIActionCloseModal));
    return kTR_Pass;
}
