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

#include "logger.h"

#include "Core/RuntimeConfig.h"
#include "Core/FolderMonitor.h"
#include "Core/PathUtil.h"
#include "Core/Config/ConfigNode.h"
#include "Document.h"
#include "Core/UnicodeHelper.h"
#include "Core/Controllers/EditController.h"

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
            static Ref Create(Document::Ref editorModel) {
                auto node = std::make_shared<Node>("");
                node->model = editorModel;
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

            // Search recursively for a node with a specific editor-model attached
            Node::Ref FindModel(const Document::Ref searchModel) {
                if (model == searchModel) {
                    return shared_from_this();
                }

                for(auto &node : childNodes) {
                    auto nodeForModel = node->FindModel(searchModel);
                    if (nodeForModel != nullptr) {
                        return nodeForModel;
                    }
                    if (node->GetModel() == searchModel) return node;
                }
                return nullptr;
            }

            // Search recursively for a node with a specific path...
            Node::Ref FindNodeWithPath(const std::filesystem::path path) {
                if (path == pathName) {
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
                // Keep the attached model's file identity in sync with the node path.
                if (model != nullptr) {
                    model->SetPath(newPath);
                }
            }

            // Checks if the node-path is pointing to a folder
            bool IsFolder() {
                // This is a workspace root folder...
                if (parent == nullptr) {
                    return true;
                }
                // Note: perhaps change to check meta...
                if (std::filesystem::is_directory(pathName)) {
                    return true;
                }
                return false;
            }


            size_t GetNumChildNodes() {
                return childNodes.size();
            }

            void SetController(EditController::Ref newController) {
                controller = newController;
            }
            EditController::Ref GetController() {
                return controller;
            }

            void SetModel(Document::Ref newModel) {
                model = newModel;
                // A model attached to a node adopts the node's path as its file identity. The node
                // path is generally set first (lazy nodes exist before their model), so sync here too.
                if (model != nullptr) {
                    model->SetPath(pathName);
                }
            }

            Document::Ref GetModel() {
                return model;
            }

            // Convenient function to retrieve the textbuffer of the underlying model (if any)
            TextBuffer::Ref GetTextBuffer() {
                if (model == nullptr) {
                    return nullptr;
                }
                return model->GetTextBuffer();
            }

            // This will flatten the workspace and return a copy of all model references
            std::vector<Document::Ref> GetModels() {
                std::vector<Document::Ref> allModels;
                RecursiveGetModels(allModels);
                return allModels;
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

            // Load data associated with the workspace node, this is here as we OWN the filename
            bool LoadData() {
                if (model == nullptr) {
                    return false;
                }
                // Does this file exists - or is it a 'new' file
                // In case this is a new file (buffer has state 'kBuffer_FileRef') the LoadData shouldn't fail if the file
                // doesn't exists - this is explicit...  one could argue a return enum would be better, but the upper layer
                // has little need of that information...
                if (!std::filesystem::exists(pathName)) {
                    return true;
                }
                return model->LoadData(pathName);
            }

            // save data associated with the workspace node, this is here as we OWN the filename
            bool SaveData() {
                if (model == nullptr) {
                    return false;
                }
                bool result = false;
                if (isPathNameChanged) {
                    result = model->SaveDataNoChangeCheck(pathName);
                    isPathNameChanged = false;
                } else {
                    result = model->SaveData(pathName);
                }

                return result;
            }

        private:
            void UpdateDisplayNameFromPath() {
                displayName = pathutil::LastNameOfPath(pathName);
            }

            void RecursiveGetModels(std::vector<Document::Ref> &outModels) {
                if (model != nullptr) {
                    outModels.push_back(model);
                    return;
                }
                for(auto &node : childNodes) {
                    node->RecursiveGetModels(outModels);
                }
            }

        private:
            ConfigNode metaData;
            bool isPathNameChanged = false;
            std::string displayName = "";
            std::filesystem::path pathName;
            Node::Ref parent = nullptr;
            EditController::Ref controller = nullptr;
            Document::Ref model = nullptr;   // This is only set for leaf nodes..
            std::vector<Node::Ref> childNodes = {};
        };

        //
        // A ProjectRoot is one top-level browseable folder in the workspace - what you perceive as a
        // top-level entry in the file browser. It owns the Node tree for that folder and (later) the
        // folder monitor that keeps the tree in sync with the filesystem. A node can't reference an
        // item outside its ProjectRoot folder. In essence:
        //   Workspace -> ProjectRoot [1..n] -> Node tree (folder/file tree)
        //
        // In the editor the 'WorkspaceView' is the graphical representation of these classes.
        //
        // NOTE: the folder monitor is currently broken/disabled (gated behind 'foldermonitor.enabled').
        // The seam is kept here for later: OnFsCreated/OnFsRemoved/OnFsChanged are the single entry
        // points the monitor calls, and they route node creation/removal back through the Workspace's
        // delegates (the same path the initial scan uses). Wiring it up later is config, not redesign.
        //
        class ProjectRoot {
        public:
            using Ref = std::shared_ptr<ProjectRoot>;
            // Node create/delete are delegated back to the Workspace (which owns node creation).
            using CreateNodeDelgate = std::function<Node::Ref (Node::Ref parent, const std::filesystem::path &path)>;
            using DeleteNodeDelgate = std::function<void (Node::Ref parent, const std::filesystem::path &path)>;
        public:
            ProjectRoot(CreateNodeDelgate createNodeHandler, DeleteNodeDelgate deleteNodeHandler,
                    const std::filesystem::path path,
                    const std::string &rootName) : name(rootName),rootPath(path), funcCreateNode(createNodeHandler), funcDeleteNode(deleteNodeHandler){

                rootNode = Node::Create(rootName);
                rootNode->SetNodePath(rootPath);
            }
            virtual ~ProjectRoot() = default;

            static Ref Create(CreateNodeDelgate createNodeHandler,
                              DeleteNodeDelgate deleteNodeHandler,
                              const std::filesystem::path path, const std::string &rootName) {

                auto logger = gnilk::Logger::GetLogger("Workspace");
                logger->Debug("ProjectRoot '%s' created @ cwd: %s", rootName.c_str(), path.c_str());

                auto ref = std::make_shared<ProjectRoot>(createNodeHandler, deleteNodeHandler, path, rootName);

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

            bool StartFolderMonitor() {
                auto logger = gnilk::Logger::GetLogger("Workspace");
                if ((funcCreateNode == nullptr) || (funcDeleteNode == nullptr)) {
                    logger->Debug("no callbacks defined - if this is default all is fine");
                    return true;
                }

                // Need to stop first..
                if ((monitor != nullptr) && (monitor->IsRunning())) {
                    logger->Debug("FolderMonitor already started");
                    return true;
                }

                // Only create if needed
                if (monitor == nullptr) {
                    logger->Debug("FolderMonitor is null - creating with root: %s", rootPath.c_str());

                    auto &folderMonitor = RuntimeConfig::Instance().GetFolderMonitor();
                    monitor = folderMonitor.CreateMonitorPoint(rootPath, [this](const std::filesystem::path &path, FolderMonitor::kChangeFlags flags) -> void {
                        // FIXME: This won't work right now - IF we monitor the build folder...
                        OnMonitorEvent(path, flags);
                    });
                    // We can't start this
                    if (monitor == nullptr) {
                        return false;
                    }
                }

                std::filesystem::path gitIgnoreFile = rootPath / ".gitignore";
                if (exists(gitIgnoreFile)) {
                    logger->Debug("GitIgnore file found - we should read and add to exclude list");
                }

                return monitor->Start();
            }

        protected:
            // Any event from the FolderMonitor is decoded here and dispatched to the OnFs* seam.
            void OnMonitorEvent(const std::filesystem::path &path, FolderMonitor::kChangeFlags flags) {
                if ((flags & FolderMonitor::kChangeFlags::kCreated) && !(flags & FolderMonitor::kChangeFlags::kRemoved)) {
                    OnFsCreated(path);
                } else if (flags & FolderMonitor::kChangeFlags::kRemoved) {
                    OnFsRemoved(path);
                } else {
                    OnFsChanged(path);
                }
            }

            // --- Filesystem -> tree seam (single set of entry points for the monitor) ---

            // A filesystem entry appeared: add a node for it under its parent.
            Node::Ref OnFsCreated(const std::filesystem::path &path) {
                if (funcCreateNode == nullptr) {
                    return nullptr;
                }

                auto logger = gnilk::Logger::GetLogger("Workspace");

                auto parent = path.parent_path();
                auto parentNode = rootNode->FindNodeWithPath(parent);
                if (parentNode == nullptr) {
                    logger->Error("Unable to find node for path=%s", path.c_str());
                    return nullptr;
                }

                // The workspace will notify the 'view' on any changes -> cause a rebuild of the tree...
                return funcCreateNode(parentNode, path);
            }

            // A filesystem entry was removed: drop its node.
            void OnFsRemoved(const std::filesystem::path &path) {
                if (!funcDeleteNode) {
                    return;
                }
                auto node = rootNode->FindNodeWithPath(path);
                funcDeleteNode(node, path);
            }

            // A filesystem entry changed on disk. Placeholder for the (future) "reload/dirty on disk"
            // signal to an open Document; the tree structure itself doesn't change. No-op for now.
            void OnFsChanged(const std::filesystem::path &path) {
                (void)path;
            }

        private:
            ProjectRoot() = default;

        private:
            std::string name = {};
            std::filesystem::path rootPath = {};
            CreateNodeDelgate funcCreateNode = nullptr;
            DeleteNodeDelgate funcDeleteNode = nullptr;
            Node::Ref rootNode = {};
            FolderMonitor::MonitorPoint::Ref monitor = {};
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

        const std::vector<Workspace::ProjectRoot::Ref> &GetProjectRoots() {
            return projectRoots;
        }

        Node::Ref GetActiveFolderNode() {
            return activeFolderNode;
        }
        void SetActiveFolderNode(Node::Ref newActiveFolder) {
            activeFolderNode = newActiveFolder;
        }

        bool OpenFolder(const std::string &folder);

        Node::Ref NewModel(const std::string &name);                       // Adds an empty model/file to the default workspace
        Node::Ref NewModel(const Node::Ref parent, const std::string &name); // Adds an empty model/file to a specific workspace

        // Adds a file-reference (i.e. doesn't load contents) to the default workspace
        Node::Ref NewModelWithFileRef(const std::filesystem::path &pathFileName);
        // Adds a file-reference (i.e doesn't load contents) to a specific (named) workedspace
        Node::Ref NewModelWithFileRef(Node::Ref parent, const std::filesystem::path &pathFileName);

        Node::Ref GetNodeFromModel(Document::Ref model);

        // Lazily create (if needed) and return the model for a file node. Folder nodes return null.
        // The browse tree stores path-only file nodes; the model is built the first time a node is
        // opened. Returns the existing model if the node already has one.
        Document::Ref EnsureModelForNode(Node::Ref node);

        bool RemoveModel(Document::Ref model);

        //
        // Open documents - the open "tabs". The Workspace is the single owner of the open-model
        // list and the active model; Editor and the views query through here. These are pure data
        // operations (no UI side effects); Editor layers the redraw/relayout on top.
        //
        const std::vector<Document::Ref> &GetOpenModels() {
            return openModels;
        }
        void AddOpenModel(Document::Ref model);
        bool RemoveOpenModel(Document::Ref model);
        bool IsModelOpen(Document::Ref model);

        Document::Ref GetActiveModel() {
            return activeModel;
        }
        void SetActiveModel(Document::Ref model);   // ignored if the model isn't open
        size_t GetActiveModelIndex();
        Document::Ref GetModelFromIndex(size_t idxModel);
        Document::Ref GetModelFromTextBuffer(TextBuffer::Ref textBuffer);
        size_t NextModelIndex(size_t idxCurrent);
        size_t PreviousModelIndex(size_t idxCurrent);

    protected:
        Node::Ref NewFolderNode(Node::Ref parent, const std::filesystem::path &pathName);
        // Adds a path-only file node (no model) under parent. The model is created lazily on open.
        Node::Ref AddFileNode(Node::Ref parent, const std::filesystem::path &pathName);
        // THE single filesystem->tree mutator: maps one fs entry (file or dir) to a node under parent.
        // Shared by the initial folder Scan (ReadFolderToNode) and (later) the live folder monitor.
        Node::Ref ApplyFsEntry(Node::Ref parent, const std::filesystem::path &path);

        ProjectRoot::Ref GetDefaultRoot();   // creates the CWD-based default root if needed

        bool RemoveNode(Node::Ref node);
        bool ReadFolderToNode(Node::Ref rootNode, const std::filesystem::path &folder);
        void UpdateMetaDataForNode(Node::Ref node);
        ProjectRoot::Ref GetOrAddProjectRoot(const std::filesystem::path &rootPath, const std::string &rootName);
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
        //Document::Ref LoadEditorModelFromFile(const char *filename);

    private:
        gnilk::ILogger *logger = nullptr;

        int isChangeHandlerDisabled = 0;

        ContentsChangedDelegate onChangeHandler = {};

        Node::Ref activeFolderNode = nullptr;

        // The browseable top-level folders. defaultRoot (the CWD-based root for new/loose files) is
        // also a member of this list.
        std::vector<ProjectRoot::Ref> projectRoots = {};
        ProjectRoot::Ref defaultRoot = nullptr;

        // The open documents (tabs) and the currently active one. Single source of truth.
        std::vector<Document::Ref> openModels = {};
        Document::Ref activeModel = nullptr;

    };
}


#endif //EDITOR_WORKSPACE_H
