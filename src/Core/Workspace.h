//
// Created by gnilk on 09.05.23.
//

#ifndef EDITOR_WORKSPACE_H
#define EDITOR_WORKSPACE_H

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <filesystem>
#include <vector>
#include <functional>
#include <set>

#include "logger.h"

#include "Core/FolderMonitor.h"
#include "Core/FileSystem/FsFilter.h"
#include "Core/PathUtil.h"
#include "Core/Config/ConfigNode.h"
#include "Document.h"
#include "Core/Session/SessionState.h"
#include "Core/UnicodeHelper.h"

namespace gedit {


    //
    // The Workspace is the single owner of two things: the open documents (the "tabs") and the
    // browseable ProjectRoots (top-level folders). A ProjectRoot owns a Node tree; nodes are
    // path-only until opened, when a Document is built lazily for them.
    //
    class Workspace {
    public:
        using Ref = std::shared_ptr<Workspace>;
        using ContentsChangedDelegate = std::function<void()>;
        using MonitorFactory = std::function<FolderMonitor::MonitorPoint::Ref (const std::filesystem::path &rootPath)>;
        // Node define the structure of a workspace
    public:

        //
        // A workspace is made up by several root nodes which are the respective work-areas
        // If 'OpenFolder' is used a work-area is created where nodes map directly to directories and files
        //
    class Node : public std::enable_shared_from_this<Node> {
        public:
            inline static const std::string kMetaKey_NodeType = "type";
            inline static const std::string kMetaKey_FileSize = "filesize";
            inline static const std::string kMetaKey_ReadOnly = "readonly";
            // Gone from disk while an open document still lives at or below it (kept, not removed)
            inline static const std::string kMetaKey_MissingOnDisk = "missing_on_disk";
            // A new buffer ('.nb') whose file hasn't been written yet - not on disk, but not deleted either
            inline static const std::string kMetaKey_NewFile = "new_file";

            enum NodeType : int {
                kNodeVirtual = 0,   // Virtual nodes are like the 'Default' node - it doesn't exists, used only for grouping
                kNodeInternal = 1,  // Internal, not used
                kNodeHidden = 2,    // Hidden, not used
                kNodeFolder = 3,    // Folder - this node is a directory
                kNodeFileRef = 4,   // FileRef - this node references a file
            };
            using Ref = std::shared_ptr<Node>;
            using ChildNodesValueType = std::vector<Node::Ref>::value_type;
        public:
            // A node has a name and generally points to a directory - but we can create nodes a bit how we want..
            // Thus we can mimic VStudio with "Source", "Headers", etc.. which are virtual nodes but appear as
            // directories...
            explicit Node(const std::string &nodeName) : displayName(nodeName) {

            }
            virtual ~Node() {
                // This is just here for debugging purposes...
                // printf("Workspace::Node::DTOR\n");
            }
            static Ref Create(const std::string &nodeName) {
                return std::make_shared<Node>(nodeName);
            }
            static Ref Create(Document::Ref document) {
                auto node = std::make_shared<Node>("");
                node->document = document;
                return node;
            }

            const std::string &GetDisplayName() {
                return displayName;
            }
            // TODO: Settle for one..
            const std::u32string GetDisplayNameU32() {
                std::u32string u32name;
                if (!UnicodeHelper::ConvertUTF8ToUTF32String(u32name, displayName)) {
                    return U"INVALID UTF8";
                }
                return u32name;
            }

            // Set the display name of an item - this is the name used in the UI
            // for a file - it's the filename, for a folder it is the last item of the full path...
            void SetDisplayName(const std::string &newDisplayName) {
                displayName = newDisplayName;

            }

            void SetParent(Node::Ref newParent) {
                // Do NOT support re-parenting
                if (parent != nullptr) {
                    return;
                }
                parent = newParent;
            }

            Node::Ref GetParent() {
                return parent;
            }

            size_t FlattenChilds(std::vector<Node::Ref> &outNodes) {
                for(auto value : childNodes) {
                    outNodes.push_back(value);
                }
                return outNodes.size();
            }

            Node::Ref AddChild(const std::string &childDisplayName) {
                auto child = Node::Create(childDisplayName);
                child->parent = shared_from_this();
                childNodes.push_back(child);
                return child;
            }

            bool DelChild(const Node::Ref child) {
                if (!HasChild(child)) {
                    return false;
                }
                auto itErase = std::find(childNodes.begin(), childNodes.end(), child);
                if (itErase == childNodes.end()) {
                    return false;
                }
                childNodes.erase(itErase);
                return true;
            }

            bool HasChild(const Node::Ref node) {
                auto itFound = std::find(childNodes.begin(), childNodes.end(), node);
                if (itFound == childNodes.end()) {
                    return false;
                }
                return true;
            }

            // Remove all children (used by ScanNode to clear before re-scanning). Does NOT
            // reset the children's parent pointer — callers ensure the cleared nodes are
            // unreachable (ScanNode only calls this on unscanned frontier folders whose
            // children have never been opened / referenced from outside the tree).
            void ClearChildren() {
                childNodes.clear();
            }

            // Search recursively for a node with a specific editor-document attached
            Node::Ref FindDocument(const Document::Ref searchDocument) {
                if (document == searchDocument) {
                    return shared_from_this();
                }

                for(auto &node : childNodes) {
                    auto nodeForDocument = node->FindDocument(searchDocument);
                    if (nodeForDocument != nullptr) {
                        return nodeForDocument;
                    }
                    if (node->GetDocument() == searchDocument) return node;
                }
                return nullptr;
            }

            // Search recursively for a node with a specific path. Compares lexically-normalized form so
            // a query path picks up redundant "." segments (e.g. an `OpenFolder(".")` root carries them
            // in every scanned child's path; an absolute query path built elsewhere normally doesn't).
            Node::Ref FindNodeWithPath(const std::filesystem::path path) {
                if (path.lexically_normal() == pathName.lexically_normal()) {
                    return shared_from_this();
                }
                for(auto &child : childNodes) {
                    auto res = child->FindNodeWithPath(path);
                    if (res != nullptr) {
                        return res;
                    }
                }
                return nullptr;
            }

            // Returns the absolute path for this node
            std::filesystem::path GetNodePath() {
                return pathName;
            }

            // Set's the node-path AND updates the display name from it...
            void SetNodePath(std::filesystem::path newPath) {
                pathName = newPath;
                isPathNameChanged = true;
                UpdateDisplayNameFromPath();
                // Keep the attached document's file identity in sync with the node path.
                if (document != nullptr) {
                    document->SetPath(newPath);
                }
            }

            // Checks if the node-path is pointing to a folder
            bool IsFolder() {
                // This is a workspace root folder...
                if (parent == nullptr) {
                    return true;
                }
                // The type recorded when the node was made - a folder gone from disk (kept for an open
                // document below it) is still a folder
                if (HasMeta(kMetaKey_NodeType)) {
                    return metaData.GetValue<int>(kMetaKey_NodeType, kNodeFolder) == kNodeFolder;
                }
                if (std::filesystem::is_directory(pathName)) {
                    return true;
                }
                return false;
            }


            size_t GetNumChildNodes() {
                return childNodes.size();
            }

            void SetDocument(Document::Ref newDocument) {
                document = newDocument;
                // A document attached to a node adopts the node's path as its file identity. The node
                // path is generally set first (lazy nodes exist before their document), so sync here too.
                if (document != nullptr) {
                    document->SetPath(pathName);
                }
            }

            Document::Ref GetDocument() {
                return document;
            }

            // Convenient function to retrieve the textbuffer of the underlying document (if any)
            TextBuffer::Ref GetTextBuffer() {
                if (document == nullptr) {
                    return nullptr;
                }
                return document->GetTextBuffer();
            }

            // This will flatten the workspace and return a copy of all document references
            std::vector<Document::Ref> GetDocuments() {
                std::vector<Document::Ref> allDocuments;
                RecursiveGetDocuments(allDocuments);
                return allDocuments;
            }

            // Meta-Data, each node can have meta data attached - this is just a way to cache certain information
            // like 'type'/'size' (files)/etc...
            template<typename T>
            void SetMeta(const std::string &keyName, const T &data) {
                metaData.SetValue<T>(keyName, data);
            }

            bool HasMeta(const std::string &keyName) {
                if (!metaData.HasKey(keyName)) {
                    return false;
                }
                return true;
            }

            template<typename T>
            auto GetMeta(const std::string &keyName, const T &defValue) {
                return metaData.GetValue(keyName, defValue);
            }

            bool IsMissingOnDisk() {
                return GetMeta<bool>(kMetaKey_MissingOnDisk, false);
            }

            void SetMissingOnDisk(bool isMissing) {
                SetMeta<bool>(kMetaKey_MissingOnDisk, isMissing);
            }

            bool IsNewFile() {
                return GetMeta<bool>(kMetaKey_NewFile, false);
            }

            void SetNewFile(bool isNew) {
                SetMeta<bool>(kMetaKey_NewFile, isNew);
            }

            // Load data associated with the workspace node, this is here as we OWN the filename
            bool LoadData() {
                if (document == nullptr) {
                    return false;
                }
                // Does this file exists - or is it a 'new' file
                // In case this is a new file (buffer has state 'kBuffer_FileRef') the LoadData shouldn't fail if the file
                // doesn't exists - this is explicit...  one could argue a return enum would be better, but the upper layer
                // has little need of that information...
                if (!std::filesystem::exists(pathName)) {
                    return true;
                }
                return document->LoadData(pathName);
            }

            // save data associated with the workspace node, this is here as we OWN the filename
            bool SaveData() {
                if (document == nullptr) {
                    return false;
                }
                bool result = false;
                if (IsMissingOnDisk()) {
                    // Deleted behind our back - re-create it (and its folders), even when unchanged
                    std::error_code ec;
                    std::filesystem::create_directories(pathName.parent_path(), ec);
                    result = document->SaveDataNoChangeCheck(pathName);
                    if (result) {
                        ClearMissingOnDiskUpwards();
                    }
                } else if (isPathNameChanged) {
                    result = document->SaveDataNoChangeCheck(pathName);
                    isPathNameChanged = false;
                } else {
                    result = document->SaveData(pathName);
                }
                if (result && IsNewFile() && std::filesystem::exists(pathName)) {
                    SetNewFile(false);
                }

                return result;
            }

        private:
            // This node and the folders above it are back on disk
            void ClearMissingOnDiskUpwards() {
                for (auto node = shared_from_this(); node != nullptr; node = node->GetParent()) {
                    if (!node->IsMissingOnDisk()) {
                        break;
                    }
                    node->SetMissingOnDisk(false);
                }
            }

            void UpdateDisplayNameFromPath() {
                displayName = pathutil::LastNameOfPath(pathName);
            }

            void RecursiveGetDocuments(std::vector<Document::Ref> &outDocuments) {
                if (document != nullptr) {
                    outDocuments.push_back(document);
                    return;
                }
                for(auto &node : childNodes) {
                    node->RecursiveGetDocuments(outDocuments);
                }
            }

        public:
            // FS-6: true = the scanner descended into this folder (or it was never scanned, in
            // which case the default keeps it as a non-expandable leaf). false = the scanner emitted
            // it but did not descend (depth bound / non-followed symlink / consumer veto) — the
            // view will offer a '+' and trigger ScanNode on expand. Record only; never a gate.
            bool IsScanned() const { return isScanned; }
            void SetIsScanned(bool value) { isScanned = value; }

        private:
            ConfigNode metaData;
            bool isPathNameChanged = false;
            bool isScanned = true;   // FS-6: set by onLeaveDir adapter (W2); read by FillTreeView (W5)
            std::string displayName = "";
            std::filesystem::path pathName;
            Node::Ref parent = nullptr;
            Document::Ref document = nullptr;   // This is only set for leaf nodes..
            std::vector<Node::Ref> childNodes = {};
        };

        //
        // A ProjectRoot is one top-level browseable folder in the workspace - what you perceive as a
        // top-level entry in the file browser. It owns the Node tree for that folder and the folder monitor
        // that keeps the tree in sync with the filesystem. A node can't reference an item outside its
        // ProjectRoot folder. In essence:
        //   Workspace -> ProjectRoot [1..n] -> Node tree (folder/file tree)
        //
        // In the editor the 'WorkspaceView' is the graphical representation of these classes.
        //
        // The monitor reports dirty FOLDERS on the main thread (FlushDirty); each one that is in the tree and
        // scanned is reconciled with disk through the Workspace (which owns node mutation), then the
        // Workspace is told once that the tree changed. See docs/foldermonitor_fixes.md §2.
        //
        class ProjectRoot : public std::enable_shared_from_this<ProjectRoot> {
        public:
            using Ref = std::shared_ptr<ProjectRoot>;
            // Bring one folder's children in line with disk, without notifying - true if the tree changed
            using ReconcileDelegate = std::function<bool (Node::Ref folder)>;
            // The tree changed (called once per flush)
            using ChangedDelegate = std::function<void ()>;
        public:
            ProjectRoot(ReconcileDelegate reconcileHandler, ChangedDelegate changedHandler,
                    const std::filesystem::path path,
                    const std::string &rootName) : name(rootName),rootPath(path), funcReconcile(reconcileHandler), funcChanged(changedHandler){

                rootNode = Node::Create(rootName);
                rootNode->SetNodePath(rootPath);
            }
            virtual ~ProjectRoot() {
                if (monitor != nullptr) {
                    monitor->Stop();
                }
            }

            static Ref Create(ReconcileDelegate reconcileHandler,
                              ChangedDelegate changedHandler,
                              const std::filesystem::path path, const std::string &rootName) {

                auto logger = gnilk::Logger::GetLogger("Workspace");
                logger->Debug("ProjectRoot '%s' created @ cwd: %s", rootName.c_str(), path.c_str());

                auto ref = std::make_shared<ProjectRoot>(reconcileHandler, changedHandler, path, rootName);

                return ref;
            }

            const std::string &GetName() {
                return name;
            }

            const std::filesystem::path &GetRootPath() {
                return rootPath;
            }

            Node::Ref GetRootNode() {
                return rootNode;
            }

            // The scan's exclude patterns (workspace.exclude) - handed to the monitor, which drops events below a match
            void SetExcludeFilter(const FsFilter &newExcludeFilter) {
                excludeFilter = newExcludeFilter;
            }

            bool HasFolderMonitor() {
                return (monitor != nullptr);
            }

            FolderMonitor::MonitorPoint::Ref GetFolderMonitor() {
                return monitor;
            }

            // Takes ownership of 'newMonitor' and starts it. Dirty folders come back through 'postToMain'.
            bool StartFolderMonitor(FolderMonitor::MonitorPoint::Ref newMonitor, FolderMonitor::MonitorPoint::PostDelegate postToMain) {
                if ((newMonitor == nullptr) || (funcReconcile == nullptr)) {
                    return false;
                }
                if (monitor != nullptr) {
                    monitor->Stop();
                }
                monitor = newMonitor;
                monitor->SetExcludeFilter(excludeFilter);
                monitor->SetPostToMain(postToMain);
                // The monitor can outlive us by a queued flush
                std::weak_ptr<ProjectRoot> weakThis = weak_from_this();
                monitor->SetDirtyHandler([weakThis](const std::set<std::filesystem::path> &dirtyFolders, bool isLostEvents) {
                    if (auto self = weakThis.lock()) {
                        self->FlushDirty(dirtyFolders, isLostEvents);
                    }
                });
                return monitor->Start();
            }

            // Main thread: reconcile the dirty folders that are in the tree and scanned - an unscanned frontier
            // is read when expanded, a folder not in the tree is below one or excluded. The set is ordered, so
            // a parent comes before its children (its reconcile may drop a child folder). Lost events ->
            // every scanned folder. One change notification for the lot.
            void FlushDirty(const std::set<std::filesystem::path> &dirtyFolders, bool isLostEvents) {
                if (funcReconcile == nullptr) {
                    return;
                }
                bool isChanged = false;
                if (isLostEvents) {
                    isChanged = ReconcileScanned(rootNode);
                } else {
                    for (auto &folder : dirtyFolders) {
                        auto node = rootNode->FindNodeWithPath(folder);
                        if ((node == nullptr) || !node->IsFolder() || !node->IsScanned()) {
                            continue;
                        }
                        isChanged |= funcReconcile(node);
                    }
                }
                if (isChanged && (funcChanged != nullptr)) {
                    funcChanged();
                }
            }

        protected:
            // Reconcile 'folder' and every scanned folder below it, one level each
            bool ReconcileScanned(Node::Ref folder) {
                if (!folder->IsScanned()) {
                    return false;
                }
                bool isChanged = funcReconcile(folder);
                std::vector<Node::Ref> children;
                folder->FlattenChilds(children);
                for (auto &child : children) {
                    // A folder kept for an open document below it is already known to be gone
                    if (child->IsFolder() && !child->IsMissingOnDisk()) {
                        isChanged |= ReconcileScanned(child);
                    }
                }
                return isChanged;
            }

        private:
            ProjectRoot() = default;

        private:
            std::string name = {};
            std::filesystem::path rootPath = {};
            ReconcileDelegate funcReconcile = nullptr;
            ChangedDelegate funcChanged = nullptr;
            Node::Ref rootNode = {};
            FolderMonitor::MonitorPoint::Ref monitor = {};
            FsFilter excludeFilter = {};
        };

    public:
        Workspace();
        virtual ~Workspace();

        static Ref Create();
        // The default ProjectRoot is the CWD-based root used for new/loose files. Returns its root node.
        const Workspace::Node::Ref GetDefaultWorkspace();

        void SetChangeDelegate(ContentsChangedDelegate newChangeHandler) {
            onChangeHandler = newChangeHandler;
        }

        // Folder-monitor glue, set by the Editor: 'factory' makes a monitor for a root folder (nullptr when
        // monitoring is off), 'postToMain' runs its flushes on the main thread. Unset -> OpenFolder doesn't
        // monitor (tests, tools).
        void SetFolderMonitor(MonitorFactory factory, FolderMonitor::MonitorPoint::PostDelegate postToMain) {
            monitorFactory = std::move(factory);
            monitorPostToMain = std::move(postToMain);
        }

        const std::vector<Workspace::ProjectRoot::Ref> &GetProjectRoots() {
            return projectRoots;
        }

        Node::Ref GetActiveFolderNode() {
            return activeFolderNode;
        }
        // The workspace's current folder (what the workspace view points at) - NOT the process CWD. Given a
        // file node, its parent folder becomes active.
        void SetActiveFolderNode(Node::Ref newActiveFolder) {
            if ((newActiveFolder != nullptr) && (newActiveFolder->GetParent() != nullptr) &&
                (newActiveFolder->GetMeta<int>(Node::kMetaKey_NodeType, Node::kNodeFolder) != Node::kNodeFolder)) {
                newActiveFolder = newActiveFolder->GetParent();
            }
            activeFolderNode = newActiveFolder;
        }

        bool OpenFolder(const std::string &folder);

        // FS-6: on-demand shallow scan for a frontier folder (isScanned==false). Clears existing
        // children, runs a maxDepth=1 scan rooted at node's path, then sets node->isScanned=true.
        // Idempotent (clear+rescan). Never reads isScanned — the flag is an output, not a gate.
        void ScanNode(Node::Ref node);

        Node::Ref NewDocument(const std::string &name);                       // Adds an empty document/file to the default workspace
        Node::Ref NewDocument(const Node::Ref parent, const std::string &name); // Adds an empty document/file to a specific workspace

        // Adds a file-reference (i.e. doesn't load contents) to the default workspace
        Node::Ref NewDocumentWithFileRef(const std::filesystem::path &pathFileName);
        // Adds a file-reference (i.e doesn't load contents) to a specific (named) workedspace
        Node::Ref NewDocumentWithFileRef(Node::Ref parent, const std::filesystem::path &pathFileName);

        Node::Ref GetNodeFromDocument(Document::Ref document);

        // Lazily create (if needed) and return the document for a file node. Folder nodes return null.
        // The browse tree stores path-only file nodes; the document is built the first time a node is
        // opened. Returns the existing document if the node already has one.
        Document::Ref EnsureDocumentForNode(Node::Ref node);

        bool RemoveDocument(Document::Ref document);

        //
        // Open documents - the open "tabs". The Workspace is the single owner of the open-document
        // list and the active document; Editor and the views query through here. These are pure data
        // operations (no UI side effects); Editor layers the redraw/relayout on top.
        //
        const std::vector<Document::Ref> &GetOpenDocuments() {
            return openDocuments;
        }
        void AddOpenDocument(Document::Ref document);
        bool RemoveOpenDocument(Document::Ref document);
        // Drop a FILE node whose file doesn't exist on disk (a new buffer never saved) - for after its
        // document is closed. False when the node is a folder or its file exists.
        bool RemoveNodeIfNotOnDisk(Node::Ref node);
        // A node's file/folder disappeared from disk (deleted/moved behind our back). Removed from the tree -
        // unless an OPEN document lives at or below it: that node (and the folders leading to it) stays,
        // flagged kMetaKey_MissingOnDisk, and the document stays open with its edits. Saving it re-creates
        // the file. Never closes a document. True when the node was removed.
        bool RemoveNodeGoneFromDisk(Node::Ref node);
        // Bring a folder node's direct children in line with disk, one level (workspace.exclude applies):
        // entries new on disk are added (a new folder arrives unscanned - a frontier), children gone from
        // disk go as in RemoveNodeGoneFromDisk, and everything else is left alone - nodes, documents,
        // scanned state. A folder that is itself gone is handled the same way. Notifies once; true when
        // the tree changed. This is the folder monitor's response to "something changed in this folder".
        bool ReconcileFolder(Node::Ref node);
        bool IsDocumentOpen(Document::Ref document);

        Document::Ref GetActiveDocument() {
            return activeDocument;
        }
        void SetActiveDocument(Document::Ref document);   // ignored if the document isn't open
        size_t GetActiveDocumentIndex();
        Document::Ref GetDocumentFromIndex(size_t idxDocument);
        Document::Ref GetDocumentFromTextBuffer(TextBuffer::Ref textBuffer);
        size_t NextDocumentIndex(size_t idxCurrent);
        size_t PreviousDocumentIndex(size_t idxCurrent);

        // Session serialisation (docs/session-cache.md §11.3). Workspace owns the open-document list and
        // the active doc, so it enumerates those; layout/geometry/tree-state are filled by their owners.
        void ToSession(RootSession &session);
        // Reopen each saved document (skipping any whose file is gone) and restore the active one by index.
        void FromSession(const RootSession &session);
        // Reopen one saved document: load its file into the workspace and restore its view-state. Returns
        // nullptr (entry skipped) if the file is missing/stale - never aborts the whole restore.
        Document::Ref ReopenDocument(const DocumentSession &docSession);

    protected:
        Node::Ref NewFolderNode(Node::Ref parent, const std::filesystem::path &pathName);
        // Adds a path-only file node (no document) under parent. The document is created lazily on open.
        Node::Ref AddFileNode(Node::Ref parent, const std::filesystem::path &pathName);
        // THE single filesystem->tree mutator: maps one fs entry (file or dir) to a node under parent.
        // Shared by the initial folder Scan (ReadFolderToNode) and (later) the live folder monitor.
        Node::Ref ApplyFsEntry(Node::Ref parent, const std::filesystem::path &path);
        // Search every ProjectRoot's tree for a node already covering this path (e.g. one scanned by
        // OpenFolder, or already open under a different root). Path is resolved to absolute before the
        // search since callers may pass either form.
        Node::Ref FindNodeForPath(const std::filesystem::path &path);
        // open-bugs.md #8: a file BELOW the initial max_scan_depth frontier (e.g. a restored session
        // file) isn't in any tree. If it lies under a real ProjectRoot, scan ONLY the path to it into
        // that root (full listing per level, descend toward the file) and return its node; nullptr if
        // the path isn't under any real root (caller then falls back to the default root).
        Node::Ref EnsurePathInTree(const std::filesystem::path &pathFileName);
        // The folder node for an absolute dir path below a project root - scans in / creates (on disk and
        // in the tree) the folders on the way. nullptr when no root owns the path.
        Node::Ref EnsureFolderNode(const std::filesystem::path &dirPath);
        // Reference-guided shallow scan from `startNode` (a frontier folder) toward refPath, via the
        // same ApplyFsEntry seam / parent-stack adapter as ReadFolderToNode and ScanNode.
        void ScanPathToReference(Node::Ref startNode, const std::filesystem::path &refPath);

        ProjectRoot::Ref GetDefaultRoot();   // creates the CWD-based default root if needed

        bool RemoveNode(Node::Ref node);
        // RemoveNodeGoneFromDisk's walk: drops the children with no open document below them, flags the
        // rest. True when 'node' must stay (an open document lives at or below it).
        bool PruneGoneFromDisk(Node::Ref node);
        // ReconcileFolder without the notification (ScanNode is called from inside the view's expand)
        bool DoReconcileFolder(Node::Ref node);
        bool ReadFolderToNode(Node::Ref rootNode, const std::filesystem::path &folder);
        void UpdateMetaDataForNode(Node::Ref node);
        ProjectRoot::Ref GetOrAddProjectRoot(const std::filesystem::path &rootPath, const std::string &rootName);
        void StartFolderMonitor(ProjectRoot::Ref projectRoot);
        void DisableNotifications() {
            isChangeHandlerDisabled++;
        }
        void EnableNotifications() {
            isChangeHandlerDisabled--;
            if (isChangeHandlerDisabled < 0) {
                isChangeHandlerDisabled = 0;
            }
        }
        void NotifyChangeHandler() {
            if ((onChangeHandler != nullptr) && (isChangeHandlerDisabled == 0)) {
                onChangeHandler();
            }
        }

    private:
        gnilk::ILogger *logger = nullptr;

        int isChangeHandlerDisabled = 0;

        ContentsChangedDelegate onChangeHandler = {};
        MonitorFactory monitorFactory = {};
        FolderMonitor::MonitorPoint::PostDelegate monitorPostToMain = {};

        Node::Ref activeFolderNode = nullptr;

        // The browseable top-level folders. defaultRoot (the CWD-based root for new/loose files) is
        // also a member of this list.
        std::vector<ProjectRoot::Ref> projectRoots = {};
        ProjectRoot::Ref defaultRoot = nullptr;

        // The open documents (tabs) and the currently active one. Single source of truth.
        std::vector<Document::Ref> openDocuments = {};
        Document::Ref activeDocument = nullptr;

    };
}


#endif //EDITOR_WORKSPACE_H
