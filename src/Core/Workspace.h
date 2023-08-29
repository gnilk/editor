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

#include "Core/Config/ConfigNode.h"
#include "EditorModel.h"

namespace gedit {
    //
    // perhaps refactor this, workspace is more or less the container for multiple workspaces
    // a folder (or root-node in the container) is a specific workspace...
    //
    class Workspace {
    public:
        using Ref = std::shared_ptr<Workspace>;
        using ContentsChangedDelegate = std::function<void()>;
        // Node define the structure of a workspace
    public:


        // This is the actual workspace...
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
            explicit Node(const std::string &nodeName) : name(nodeName), displayName(nodeName) {

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

            Node::Ref AddChild(const std::string &newName) {
                auto child = Node::Create(newName);
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

//            Node::Ref GetOrAddChild(const std::string &childName) {
//                if (HasChild(childName)) {
//                    return childNodes[childName];
//                }
//                return AddChild(childName);
//            }
//            std::optional<Node::Ref> GetChild(const std::string &childName) {
//                if (!HasChild(childName)) {
//                    return {};
//                }
//                return childNodes[childName];
//            }

            std::filesystem::path GetNodePath() {
                return pathName;
            }
            void SetNodePath(std::filesystem::path newPath) {
                pathName = newPath;
                UpdateDisplayNameFromPath();
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
                return model->LoadData(pathName);
            }
            bool SaveData() {
                if (model == nullptr) {
                    return false;
                }
                return model->SaveData(pathName);
            }


        private:
            void UpdateDisplayNameFromPath() {
                displayName = pathName.filename();
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
            std::string name = "";
            std::string displayName = "";
            std::filesystem::path pathName;
            Node::Ref parent = nullptr;
            EditorModel::Ref model = nullptr;   // This is only set for leaf nodes..
            std::vector<Node::Ref> childNodes = {};
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

        const std::unordered_map<std::string, Workspace::Node::Ref> &GetRootNodes() {
            return rootNodes;
        }

        Node::Ref GetActiveFolderNode() {
            return activeFolderNode;
        }
        void SetActiveFolderNode(Node::Ref newActiveFolder) {
            activeFolderNode = newActiveFolder;
        }

        bool OpenFolder(const std::string &folder);

        Node::Ref NewModel();                       // Adds an empty model/file to the default workspace
        Node::Ref NewModel(const Node::Ref parent); // Adds an empty model/file to a specific workspace

        // Adds a file-reference (i.e. doesn't load contents) to the default workspace
        Node::Ref NewModelWithFileRef(const std::filesystem::path &pathFileName);
        // Adds a file-reference (i.e doesn't load contents) to a specific (named) workedspace
        Node::Ref NewModelWithFileRef(Node::Ref parent, const std::filesystem::path &pathFileName);

        Node::Ref GetNodeFromModel(EditorModel::Ref model);

        bool RemoveModel(EditorModel::Ref model);

        void DumpToLog();

    protected:
        bool ReadFolderToNode(Node::Ref rootNode, const std::filesystem::path &folder);
        void UpdateMetaDataForNode(Node::Ref node);
        Node::Ref GetOrAddNode(const std::string &name);
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

        std::unordered_map<std::string, Node::Ref> rootNodes = {};

        std::vector<EditorModel::Ref> models;

    };
}


#endif //EDITOR_WORKSPACE_H
