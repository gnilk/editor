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
#include "EditorModel.h"

namespace gedit {


    //
    // perhaps refactor this, workspace is everything - 'Desktops' (in lack of a better name) is a view/project or similar => rename to project??? (want to keep that name free for now)
    // This should probably be refactored - it is hairy...
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
            static Ref Create(EditorModel::Ref editorModel) {
                auto node = std::make_shared<Node>("");
                node->model = editorModel;
                return node;
            }

            const std::string &GetDisplayName() {
                return displayName;
            }
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

            Node::Ref AddChild(const std::string &displayName) {
                auto child = Node::Create(displayName);
                child->parent = shared_from_this();
                childNodes.push_back(child);
                // Resolve path??
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

            Node::Ref FindModel(const EditorModel::Ref searchModel) {
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

            std::filesystem::path GetNodePath() {
                return pathName;
            }

            void SetNodePath(std::filesystem::path newPath) {
                pathName = newPath;
                isPathNameChanged = true;
                UpdateDisplayNameFromPath();
            }

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


            void SetModel(EditorModel::Ref newModel) {
                model = newModel;
            }

            EditorModel::Ref GetModel() {
                return model;
            }
            TextBuffer::Ref GetTextBuffer() {
                if (model == nullptr) {
                    return nullptr;
                }
                return model->GetTextBuffer();
            }

            // This will flatten the workspace and return a copy of all model references
            std::vector<EditorModel::Ref> GetModels() {
                std::vector<EditorModel::Ref> allModels;
                RecursiveGetModels(allModels);
                return allModels;
            }

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

            bool LoadData() {
                if (model == nullptr) {
                    return false;
                }
                // Does this file exists - or is it a 'new' file
                if (!std::filesystem::exists(pathName)) {
                    return true;
                }
                return model->LoadData(pathName);
            }

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

            void RecursiveGetModels(std::vector<EditorModel::Ref> &outModels) {
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
            EditorModel::Ref model = nullptr;   // This is only set for leaf nodes..
            std::vector<Node::Ref> childNodes = {};
        };

        class Desktop {
        public:
            using Ref = std::shared_ptr<Desktop>;
            // Must be set by called
            using CreateNodeDelgate = std::function<Node::Ref (Node::Ref parent, const std::filesystem::path &path)>;
            using DeleteNodeDelgate = std::function<void (Node::Ref parent, const std::filesystem::path &path)>;
        public:
            Desktop(CreateNodeDelgate createNodeHandler, DeleteNodeDelgate deleteNodeHandler,
                    const std::filesystem::path path,
                    const std::string &desktopName) : name(desktopName),rootPath(path), funcCreateNode(createNodeHandler), funcDeleteNode(deleteNodeHandler){

                rootNode = Node::Create(desktopName);
            }
            virtual ~Desktop() = default;
            static Ref Create(CreateNodeDelgate createNodeHandler,
                              DeleteNodeDelgate deleteNodeHandler,
                              const std::filesystem::path path, const std::string &desktopName) {

                auto logger = gnilk::Logger::GetLogger("Workspace");
                logger->Debug("Desktop '%s' created @ cwd: %s", desktopName.c_str(), path.c_str());

                auto ref = std::make_shared<Desktop>(createNodeHandler, deleteNodeHandler, path, desktopName);

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

                // Need to stop first..
                if ((changeMonitor != nullptr) && (changeMonitor->IsRunning())) {
                    logger->Debug("FolderMonitor already started");
                    return true;
                }

                // Only create if needed
                if (changeMonitor == nullptr) {
                    logger->Debug("FolderMonitor is null - creating with root: %s", rootPath.c_str());

                    auto &folderMonitor = RuntimeConfig::Instance().GetFolderMonitor();
                    changeMonitor = folderMonitor.CreateMonitorPoint(rootPath, [this](const std::filesystem::path &path, FolderMonitor::kChangeFlags flags) -> void {
                        OnMonitorEvent(path, flags);
                    });
                }

                std::filesystem::path gitIgnoreFile = rootPath / ".gitignore";
                if (exists(gitIgnoreFile)) {
                    auto logger = gnilk::Logger::GetLogger("Workspace");
                    logger->Debug("GitIgnore file found - we should read and add to exclude list");
                }

                return changeMonitor->Start();
            }
        protected:

            void OnMonitorEvent(const std::filesystem::path &path, FolderMonitor::kChangeFlags flags) {
                auto logger = gnilk::Logger::GetLogger("Workspace");

                if ((flags & FolderMonitor::kChangeFlags::kCreated) && !(flags & FolderMonitor::kChangeFlags::kRemoved)) {
                    auto node = AddFromFileEvent(path);
                } else if (flags & FolderMonitor::kChangeFlags::kRemoved) {
                    DeleteFromFileEvent(path);
                }
            }
            // This function would benefit from being outside (i.e. in the workspace)
            Node::Ref AddFromFileEvent(const std::filesystem::path &path) {
                // We are most likely the default and some lousy developer started the folder monitor...
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

            void DeleteFromFileEvent(const std::filesystem::path &path) {
                if (!funcDeleteNode) {
                    return;
                }
                auto node = rootNode->FindNodeWithPath(path);
                funcDeleteNode(node, path);
            }

        private:
            Desktop() = default;

        private:
            std::string name = {};
            std::filesystem::path rootPath = {};
            CreateNodeDelgate funcCreateNode = nullptr;
            DeleteNodeDelgate funcDeleteNode = nullptr;
            Node::Ref rootNode = {};
            FolderMonitor::MonitorPoint::Ref changeMonitor = {};
        };

    public:
        Workspace();
        virtual ~Workspace();

        static Ref Create();
        const Workspace::Node::Ref GetDefaultWorkspace();
        const Workspace::Node::Ref GetNamedWorkspace(const std::string &name);

        void SetChangeDelegate(ContentsChangedDelegate newChangeHandler) {
            onChangeHandler = newChangeHandler;
        }

        const std::unordered_map<std::string, Workspace::Desktop::Ref> &GetDesktops() {
            return rootNodes;
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

        Node::Ref GetNodeFromModel(EditorModel::Ref model);

        bool RemoveModel(EditorModel::Ref model);

        void DumpToLog();

    protected:
        bool RemoveNode(Node::Ref node);
        bool ReadFolderToNode(Node::Ref rootNode, const std::filesystem::path &folder);
        void UpdateMetaDataForNode(Node::Ref node);
        Desktop::Ref GetOrAddDesktop(const std::filesystem::path &rootPath, const std::string &desktopName);
        void DisableNotifications() {
            isChangeHandlerEnabled = false;
        }
        void EnableNotifications() {
            isChangeHandlerEnabled = true;
        }
        void NotifyChangeHandler() {
            if ((onChangeHandler != nullptr) && (isChangeHandlerEnabled)) {
                onChangeHandler();
            }
        }
        //EditorModel::Ref LoadEditorModelFromFile(const char *filename);

    private:
        gnilk::ILogger *logger = nullptr;
        int newFileCounter = 0;

        bool isChangeHandlerEnabled = true;
        ContentsChangedDelegate onChangeHandler = {};

        Node::Ref activeFolderNode = nullptr;

        //std::unordered_map<std::string, Node::Ref> rootNodes = {};
        std::unordered_map<std::string, Desktop::Ref> rootNodes = {};

        std::vector<EditorModel::Ref> models;

    };
}


#endif //EDITOR_WORKSPACE_H
