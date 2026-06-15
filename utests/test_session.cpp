//
// Created by gnilk on 15.06.26.
//
// Phase-1 session-cache scaffolding tests. Real round-trip serialisation tests (YAML, atomic write,
// restore-with-missing-file) land as the implementation does — these guard the DTO + singleton seams.
//
#include <testinterface.h>
#include <memory>
#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"
#include "Core/Document.h"
#include "Core/DocumentViewState.h"
#include "Core/Workspace.h"
#include "Core/Views/VSplitView.h"
#include "Core/Views/HSplitView.h"
#include "Core/Session/SessionState.h"
#include "Core/Session/SessionManager.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_session(ITesting *t);
DLL_EXPORT int test_session_dto_defaults(ITesting *t);
DLL_EXPORT int test_session_singleton_clear(ITesting *t);
DLL_EXPORT int test_session_documentviewstate_roundtrip(ITesting *t);
DLL_EXPORT int test_session_selection_direction_preserved(ITesting *t);
DLL_EXPORT int test_session_document_roundtrip(ITesting *t);
DLL_EXPORT int test_session_workspace_enumerate(ITesting *t);
DLL_EXPORT int test_session_workspace_reopen_skips_missing(ITesting *t);
DLL_EXPORT int test_session_splitter_roundtrip(ITesting *t);
DLL_EXPORT int test_session_splitter_untagged_noop(ITesting *t);
}

DLL_EXPORT int test_session(ITesting *t) {
    // The splitter round-trip tests build real VSplitView/HSplitView, whose SetSplitterPos initialises
    // the view (which needs an open screen). Open one here so this module is hermetic when run in
    // isolation, not only as part of the verified-green set (mirrors test_layout).
    Editor::Instance().OpenScreen();
    RuntimeConfig::Instance().SetMainThreadID();
    return kTR_Pass;
}

// The DTO surface has the defaults the restore logic leans on (version stamped, empty doc list,
// no active doc, text view-mode).
DLL_EXPORT int test_session_dto_defaults(ITesting *t) {
    RootSession rs;
    TR_ASSERT(t, rs.version == RootSession::kVersion);
    TR_ASSERT(t, rs.documents.empty());
    TR_ASSERT(t, rs.activeDocumentIndex == -1);
    TR_ASSERT(t, rs.layout.splitters.empty());
    TR_ASSERT(t, !rs.layout.window.IsValid());     // zero geometry == "use cold-start default"

    DocumentSession ds;
    TR_ASSERT(t, ds.viewMode == DocumentViewMode::kText);
    TR_ASSERT(t, !ds.selection.isActive);
    ds.path = "src/main.cpp";
    ds.viewMode = DocumentViewMode::kHex;
    rs.documents.push_back(ds);
    rs.activeDocumentIndex = 0;
    TR_ASSERT(t, rs.documents.size() == 1);
    TR_ASSERT(t, rs.documents[0].viewMode == DocumentViewMode::kHex);
    return kTR_Pass;
}

// Singleton identity + Clear() resets in-memory state (test-isolation contract).
DLL_EXPORT int test_session_singleton_clear(ITesting *t) {
    auto &mgr = SessionManager::Instance();
    mgr.Clear();
    TR_ASSERT(t, mgr.CurrentSession().documents.empty());
    TR_ASSERT(t, mgr.CurrentSession().activeDocumentIndex == -1);

    mgr.CurrentSession().activeDocumentIndex = 5;
    mgr.CurrentSession().documents.push_back(DocumentSession{});
    // Same instance observes the mutation.
    TR_ASSERT(t, SessionManager::Instance().CurrentSession().activeDocumentIndex == 5);
    TR_ASSERT(t, SessionManager::Instance().CurrentSession().documents.size() == 1);

    mgr.Clear();
    TR_ASSERT(t, mgr.CurrentSession().activeDocumentIndex == -1);
    TR_ASSERT(t, mgr.CurrentSession().documents.empty());
    return kTR_Pass;
}

// The property restore leans on: DocumentViewState -> DocumentSession -> DocumentViewState is lossless
// for cursor, scroll window, selection and view-mode.
DLL_EXPORT int test_session_documentviewstate_roundtrip(ITesting *t) {
    DocumentViewState vs;
    vs.lineCursor.cursor.position = gedit::Point(7, 42);
    vs.lineCursor.cursor.wantedColumn = 9;
    vs.lineCursor.idxActiveLine = 42;
    vs.lineCursor.viewTopLine = 30;
    vs.lineCursor.viewBottomLine = 60;
    vs.currentSelection.SetActive(true);
    vs.currentSelection.SetStart(gedit::Point(3, 40));
    vs.currentSelection.SetEnd(gedit::Point(11, 44));
    vs.viewMode = DocumentViewMode::kHex;

    DocumentViewState restored;
    restored.FromSession(vs.ToSession());

    TR_ASSERT(t, restored.lineCursor.cursor.position.x == 7);
    TR_ASSERT(t, restored.lineCursor.cursor.position.y == 42);
    TR_ASSERT(t, restored.lineCursor.cursor.wantedColumn == 9);
    TR_ASSERT(t, restored.lineCursor.idxActiveLine == 42);
    TR_ASSERT(t, restored.lineCursor.viewTopLine == 30);
    TR_ASSERT(t, restored.lineCursor.viewBottomLine == 60);
    TR_ASSERT(t, restored.viewMode == DocumentViewMode::kHex);
    TR_ASSERT(t, restored.currentSelection.IsActive());
    TR_ASSERT(t, restored.currentSelection.GetRawStart().x == 3);
    TR_ASSERT(t, restored.currentSelection.GetRawStart().y == 40);
    TR_ASSERT(t, restored.currentSelection.GetRawEnd().x == 11);
    TR_ASSERT(t, restored.currentSelection.GetRawEnd().y == 44);
    return kTR_Pass;
}

// A backward selection (start AFTER end) must survive un-normalised — guards the GetRawStart/GetRawEnd
// path against the sorted GetStart/GetEnd that would silently flip the anchor.
DLL_EXPORT int test_session_selection_direction_preserved(ITesting *t) {
    DocumentViewState vs;
    vs.currentSelection.SetActive(true);
    vs.currentSelection.SetStart(gedit::Point(11, 44));   // start is after end
    vs.currentSelection.SetEnd(gedit::Point(3, 40));

    DocumentViewState restored;
    restored.FromSession(vs.ToSession());

    TR_ASSERT(t, restored.currentSelection.GetRawStart().x == 11);
    TR_ASSERT(t, restored.currentSelection.GetRawStart().y == 44);
    TR_ASSERT(t, restored.currentSelection.GetRawEnd().x == 3);
    TR_ASSERT(t, restored.currentSelection.GetRawEnd().y == 40);
    return kTR_Pass;
}

// Document aggregates file identity (path) + its DocumentViewState into one DocumentSession, and
// FromSession restores the view-state. (No TextBuffer needed — these touch only path + view state.)
DLL_EXPORT int test_session_document_roundtrip(ITesting *t) {
    Document doc;
    doc.SetPath("src/Core/Editor.cpp");
    doc.GetLineCursor().cursor.position = gedit::Point(4, 17);
    doc.GetLineCursor().cursor.wantedColumn = 4;
    doc.GetLineCursor().idxActiveLine = 17;
    doc.GetLineCursor().viewTopLine = 10;
    doc.GetLineCursor().viewBottomLine = 40;
    doc.SetViewMode(DocumentViewMode::kHex);

    auto dto = doc.ToSession();
    TR_ASSERT(t, dto.path == "src/Core/Editor.cpp");
    TR_ASSERT(t, dto.cursorX == 4);
    TR_ASSERT(t, dto.cursorY == 17);
    TR_ASSERT(t, dto.idxActiveLine == 17);
    TR_ASSERT(t, dto.viewMode == DocumentViewMode::kHex);

    Document restored;
    restored.FromSession(dto);
    TR_ASSERT(t, restored.GetLineCursor().cursor.position.x == 4);
    TR_ASSERT(t, restored.GetLineCursor().cursor.position.y == 17);
    TR_ASSERT(t, restored.GetLineCursor().cursor.wantedColumn == 4);
    TR_ASSERT(t, restored.GetLineCursor().idxActiveLine == 17);
    TR_ASSERT(t, restored.GetLineCursor().viewTopLine == 10);
    TR_ASSERT(t, restored.GetLineCursor().viewBottomLine == 40);
    TR_ASSERT(t, restored.GetViewMode() == DocumentViewMode::kHex);
    return kTR_Pass;
}

// Workspace enumerates its open documents + the active index into a RootSession.
DLL_EXPORT int test_session_workspace_enumerate(ITesting *t) {
    Workspace workspace;

    auto docA = std::make_shared<Document>();
    docA->SetPath("a.cpp");
    docA->GetLineCursor().cursor.position = gedit::Point(1, 2);

    auto docB = std::make_shared<Document>();
    docB->SetPath("b.cpp");
    docB->GetLineCursor().cursor.position = gedit::Point(3, 4);

    workspace.AddOpenDocument(docA);
    workspace.AddOpenDocument(docB);
    workspace.SetActiveDocument(docB);

    RootSession session;
    workspace.ToSession(session);

    TR_ASSERT(t, session.documents.size() == 2);
    TR_ASSERT(t, session.documents[0].path == "a.cpp");
    TR_ASSERT(t, session.documents[0].cursorX == 1);
    TR_ASSERT(t, session.documents[1].path == "b.cpp");
    TR_ASSERT(t, session.documents[1].cursorY == 4);
    TR_ASSERT(t, session.activeDocumentIndex == 1);

    // An empty workspace reports no active document.
    Workspace empty;
    RootSession emptySession;
    empty.ToSession(emptySession);
    TR_ASSERT(t, emptySession.documents.empty());
    TR_ASSERT(t, emptySession.activeDocumentIndex == -1);
    return kTR_Pass;
}

// A saved document whose file is gone on disk is skipped, and the rest of the restore continues.
DLL_EXPORT int test_session_workspace_reopen_skips_missing(ITesting *t) {
    Workspace workspace;

    DocumentSession gone;
    gone.path = "this/path/does/not/exist-xyz.cpp";
    TR_ASSERT(t, workspace.ReopenDocument(gone) == nullptr);
    TR_ASSERT(t, workspace.GetOpenDocuments().empty());

    // FromSession over a list of only-missing docs must not throw or abort - it just opens nothing.
    RootSession session;
    session.documents.push_back(gone);
    session.activeDocumentIndex = 0;
    workspace.FromSession(session);
    TR_ASSERT(t, workspace.GetOpenDocuments().empty());
    return kTR_Pass;
}

// Tagged splitters serialise their position into LayoutSession and restore it (by ratio). Covers both
// axes: VSplitView (width) and HSplitView (height; HSplitViewStatus inherits this).
DLL_EXPORT int test_session_splitter_roundtrip(ITesting *t) {
    LayoutSession layout;

    VSplitView vsplit;
    vsplit.SetWidth(100);
    vsplit.SetHeight(40);
    vsplit.SetSessionId("split.workspace");
    vsplit.SetSplitterPos(30);
    vsplit.ToSession(layout);
    TR_ASSERT(t, layout.splitters.size() == 1);
    TR_ASSERT(t, layout.splitters[0].id == "split.workspace");
    TR_ASSERT(t, layout.splitters[0].absolute == 30);

    HSplitView hsplit;
    hsplit.SetWidth(80);
    hsplit.SetHeight(100);
    hsplit.SetSessionId("split.terminal");
    hsplit.SetSplitterPos(60);
    hsplit.ToSession(layout);          // appends under its own id
    TR_ASSERT(t, layout.splitters.size() == 2);

    VSplitView vrestored;
    vrestored.SetWidth(100);
    vrestored.SetHeight(40);
    vrestored.SetSessionId("split.workspace");
    vrestored.FromSession(layout);
    TR_ASSERT(t, (vrestored.GetSplitterPos() >= 29) && (vrestored.GetSplitterPos() <= 31));

    HSplitView hrestored;
    hrestored.SetWidth(80);
    hrestored.SetHeight(100);
    hrestored.SetSessionId("split.terminal");
    hrestored.FromSession(layout);
    TR_ASSERT(t, (hrestored.GetSplitterPos() >= 59) && (hrestored.GetSplitterPos() <= 61));
    return kTR_Pass;
}

// An untagged splitter (no session id) is invisible to the session: writes nothing, restores nothing.
DLL_EXPORT int test_session_splitter_untagged_noop(ITesting *t) {
    VSplitView vsplit;
    vsplit.SetWidth(100);
    vsplit.SetSplitterPos(30);

    LayoutSession layout;
    vsplit.ToSession(layout);
    TR_ASSERT(t, layout.splitters.empty());

    vsplit.FromSession(layout);        // no id, no matching entry -> no-op, no crash
    TR_ASSERT(t, layout.splitters.empty());
    return kTR_Pass;
}
