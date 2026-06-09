# Workspace refactor plan

Branch: `dev_workspace`

## Why

The current `Workspace` / `Desktop` / `Node` / `EditorModel` / `Editor::openModels`
arrangement spreads ownership of a buffer across several objects, so it is unclear where
a text buffer lives or who owns its metadata. Concretely:

- `Node` does two unrelated jobs: it is a filesystem-tree presentation entry **and** the
  owner of a live document (`model` + `controller`). There is no `Document` type.
- An `EditorModel` is owned by **both** `Editor::openModels` *and* a `Node::model` — double
  bookkeeping with no single authority.
- Opening a folder eagerly allocates an `EditorModel` + `EditController` + `TextBuffer` for
  **every file in the tree** (`ReadFolderToNode` -> `NewModelWithFileRef` recursively), even
  files that are never opened.
- `Desktop` is a thin wrapper whose only real job is hosting the (currently broken) folder
  monitor; the header even admits the name is a placeholder.
- Defects/cruft: `GetDefaultWorkspace` returns the hard-coded `rootNodes["default"]` instead
  of the configured name; `GetNamedWorkspace` searches the literal string `"name"`; the
  `Workspace::models` vector is never populated; `Editor::NewModel` is a `return nullptr`
  stub; `NewModelWithFileRef` builds and discards a dead `EditController`.

The current architecture is treated as disposable.

## Target model — three concepts, one owner each

| Concept | What it is | Owns | Replaces |
|---|---|---|---|
| **`Document`** | one open, editable buffer + everything about it | `TextBuffer`, `EditController`, cursor/undo/selection/search, **path**, **file metadata**, dirty/readonly flags, `Load()`/`Save()` | `EditorModel` + the identity bits that leaked into `Node` |
| **`Workspace`** | the app's open-document set **and** the browseable roots | the `Document` list (single source of truth), the `ProjectRoot` list, active-document pointer | `Workspace` + `Editor::openModels` + dead `models` vector |
| **`Workspace::ProjectRoot`** | one top-level browseable folder | a `Node` tree, its `path`, its folder **monitor** | `Desktop` |
| **`Workspace::Node`** | a *presentation* entry in a browse tree (file / folder / virtual group) | type, path, displayName, children, meta, and a **non-owning** link to a `Document` if open | the dual-role `Node`, minus model/controller ownership |

**The one rule:**

> `Document` owns the buffer and its metadata. The tree only *references* documents — it
> never owns one. `Workspace` owns the documents. No object holds an owning ref to a
> document except the workspace's open-document list.

This removes the double-bookkeeping and the eager allocation: a `Document` exists only for a
file that has actually been opened; scanned-but-unopened files are cheap path-only `Node`s.

### Naming decisions

- Root-folder type: **`ProjectRoot`** (deliberately avoiding bare `Root`, which is overloaded
  by filesystems/users/etc.).
- `EditorModel` -> **`Document`** (aligns the C++ core with the existing JS `DocumentAPI`).
- Tree node: keep **`Node`** (becomes accurate once it is presentation-only); optional later
  rename to `FileNode` if desired.

### Target headers (interface sketch)

```cpp
// Document.h  — was EditorModel, elevated to own its identity + controller
class Document {
    using Ref = std::shared_ptr<Document>;
    std::filesystem::path   path;
    FileMeta                meta;          // size, readonly, type
    bool isDirty, isReadOnly, isActive;
    TextBuffer::Ref         textBuffer;
    EditController::Ref      controller;    // moved off Node
    // cursor / undo / selection / search … (already here today)
    bool Load();                            // moved off Node::LoadData
    bool Save();                            // moved off Node::SaveData
};

// Workspace.h
class Workspace {
    std::vector<Document::Ref> openDocuments;   // source of truth (replaces Editor::openModels)
    Document::Ref              activeDocument;   // explicit, O(1) (replaces isActive scan)
    std::vector<ProjectRoot::Ref> roots;         // replaces rootNodes map<string,Desktop>
    Node::Ref                  looseFiles;       // optional virtual root for out-of-tree open files

    Document::Ref OpenFile(path);     // ensure a Document exists+active; works for loose files
    ProjectRoot::Ref AddRoot(path);   // scan a folder into path-only nodes
    Document::Ref OpenNode(Node::Ref);// lazily create the Document for a browse node
    void          Close(Document::Ref);
};

// Workspace::ProjectRoot  — was Desktop
class ProjectRoot {
    std::filesystem::path             path;
    Node::Ref                         rootNode;
    FolderMonitor::MonitorPoint::Ref  monitor;   // room for later; created-but-disabled now
    void Scan();                                   // initial fill, uses ApplyFsEntry
    void OnFsCreated(path); void OnFsRemoved(path); void OnFsChanged(path);  // monitor seam
private:
    Node::Ref ApplyFsEntry(const fs::path&, NodeType);  // the ONE fs->tree mutator
};

// Workspace::Node — pure presentation now
class Node {
    NodeType                  type;       // folder / file / virtual
    std::filesystem::path     path;
    std::string               displayName;
    FileMeta                  meta;
    std::vector<Node::Ref>    children;
    std::weak_ptr<Document>   openDoc;    // non-owning; set iff the file is currently open
};
```

## Making room for the folder monitor (designed, not implemented)

The monitor is the reason `Desktop` exists, so the replacement hosts it cleanly. The key
move — which the current code gets wrong — is to **unify the initial scan and live updates
behind one mutator**:

- The monitor lives on `ProjectRoot` (one watched point per root folder).
- Filesystem deltas land on `ProjectRoot::OnFsCreated / OnFsRemoved / OnFsChanged`, which call
  the **same** `ApplyFsEntry` that the initial `Scan()` uses. Today `ReadFolderToNode` (scan)
  and `AddFromFileEvent` (monitor) are separate code — that is why the monitor is fragile.
- The monitor **only mutates `Node`s, never `Document`s**. Clean split: monitor <-> browse
  tree; open/close <-> documents.
- If a changed/removed file is currently open (`Node::openDoc` alive), `ProjectRoot` raises a
  separate document-level signal (future: "reload from disk?" / "deleted on disk" banner).
  Define the hook now, no behavior yet.
- Stays gated behind `foldermonitor.enabled`. `ProjectRoot` is born with a `monitor` member
  and the three `OnFs*` entry points routing to `ApplyFsEntry`, but the monitor is constructed
  disabled. That is the "room."

## Ordered steps — each compiles, keeps the `workspace` test module green

- **Step 0 — Pin the contract.** Extend `utests/test_workspace.cpp` to assert the behaviors
  that must survive: open a file -> in open list + active; open a folder -> tree populated,
  files *not* yet opened; open a loose file outside the root -> opens; close -> active moves
  to a neighbor; reopen already-open file -> just re-activates.
- **Step 1 — Move file identity onto the model.** Add `path` + `FileMeta` + `Load()`/`Save()`
  to `EditorModel`. Repoint callers of `Node::LoadData/SaveData/GetNodePath`
  (`Editor::OpenModelFromWorkspace`/`LoadModel`, `DocumentAPI`). Green.
- **Step 2 — Move the controller onto the model.** `EditorModel` creates/owns its
  `EditController`; delete `Node::controller` and the dead local in `NewModelWithFileRef`.
  Update `EditorView.cpp` to pull the controller from the model. Green.
- **Step 3 — Workspace owns the open-document list; delete `Editor::openModels`.** Move the
  open list + explicit `activeDocument` pointer into `Workspace`. `Editor` accessors become
  thin delegates (callers in `EditorAPI`, `EditorHeaderView`, `WorkspaceView` keep working).
  Replace the `isActive` scan with the pointer. Dead `Workspace::models` becomes this list.
  Green.
- **Step 4 — Browse tree reference-only + lazy.** Folder scan creates path-only `Node`s.
  `Workspace::OpenNode` lazily creates the `Document`, registers it, sets `Node::openDoc`.
  Add a test asserting a scanned-but-unopened file has no `Document`. Green.
- **Step 5 — Replace `Desktop` with `ProjectRoot`; unify fs->tree.** Swap `rootNodes` map for
  `vector<ProjectRoot::Ref>` (+ optional `looseFiles` virtual root). Extract `ApplyFsEntry`;
  `Scan()` uses it; add the disabled `monitor` member and `OnFs*` seam. Update
  `WorkspaceView` (`GetDesktops`->`GetRoots`). Fix the `current_path()` chdir-per-root conflict
  (resolve relative paths against the root instead of chdir-ing). Green.
- **Step 6 — Sweep the defects** (mostly gone by now): `rootNodes["default"]` hardcode, the
  `find("name")` literal bug, the `Editor::NewModel` stub, remaining cruft. Green.
- **Step 7 — (Optional, mechanical, last) rename for clarity.** `EditorModel -> Document`,
  point `DocumentAPI` at `Document` (currently wraps a `Node`). Isolated commit so the
  conceptual diff in 1–6 stays readable.

## Decisions / deviations

- **Controller ownership (resolved 2026-06-09).** A first cut of Step 2 made `EditorModel`
  *own* its `EditController` (with a non-owning back-pointer). That created a Model<->Controller
  cycle, which is a no-go. Decision: keep the dependency **one-way, `EditController -> EditorModel`**
  — the controller owns the model, because the model is local to that controller chain. This
  matches how MVC actually fits here: it made sense for the editor, but never for the other
  controllers (they have no model — which is also why `WorkspaceView` never got a controller).
  Step 2 was reverted; the model does **not** own the controller.
- **`EditController` is half-dead** (mostly delegates to the model) and is a candidate for
  dissolution **later** — explicitly not now. When revisited, the key finding: `BaseController`
  is not used polymorphically anywhere; it is a *single-line-edit helper bag*
  (`DefaultEditLine/DefaultEditSpecial/AddCharToLine/RemoveCharFromLine`) that controllers inherit
  — and `QuickCommandController` just *composes* one. The clean path is to extract those helpers
  into a model-agnostic utility and fold `EditController`'s remaining pipeline into the model.

- **chdir-per-root deferred (2026-06-09, during Step 5).** The `current_path()` chdir in
  `Editor::OpenModelOrFolder` was left as-is. Switching to resolve relative paths against a
  ProjectRoot is entangled with the CWD-based default root and asset loading; a partial fix is
  riskier than its value. Step 5's structural goal (coherent multi-root list, no inconsistent
  map keying) is done; the chdir change is its own future task. With absolute paths stored in
  nodes, opening a second root does not corrupt the first - the only effect is the ambient CWD.

- **DocumentAPI re-wrapping deferred (2026-06-09, during Step 7).** Step 7 renamed the class
  `EditorModel -> Document` (files, includes, guard, CMake, logger category). The JS-facing
  `DocumentAPI` still wraps a `Workspace::Node`, not the `Document`. Re-pointing it is a behavioral
  change (e.g. SaveAs/rename must keep the tree node's path in sync, which the Node currently owns),
  so it is its own task rather than part of the mechanical rename.

- **Step 7b - Model-named identifier sweep (2026-06-09).** After the class rename, the
  `Model`-named functions/members read wrong, so they were renamed to `Document` across the
  codebase (all backends under `src/Core`, `main.cpp`, tests): e.g. `NewModel->NewDocument`,
  `AddOpenModel->AddOpenDocument`, `GetActiveModel->GetActiveDocument`, `IsModelOpen->
  IsDocumentOpen`, `GetNodeFromModel->GetNodeFromDocument`, `EnsureModelForNode->
  EnsureDocumentForNode`, `Node::Get/SetModel->Get/SetDocument`, `FindModel->FindDocument`,
  members `openModels->openDocuments` / `activeModel->activeDocument` / `editorModel->document`,
  logger `"EditModel"->"Document"`. Intentionally left: the navigation view-model
  (`VerticalNavigationViewModel`/`VNavModel`); the bare `model` param/member token (pervasive,
  also in third-party code); node-typed locals (`nodeModel`/`nodeForModel`); and historical TODO
  comments in `main.cpp`. JS API surface (`DocumentAPI`/`EditorAPI` names) unchanged.

## Status

Steps 1-7 (+7b) complete and on `dev_workspace` (each its own commit, verified-green set passing).
The class is `Document` and the `Model`-named API has been swept to `Document` naming.
Deferred follow-ups: chdir-per-root resolution, DocumentAPI->Document re-wrapping, and the
(separately-decided) EditController dissolution.

Next: **Phase 2 — the buffer/window split** (below). Planned, not yet started.

---

# Phase 2 — the buffer/window split (ViewState extraction + controller-to-view)

Branch: continue on `dev_workspace`.

## Why — the one asymmetry behind all the confusion

Every other controller/view in the app is a clean **1 : 1 : 1** relationship that owns its
parts: `TerminalController` is a value member of `TerminalView` (`TerminalView.h:45`),
`QuickCommandController` is a value member of `Editor` (`Editor.h:221`). The editor is the
odd one out, and the reason is a cardinality the others don't have:

> **One `EditorView` : N `Document`s.** There is exactly one `EditorView` for the program's
> life (a stack value in `main.cpp:476`, wired into the layout by pointer, never recreated —
> `ReInitView` runs on the same object). That single view *re-points* at whichever document is
> active (`EditorView.cpp:41,81`).

Because the single view roves over N documents, **per-document editing state (cursor, scroll,
selection) has to persist per document** while the view moves between them. `Document` was the
only per-document home available, so the cursor/selection/scroll landed there
(`Document.h:320-323`: `lineCursor`, `currentSelection`, `verticalNavigationViewModel`,
`viewRect`). **For a single-view editor this is correct** — switch from doc A to B and back and
A's cursor is exactly where you left it, for free.

It also explains the Phase 1 leftovers: the `EditController` is hollow (its `OnAction` is a pure
passthrough `return model->OnAction(...)`, `EditController.cpp:187-194`) and is *stored in
`Workspace::Node`* (`Workspace.h:315-316`), fetched back out by the view via
`node->GetController()` (`EditorView.cpp:53,87`). The controller sits in the node only because
the node is the per-document home — same pressure that put the cursor in the Document.

## Where it breaks — the future case inverts the cardinality

The wanted feature is **M views : 1 buffer** (two views onto the same file, independent
scrolling). That needs per-**view** state — but the cursor is stored per-**document**. Two views
of one buffer would fight over one cursor. Today's "make a whole new Document for independent
scroll" workaround is the *one-object-too-many* that flagged this as wrong. The contradiction is
real: per-document persistence and per-view independence are two different cardinalities that one
storage location cannot serve.

## Target — pull two state-bundles out of `Document` into their own classes

`Document` today is a fat object that fuses three different things. Phase 2 separates the two
state-bundles into their own classes and — **for now** — has `Document` hold a *reference* to
each. Where they ultimately live (and how multi-view shares them) is deliberately left open; the
near-term move is only "separate classes, referenced from `Document`".

| Concept | Contains | Cardinality (eventual / likely) | For now |
|---|---|---|---|
| **`TextBuffer`** | the text itself | 1 per file | have it |
| **`Document`** | path/identity, language, dirty/readonly; **references** a `ViewState` + an `EditState` | 1 per open file | Workspace open-list; owns the two refs |
| **`ViewState`** *(new class)* | `lineCursor` (cursor), `currentSelection` | per **view** | one instance, referenced by `Document` |
| **`EditState`** *(new class)* | `UndoHistory` (the history buffer) — that is all it is today | per **document / buffer** (shared by all views of it) | one instance, referenced by `Document` |

**Why two classes and not one:** their cardinalities differ, and that difference *is* the reason
to split them. Two views onto one buffer should share **one** undo history (`EditState`) but keep
**independent** cursors/selections (`ViewState`). Fusing them into `Document` (or into each other)
is exactly what makes the multi-view case feel impossible. Separated and referenced, the eventual
move is just "where does each ref point" — not a redesign.

> Note: the other view-ish members still in `Document` — `verticalNavigationViewModel`, `viewRect`,
> `wantedColumn`, the search-highlight index — are *candidates* to migrate into `ViewState` later.
> Per the scope of this phase, only cursor + selection move now; the rest is classified when the
> multi-view work is actually tackled.

### How this dissolves both scenarios

- **Multi-view, same buffer:** each view gets its own `ViewState` (independent cursor/selection)
  while all views share the *one* `EditState` (undo) and the one `TextBuffer` on the same
  `Document`. The extra per-view object is the small `ViewState`, not a whole Document — the "one
  too many" disappears.
- **Reopen restores cursor *and* undo (a wanted feature):** because `ViewState` and `EditState` are
  their own classes rather than buried in `Document`, both are serializable units. Reopening a
  folder/project can restore each document's cursor + selection (`ViewState`) **and** its undo
  history (`EditState`) — not just the cursor. The separation is what makes "restore where I was
  *and* my undo stack" a clean feature instead of a Document-internals dump. (Persisting to a
  session/workspace file is a later task; the classes are the prerequisite.)

### Attach/Detach is smart — and already happening unnamed

The single view *already* attaches/detaches documents: `document = GetActiveDocument()` on every
switch **is** attach/detach, just unnamed and not carrying state cleanly. Phase 2 makes it
explicit. On **attach**, the view/controller binds to a `Document` (shared `TextBuffer` +
`EditState`) and a `ViewState` (this view's cursor/selection). Exactly who hands out the
`ViewState` on attach — the Document's own, or one the view keeps per document — is the part left
to mull over; for now the Document references a single `ViewState`, so attach just uses that.

### This is the proven shape

It is the **buffer/window split** every serious editor lands on: Emacs `buffer` vs `window`
(buffer also keeps a `point` = the snapshot); VSCode `TextModel` vs per-editor serializable
`viewState`; Vim `buffer` vs `window` plus the `'"` mark persisted for reopen.

## Ordered steps — each compiles, keeps the verified-green set green

- **Step P2.0 — Pin the contract.** Add `utests` cases asserting what must survive: switch away
  from a document and back -> cursor/selection **and** undo history preserved (the single-view
  behavior we must *not* regress).
- **Step P2.1 — Extract `ViewState`.** Pull `lineCursor` (cursor) and `currentSelection` out of
  `Document` into a `ViewState` class; `Document` holds a `ViewState::Ref` (referenced, not fused).
  The editing ops in `Document` operate through the ref (`viewState->lineCursor`). Behavior
  unchanged — purely drawing the class boundary. Green.
- **Step P2.2 — Extract `EditState`.** Pull `UndoHistory historyBuffer` out of `Document` into an
  `EditState` class (today it *is* just the history buffer); `Document` holds an `EditState::Ref`.
  `BeginUndoItem`/`EndUndoItem`/`Undo` route through the ref. Behavior unchanged. Green.
- **Step P2.3 — Controller becomes a view member.** Make `EditController` a value/`unique_ptr`
  member of `EditorView` (like `TerminalController` in `TerminalView`), holding a **non-owning**
  (borrowed) document handle. Add `EditController::Attach(Document::Ref)` / `Detach()`. Delete
  `Node::controller` / `SetController` / `GetController` (`Workspace.h:209-214,315`) and stop
  creating the controller in `EnsureDocumentForNode` (`Workspace.cpp:212-216`). `EditorView`
  re-points its controller on document switch instead of `node->GetController()`. Green.

After P2.1–P2.3 the seam is in place: two cleanly-separated state classes referenced by `Document`,
and the controller owned by the view. Where the refs eventually point for multi-view is then a
localized change, not a redesign.

## Deferred — to mull over (explicitly NOT decided here)

- **Where `ViewState` / `EditState` ultimately live and how multi-view shares them.** Likely shape:
  `ViewState` becomes per-view (each view keyed by document), `EditState` stays per-document (shared
  by all views of that buffer). But the ownership/keying (view-owns-`map<Document*,ViewState>` vs
  Document-hands-out vs a separate session object) is left open by design. P2.1–P2.3 only draw the
  class boundaries; they do not force this.
- **Restore-on-reopen feature (wanted).** Persist each document's `ViewState` (cursor + selection)
  **and** `EditState` (undo history) to a session/workspace file and restore them when the
  folder/project is reopened. The class extraction in P2.1/P2.2 is the prerequisite; the
  serialization + reopen wiring is its own later task.

## Phase 2 — out of scope

- Multi-view *implementation* (actual split-of-one-buffer UI). Only the seam (P2.1–P2.3) is built.
- Cross-session persistence (restore-on-reopen above): the classes land in-memory on `Document`;
  serializing them to disk and restoring on folder-reopen is a separate later task.
- `EditController` dissolution (still the separately-decided later task; P2.3 only *relocates* the
  controller, it does not fold it into the Document).

## Out of scope (Phase 1)

- Folder-monitor *implementation* (only the seam is built).
- The `EditController` / `EditorModel` editing-logic boundary (kept as-is; Phase 1 is about
  ownership and tree-coupling, not the edit pipeline). Phase 2 relocates the controller but still
  leaves the edit-pipeline split for the dissolution task.
