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

## Out of scope

- Folder-monitor *implementation* (only the seam is built).
- The `EditController` / `EditorModel` editing-logic boundary (kept as-is; this refactor is
  about ownership and tree-coupling, not the edit pipeline).
