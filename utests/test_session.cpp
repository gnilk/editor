//
// Created by gnilk on 15.06.26.
//
// Phase-1 session-cache scaffolding tests. Real round-trip serialisation tests (YAML, atomic write,
// restore-with-missing-file) land as the implementation does — these guard the DTO + singleton seams.
//
#include <testinterface.h>
#include "Core/Session/SessionState.h"
#include "Core/Session/SessionManager.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_session(ITesting *t);
DLL_EXPORT int test_session_dto_defaults(ITesting *t);
DLL_EXPORT int test_session_singleton_clear(ITesting *t);
}

DLL_EXPORT int test_session(ITesting *t) {
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
