//
// Created by gnilk on 15.06.26.
//
// Phase-1 session-cache scaffolding tests. Real round-trip serialisation tests (YAML, atomic write,
// restore-with-missing-file) land as the implementation does — these guard the DTO + singleton seams.
//
#include <testinterface.h>
#include <memory>
#include <cstdlib>
#include <filesystem>
#include <unordered_map>
#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"
#include "Core/AssetLoaderBase.h"
#include "Core/Config/Config.h"
#include "Core/Document.h"
#include "Core/DocumentViewState.h"
#include "Core/Workspace.h"
#include "Core/UI/Views/VSplitView.h"
#include "Core/UI/Views/HSplitView.h"
#include "Core/Editor/Views/WorkspaceView.h"
#include "Core/UI/Graphics/ScreenBase.h"
#include "Core/Session/SessionState.h"
#include "Core/Session/SessionManager.h"
#include "Core/Session/SessionSerializer.h"
#include "Core/Session/LayoutSessionSink.h"

using namespace gedit;

namespace {
    // Exposes the protected tree-build path so the expand/collapse round-trip can be driven directly.
    struct SessionTestWorkspaceView : public WorkspaceView {
        void BuildTree() {
            CreateTree();
            PopulateTree();
        }
    };

    // Drives the protected ScreenBase geometry helpers with a stubbed display so the default + clamp
    // logic (and the change callback) can be tested without a real backend / monitor / session.
    struct GeometryTestScreen : public ScreenBase {
        int bx = 0, by = 0, bw = 0, bh = 0;
        bool hasBounds = true;

        bool GetPrimaryDisplayBounds(int &x, int &y, int &width, int &height) override {
            if (!hasBounds) {
                return false;
            }
            x = bx; y = by; width = bw; height = bh;
            return true;
        }
        void Resolve(int &x, int &y, int &width, int &height) {
            ResolveStartupGeometry(x, y, width, height);
        }
        void EmitGeometry(int x, int y, int width, int height) {
            NotifyWindowGeometryChanged(x, y, width, height);
        }
    };

    // Is the rect [gx,gx+gw) x [gy,gy+gh) fully inside the display bounds [x,x+w) x [y,y+h)?
    static bool IsInsideBounds(int gx, int gy, int gw, int gh, int x, int y, int w, int h) {
        return (gx >= x) && (gy >= y) && ((gx + gw) <= (x + w)) && ((gy + gh) <= (y + h));
    }
}

extern "C" {
DLL_EXPORT int test_session(ITesting *t);
DLL_EXPORT int test_session_dto_defaults(ITesting *t);
DLL_EXPORT int test_session_singleton_clear(ITesting *t);
DLL_EXPORT int test_session_documentviewstate_roundtrip(ITesting *t);
DLL_EXPORT int test_session_selection_direction_preserved(ITesting *t);
DLL_EXPORT int test_session_document_roundtrip(ITesting *t);
DLL_EXPORT int test_session_workspace_enumerate(ITesting *t);
DLL_EXPORT int test_session_workspace_reopen_skips_missing(ITesting *t);
DLL_EXPORT int test_session_workspace_reopen_reuses_scanned_node(ITesting *t);
DLL_EXPORT int test_session_workspace_reopen_scans_deep_file(ITesting *t);
DLL_EXPORT int test_session_splitter_roundtrip(ITesting *t);
DLL_EXPORT int test_session_splitter_untagged_noop(ITesting *t);
DLL_EXPORT int test_session_layout_walk_roundtrip(ITesting *t);
DLL_EXPORT int test_session_workspaceview_expandcollapse(ITesting *t);
DLL_EXPORT int test_session_yaml_roundtrip(ITesting *t);
DLL_EXPORT int test_session_yaml_bad_version(ITesting *t);
DLL_EXPORT int test_session_yaml_corrupt(ITesting *t);
DLL_EXPORT int test_session_save_load_roundtrip(ITesting *t);
DLL_EXPORT int test_session_assetloader_replace_kproject(ITesting *t);
DLL_EXPORT int test_session_geometry_resolve(ITesting *t);
DLL_EXPORT int test_session_geometry_callback(ITesting *t);
DLL_EXPORT int test_session_terminal_roundtrip(ITesting *t);
DLL_EXPORT int test_session_terminal_backcompat(ITesting *t);
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

// Reproduces open-bugs.md #4: restoring a session after OpenFolder must reuse the node OpenFolder
// already scanned for that path, not create a second one. Before the fix, ReopenDocument ->
// NewDocumentWithFileRef always parented under the (separate) default workspace root, so the restored
// document ended up on a brand-new node/root disconnected from the one the WorkspaceView displays -
// selecting that displayed (still doc-less) node from the UI would then open a second Document for the
// same file.
DLL_EXPORT int test_session_workspace_reopen_reuses_scanned_node(ITesting *t) {
    Workspace workspace;
    TR_ASSERT(t, workspace.OpenFolder("."));
    TR_ASSERT(t, workspace.GetProjectRoots().size() == 1);

    // With max_scan_depth=1 (FS-6 W6 default), "testfiles/" is a frontier dir after OpenFolder.
    // ScanNode it first so ConvertUTF.cpp is in the model tree — this exercises the lazy-expansion
    // path that a real user would trigger by expanding the folder in the UI.
    auto rootNode = workspace.GetProjectRoots()[0]->GetRootNode();
    auto testfilesNode = rootNode->FindNodeWithPath(std::filesystem::current_path() / "testfiles");
    TR_ASSERT(t, testfilesNode != nullptr);
    workspace.ScanNode(testfilesNode);

    // Find the scanned (path-only, no document yet) node for a known fixture file.
    std::function<Workspace::Node::Ref(Workspace::Node::Ref)> findConvertUTF = [&](Workspace::Node::Ref node) -> Workspace::Node::Ref {
        if (node->GetNodePath().filename() == "ConvertUTF.cpp") {
            return node;
        }
        std::vector<Workspace::Node::Ref> children;
        node->FlattenChilds(children);
        for (auto &child : children) {
            if (auto found = findConvertUTF(child); found != nullptr) {
                return found;
            }
        }
        return nullptr;
    };
    auto scannedNode = findConvertUTF(rootNode);
    TR_ASSERT(t, scannedNode != nullptr);
    TR_ASSERT(t, scannedNode->GetDocument() == nullptr);

    // Restore a session that references that same file by its (relative) path, as ReopenDocument does.
    DocumentSession docSession;
    docSession.path = "testfiles/ConvertUTF.cpp";
    auto document = workspace.ReopenDocument(docSession);
    TR_ASSERT(t, document != nullptr);

    // No duplicate root/node was created - the restored document landed on the SAME node OpenFolder
    // scanned, and there's exactly one open document for this file.
    TR_ASSERT(t, workspace.GetProjectRoots().size() == 1);
    TR_ASSERT(t, scannedNode->GetDocument() == document);
    TR_ASSERT(t, workspace.GetOpenDocuments().size() == 1);

    return kTR_Pass;
}

// Reproduces open-bugs.md #8: restoring a session that references a file BELOW the initial
// max_scan_depth frontier. OpenFolder scans only to the depth bound, so the deep file is absent from
// the model tree; ReopenDocument -> NewDocumentWithFileRef -> FindNodeForPath then misses, and (before
// the fix) the file is parented under a separate 'default' root instead of its real location. The fix
// scans just the path-to-the-file into its owning project root, so the restored document lands on a
// node reachable from the real root at its true path.
DLL_EXPORT int test_session_workspace_reopen_scans_deep_file(ITesting *t) {
    // Force the shallow frontier: testfiles/ is entered at depth 0 but NOT descended (depth+1 == 1 ==
    // maxDepth), so testfiles/ConvertUTF.cpp is below the frontier and not in the tree after OpenFolder.
    int prevDepth = Config::Instance()["workspace"].GetInt("max_scan_depth", 6);
    Config::Instance()["workspace"].SetInt("max_scan_depth", 1);

    Workspace workspace;
    bool opened = workspace.OpenFolder(".");

    auto rootNode = workspace.GetProjectRoots().empty() ? nullptr
                                                        : workspace.GetProjectRoots()[0]->GetRootNode();
    auto absFile = std::filesystem::current_path() / "testfiles" / "ConvertUTF.cpp";
    // Precondition: the frontier was NOT descended, so the deep file really is absent right now.
    bool absentBefore = (rootNode != nullptr) && (rootNode->FindNodeWithPath(absFile) == nullptr);

    // Restore a session entry for that deep file (relative path, exactly as a saved session carries it).
    DocumentSession docSession;
    docSession.path = "testfiles/ConvertUTF.cpp";
    auto document = workspace.ReopenDocument(docSession);

    // The restored document must be reachable from the REAL project root at its true path - not dumped
    // under a separate 'default' root.
    auto placedNode = (rootNode != nullptr) ? rootNode->FindNodeWithPath(absFile) : nullptr;
    size_t rootCount = workspace.GetProjectRoots().size();

    // Restore config BEFORE asserting so an assert-abort can't leak the setting to other tests.
    Config::Instance()["workspace"].SetInt("max_scan_depth", prevDepth);

    TR_ASSERT(t, opened);
    TR_ASSERT(t, rootNode != nullptr);
    TR_ASSERT(t, absentBefore);                       // deep file genuinely below the frontier
    TR_ASSERT(t, document != nullptr);
    TR_ASSERT(t, placedNode != nullptr);              // FAILS pre-fix: file lands under 'default' root
    TR_ASSERT(t, placedNode->GetDocument() == document);
    TR_ASSERT(t, rootCount == 1);                     // no separate 'default' root was spun up
    return kTR_Pass;
}

// Tagged splitters serialise their position into LayoutSession and restore it (by ratio). Covers both
// axes: VSplitView (width) and HSplitView (height; HSplitViewStatus inherits this).
DLL_EXPORT int test_session_splitter_roundtrip(ITesting *t) {
    LayoutSession layout;

    LayoutSessionSink sink(layout);

    VSplitView vsplit;
    vsplit.SetWidth(100);
    vsplit.SetHeight(40);
    vsplit.SetSessionId("split.workspace");
    vsplit.SetSplitterPos(30);
    vsplit.ToSession(sink);
    TR_ASSERT(t, layout.splitters.size() == 1);
    TR_ASSERT(t, layout.splitters[0].id == "split.workspace");
    TR_ASSERT(t, layout.splitters[0].absolute == 30);

    HSplitView hsplit;
    hsplit.SetWidth(80);
    hsplit.SetHeight(100);
    hsplit.SetSessionId("split.terminal");
    hsplit.SetSplitterPos(60);
    hsplit.ToSession(sink);          // appends under its own id
    TR_ASSERT(t, layout.splitters.size() == 2);

    VSplitView vrestored;
    vrestored.SetWidth(100);
    vrestored.SetHeight(40);
    vrestored.SetSessionId("split.workspace");
    vrestored.FromSession(sink);
    TR_ASSERT(t, (vrestored.GetSplitterPos() >= 29) && (vrestored.GetSplitterPos() <= 31));

    HSplitView hrestored;
    hrestored.SetWidth(80);
    hrestored.SetHeight(100);
    hrestored.SetSessionId("split.terminal");
    hrestored.FromSession(sink);
    TR_ASSERT(t, (hrestored.GetSplitterPos() >= 59) && (hrestored.GetSplitterPos() <= 61));
    return kTR_Pass;
}

// An untagged splitter (no session id) is invisible to the session: writes nothing, restores nothing.
DLL_EXPORT int test_session_splitter_untagged_noop(ITesting *t) {
    VSplitView vsplit;
    vsplit.SetWidth(100);
    vsplit.SetSplitterPos(30);

    LayoutSession layout;
    LayoutSessionSink sink(layout);
    vsplit.ToSession(sink);
    TR_ASSERT(t, layout.splitters.empty());

    vsplit.FromSession(sink);        // no id, no matching entry -> no-op, no crash
    TR_ASSERT(t, layout.splitters.empty());
    return kTR_Pass;
}

// CollectLayout/ApplyLayout recurse the whole view subtree, so one call at the root persists every
// tagged splitter regardless of depth. Mirrors the real shape: outer HSplitView (terminal) holds the
// inner VSplitView (workspace) as its upper panel.
DLL_EXPORT int test_session_layout_walk_roundtrip(ITesting *t) {
    HSplitView outer;
    outer.SetWidth(120);
    outer.SetHeight(60);
    outer.SetSessionId("split.terminal");

    VSplitView inner;
    inner.SetSessionId("split.workspace");
    outer.SetUpper(&inner);            // nests inner; AddView initialises it within outer's rect
    inner.SetSplitterPos(40);
    outer.SetSplitterPos(45);

    // One CollectLayout at the outer view must pull BOTH splitters out of the subtree.
    LayoutSession layout;
    LayoutSessionSink sink(layout);
    outer.CollectLayout(sink);
    TR_ASSERT(t, layout.splitters.size() == 2);
    bool sawTerminal = false;
    bool sawWorkspace = false;
    for (const auto &s : layout.splitters) {
        if (s.id == "split.terminal") { sawTerminal = true; }
        if (s.id == "split.workspace") { sawWorkspace = true; }
    }
    TR_ASSERT(t, sawTerminal && sawWorkspace);

    // Move both, then ApplyLayout at the root restores each by id (within the ratio tolerance).
    int outerBefore = outer.GetSplitterPos();
    int innerBefore = inner.GetSplitterPos();
    outer.SetSplitterPos(20);
    inner.SetSplitterPos(15);
    outer.ApplyLayout(sink);
    TR_ASSERT(t, std::abs(outer.GetSplitterPos() - outerBefore) <= 1);
    TR_ASSERT(t, std::abs(inner.GetSplitterPos() - innerBefore) <= 1);
    return kTR_Pass;
}

// WorkspaceView snapshots the tree expand/collapse state and re-applies a restored seed on rebuild.
DLL_EXPORT int test_session_workspaceview_expandcollapse(ITesting *t) {
    // Build a real tree over the 'testfiles' fixture with the folder monitor off (no side effects).
    bool prevMonitor = Config::Instance()["foldermonitor"].GetBool("enabled", true);
    Config::Instance()["foldermonitor"].SetBool("enabled", false);

    auto testPath = std::filesystem::current_path() / "testfiles";
    Editor::Instance().GetWorkspace()->OpenFolder(testPath.string());

    SessionTestWorkspaceView wv;
    wv.BuildTree();
    std::unordered_map<std::string, bool> saved;
    wv.SaveExpandCollapseState(saved);

    // Fresh view: seed the saved state BEFORE building, then build -> the one-shot seed is applied.
    SessionTestWorkspaceView wv2;
    wv2.RestoreExpandCollapseState(saved);   // no tree yet -> just stores the seed
    wv2.BuildTree();
    std::unordered_map<std::string, bool> restored;
    wv2.SaveExpandCollapseState(restored);

    // Restore config BEFORE asserting, so an assert-abort can't leak the disabled monitor to other tests.
    Config::Instance()["foldermonitor"].SetBool("enabled", prevMonitor);

    TR_ASSERT(t, !saved.empty());          // the auto-expanded root is recorded
    TR_ASSERT(t, restored == saved);       // one-shot seed re-applied -> identical expanded set
    return kTR_Pass;
}

// Full RootSession survives a YAML emit -> parse round-trip (selection + view-mode + geometry + splitters
// + tree state). The float splitter ratio is compared with tolerance (text encoding is lossy).
DLL_EXPORT int test_session_yaml_roundtrip(ITesting *t) {
    RootSession s;
    s.primaryRoot = "/home/u/proj";
    s.extraRoots = {"/home/u/lib"};
    s.activeDocumentIndex = 1;

    DocumentSession d0;
    d0.path = "a.cpp";
    d0.cursorX = 3; d0.cursorY = 9; d0.wantedColumn = 5;
    d0.idxActiveLine = 9; d0.viewTopLine = 4; d0.viewBottomLine = 30;
    d0.viewMode = DocumentViewMode::kText;
    d0.selection.isActive = true;
    d0.selection.startX = 1; d0.selection.startY = 2; d0.selection.endX = 7; d0.selection.endY = 8;

    DocumentSession d1;
    d1.path = "b.bin";
    d1.cursorX = 0; d1.cursorY = 100;
    d1.viewMode = DocumentViewMode::kHex;

    s.documents = {d0, d1};
    s.expandCollapse["/home/u/proj/src"] = true;

    s.layout.window = {120, 40, 1280, 800};
    s.layout.focusedTopView = "editor";
    s.layout.splitters.push_back(SplitterSession{"split.workspace", 30, 0.3f});
    s.layout.splitters.push_back(SplitterSession{"split.terminal", 18, 0.75f});

    std::string yaml = SessionSerializer::ToYaml(s);
    TR_ASSERT(t, !yaml.empty());

    RootSession r;
    TR_ASSERT(t, SessionSerializer::FromYaml(yaml, r));

    TR_ASSERT(t, r.version == RootSession::kVersion);
    TR_ASSERT(t, r.primaryRoot == s.primaryRoot);
    TR_ASSERT(t, r.extraRoots.size() == 1);
    TR_ASSERT(t, r.extraRoots[0] == "/home/u/lib");
    TR_ASSERT(t, r.activeDocumentIndex == 1);

    TR_ASSERT(t, r.documents.size() == 2);
    TR_ASSERT(t, r.documents[0].path == "a.cpp");
    TR_ASSERT(t, r.documents[0].cursorX == 3);
    TR_ASSERT(t, r.documents[0].cursorY == 9);
    TR_ASSERT(t, r.documents[0].wantedColumn == 5);
    TR_ASSERT(t, r.documents[0].idxActiveLine == 9);
    TR_ASSERT(t, r.documents[0].viewTopLine == 4);
    TR_ASSERT(t, r.documents[0].viewBottomLine == 30);
    TR_ASSERT(t, r.documents[0].viewMode == DocumentViewMode::kText);
    TR_ASSERT(t, r.documents[0].selection.isActive);
    TR_ASSERT(t, r.documents[0].selection.startX == 1);
    TR_ASSERT(t, r.documents[0].selection.endY == 8);
    TR_ASSERT(t, r.documents[1].path == "b.bin");
    TR_ASSERT(t, r.documents[1].cursorY == 100);
    TR_ASSERT(t, r.documents[1].viewMode == DocumentViewMode::kHex);

    TR_ASSERT(t, r.expandCollapse.size() == 1);
    TR_ASSERT(t, r.expandCollapse.count("/home/u/proj/src") == 1);

    TR_ASSERT(t, r.layout.window.x == 120);
    TR_ASSERT(t, r.layout.window.width == 1280);
    TR_ASSERT(t, r.layout.window.height == 800);
    TR_ASSERT(t, r.layout.focusedTopView == "editor");
    TR_ASSERT(t, r.layout.splitters.size() == 2);
    TR_ASSERT(t, r.layout.splitters[0].id == "split.workspace");
    TR_ASSERT(t, r.layout.splitters[0].absolute == 30);
    TR_ASSERT(t, (r.layout.splitters[1].relative > 0.74f) && (r.layout.splitters[1].relative < 0.76f));
    return kTR_Pass;
}

// A newer/unknown version is ignored (caller starts clean) rather than mis-parsed.
DLL_EXPORT int test_session_yaml_bad_version(ITesting *t) {
    RootSession s;
    s.version = RootSession::kVersion + 99;
    std::string yaml = SessionSerializer::ToYaml(s);

    RootSession r;
    TR_ASSERT(t, !SessionSerializer::FromYaml(yaml, r));
    return kTR_Pass;
}

// Malformed YAML never throws; it returns false so startup can proceed clean.
DLL_EXPORT int test_session_yaml_corrupt(ITesting *t) {
    RootSession r;
    TR_ASSERT(t, !SessionSerializer::FromYaml("{ this: is: not: valid", r));
    TR_ASSERT(t, !SessionSerializer::FromYaml("", r));
    return kTR_Pass;
}

// SessionManager Save -> Load round-trips through a real session.yml resolved via AssetLoader kProject,
// written atomically (no leftover .tmp).
DLL_EXPORT int test_session_save_load_roundtrip(ITesting *t) {
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();

    auto testRoot = std::filesystem::temp_directory_path() / "goatedit_sess_test";
    auto projDir = testRoot / ".goatedit";
    std::error_code ec;
    std::filesystem::remove_all(testRoot, ec);
    std::filesystem::create_directories(projDir);
    // ReplaceSearchPath (as production EstablishProjectDir does) so this is the SOLE kProject path —
    // hermetic regardless of any kProject path the editor registered at init (e.g. a .goatedit under
    // the test's cwd left by a real run).
    assetLoader.ReplaceSearchPath(projDir, AssetLoaderBase::kLocationType::kProject);

    auto writePath = assetLoader.ResolveWritePath("session.yml", AssetLoaderBase::kLocationType::kProject);
    TR_ASSERT(t, writePath == (projDir / "session.yml"));

    auto &mgr = SessionManager::Instance();
    mgr.Clear();
    mgr.CurrentSession().activeDocumentIndex = 2;
    DocumentSession d;
    d.path = "x.cpp";
    d.cursorY = 42;
    d.viewMode = DocumentViewMode::kHex;
    mgr.CurrentSession().documents.push_back(d);

    TR_ASSERT(t, mgr.Save());
    TR_ASSERT(t, std::filesystem::exists(writePath));
    TR_ASSERT(t, !std::filesystem::exists(writePath.string() + ".tmp"));   // atomic: no leftover temp

    mgr.Clear();
    TR_ASSERT(t, mgr.CurrentSession().documents.empty());
    TR_ASSERT(t, mgr.Load());
    TR_ASSERT(t, mgr.CurrentSession().activeDocumentIndex == 2);
    TR_ASSERT(t, mgr.CurrentSession().documents.size() == 1);
    TR_ASSERT(t, mgr.CurrentSession().documents[0].path == "x.cpp");
    TR_ASSERT(t, mgr.CurrentSession().documents[0].cursorY == 42);
    TR_ASSERT(t, mgr.CurrentSession().documents[0].viewMode == DocumentViewMode::kHex);

    mgr.Clear();
    std::filesystem::remove_all(testRoot, ec);
    return kTR_Pass;
}

// ReplaceSearchPath gives a single, current kProject write-path (re-pointing a project root must not
// stack duplicate paths that make ResolveWritePath ambiguous).
DLL_EXPORT int test_session_assetloader_replace_kproject(ITesting *t) {
    AssetLoaderBase loader;
    auto a = std::filesystem::temp_directory_path() / "goat_rp_a";
    auto b = std::filesystem::temp_directory_path() / "goat_rp_b";
    std::error_code ec;
    std::filesystem::create_directories(a);
    std::filesystem::create_directories(b);

    loader.AddSearchPath(a, AssetLoaderBase::kLocationType::kProject);
    TR_ASSERT(t, loader.ResolveWritePath("session.yml", AssetLoaderBase::kLocationType::kProject) == (a / "session.yml"));

    loader.ReplaceSearchPath(b, AssetLoaderBase::kLocationType::kProject);
    TR_ASSERT(t, loader.ResolveWritePath("session.yml", AssetLoaderBase::kLocationType::kProject) == (b / "session.yml"));

    std::filesystem::remove_all(a, ec);
    std::filesystem::remove_all(b, ec);
    return kTR_Pass;
}

// ScreenBase::ResolveStartupGeometry (docs/session-cache.md §4.2): with no request it centres a fraction
// of the display; a requested geometry that fits is preserved; an off-display/oversize request is clamped
// fully inside the display bounds. Pure graphics math — no session. Asserts the SAFETY PROPERTY
// (inside-bounds), not magic numbers.
DLL_EXPORT int test_session_geometry_resolve(ITesting *t) {
    GeometryTestScreen screen;
    screen.bx = 100; screen.by = 50; screen.bw = 1600; screen.bh = 900;
    int x = 0, y = 0, w = 0, h = 0;

    // (a) No request: centred, sized to a fraction, fully inside the display.
    screen.Resolve(x, y, w, h);
    TR_ASSERT(t, (w > 0) && (h > 0));
    TR_ASSERT(t, w < screen.bw);
    TR_ASSERT(t, h < screen.bh);
    TR_ASSERT(t, IsInsideBounds(x, y, w, h, screen.bx, screen.by, screen.bw, screen.bh));

    // (b) Requested geometry that already fits is returned untouched.
    screen.SetRequestedWindowGeometry(300, 200, 800, 600);
    screen.Resolve(x, y, w, h);
    TR_ASSERT(t, (x == 300) && (y == 200) && (w == 800) && (h == 600));

    // (c) Off-display / oversize request is clamped fully inside the bounds (monitor changed).
    screen.SetRequestedWindowGeometry(5000, 5000, 4000, 3000);
    screen.Resolve(x, y, w, h);
    TR_ASSERT(t, IsInsideBounds(x, y, w, h, screen.bx, screen.by, screen.bw, screen.bh));

    // (d) No display bounds + no request -> still a valid (fallback) size, never zero.
    screen.SetRequestedWindowGeometry(0, 0, 0, 0);   // clears the request
    screen.hasBounds = false;
    screen.Resolve(x, y, w, h);
    TR_ASSERT(t, (w > 0) && (h > 0));
    return kTR_Pass;
}

// NotifyWindowGeometryChanged forwards live geometry to the registered handler (the glue layer's hook
// that persists it); with no handler set it must be a safe no-op.
DLL_EXPORT int test_session_geometry_callback(ITesting *t) {
    int gx = 0, gy = 0, gw = 0, gh = 0;
    bool called = false;

    GeometryTestScreen screen;
    screen.SetWindowGeometryChangedHandler([&](int x, int y, int width, int height) {
        called = true;
        gx = x; gy = y; gw = width; gh = height;
    });
    screen.EmitGeometry(12, 34, 1024, 768);
    TR_ASSERT(t, called);
    TR_ASSERT(t, (gx == 12) && (gy == 34) && (gw == 1024) && (gh == 768));

    // No handler -> no crash.
    GeometryTestScreen unwired;
    unwired.EmitGeometry(1, 2, 3, 4);
    return kTR_Pass;
}

// --- TS-5b: TerminalSession block index round-trips through YAML (terminal-scrollback.md §8.1) ---------

DLL_EXPORT int test_session_terminal_roundtrip(ITesting *t) {
    RootSession s;
    s.primaryRoot = "/home/u/proj";
    s.terminal.scrollbackFile = "terminal_scrollback.bin";

    // A closed block with full metadata (OSC 133: known exit + cwd), language set (Phase-4 forward-compat).
    TerminalBlockSession b0;
    b0.id = 1; b0.command = "cmake -B build";
    b0.startLine = 0; b0.endLine = 12;
    b0.exitCode = 0; b0.cwd = "/home/u/proj"; b0.source = 1; b0.language = "cmake";

    // The (formerly open) block, closed at the saved tail: exit UNKNOWN (the sentinel must survive YAML).
    TerminalBlockSession b1;
    b1.id = 2; b1.command = "ls -la";
    b1.startLine = 12; b1.endLine = 20;
    b1.exitCode = TerminalBlockSession::kExitCodeUnknown; b1.source = 0;

    s.terminal.blocks = {b0, b1};

    std::string yaml = SessionSerializer::ToYaml(s);
    TR_ASSERT(t, !yaml.empty());

    RootSession r;
    TR_ASSERT(t, SessionSerializer::FromYaml(yaml, r));

    TR_ASSERT(t, r.terminal.scrollbackFile == "terminal_scrollback.bin");
    TR_ASSERT(t, r.terminal.blocks.size() == 2);

    TR_ASSERT(t, r.terminal.blocks[0].id == 1);
    TR_ASSERT(t, r.terminal.blocks[0].command == "cmake -B build");
    TR_ASSERT(t, r.terminal.blocks[0].startLine == 0);
    TR_ASSERT(t, r.terminal.blocks[0].endLine == 12);
    TR_ASSERT(t, r.terminal.blocks[0].exitCode == 0);
    TR_ASSERT(t, r.terminal.blocks[0].cwd == "/home/u/proj");
    TR_ASSERT(t, r.terminal.blocks[0].source == 1);
    TR_ASSERT(t, r.terminal.blocks[0].language == "cmake");

    TR_ASSERT(t, r.terminal.blocks[1].id == 2);
    TR_ASSERT(t, r.terminal.blocks[1].command == "ls -la");
    TR_ASSERT(t, r.terminal.blocks[1].startLine == 12);
    TR_ASSERT(t, r.terminal.blocks[1].endLine == 20);
    // The unknown-exit sentinel round-trips, distinct from a real status (incl. -1).
    TR_ASSERT(t, r.terminal.blocks[1].exitCode == TerminalBlockSession::kExitCodeUnknown);
    TR_ASSERT(t, r.terminal.blocks[1].cwd.empty());
    TR_ASSERT(t, r.terminal.blocks[1].source == 0);
    TR_ASSERT(t, r.terminal.blocks[1].language.empty());

    return kTR_Pass;
}

// A session.yml predating the feature (no `terminal:` key) restores an empty terminal, defaulted file name.
DLL_EXPORT int test_session_terminal_backcompat(ITesting *t) {
    const std::string legacyYaml =
        "version: 1\n"
        "primaryRoot: /home/u/proj\n"
        "documents: []\n";

    RootSession r;
    TR_ASSERT(t, SessionSerializer::FromYaml(legacyYaml, r));
    TR_ASSERT(t, r.terminal.blocks.empty());
    TR_ASSERT(t, r.terminal.scrollbackFile == "terminal_scrollback.bin");
    return kTR_Pass;
}
