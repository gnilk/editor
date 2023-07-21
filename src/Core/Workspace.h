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
            using Ref = std::shared_ptr<Node>;
            using ChildNodesValueType = std::unordered_map<std::string, Node::Ref>::value_type;
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
                for(auto &[key, value] : childNodes) {
                    outNodes.push_back(value);
                }
                return outNodes.size();
            }

            Node::Ref AddChild(const std::string &newName) {
                auto child = Node::Create(newName);
                child->parent = shared_from_this();
                childNodes[newName] = child;
                // Resolve path??
                return child;
            }

            bool DelChild(Node::Ref child) {
                if (!HasChild(name)) {
                    return false;
                }
                auto it = childNodes.find(name);
                childNodes.erase(it);
                return true;
            }

            bool DelModel(Node::Ref nodeForModel) {
                auto itModel = std::find_if(models.begin(), models.end(), [&nodeForModel](const Node::Ref &node){ return nodeForModel->GetModel() == node->GetModel(); });
                if (itModel == models.end()) {
                    return false;
                }
                models.erase(itModel);

//                // No child is not an error as leaves are just stored in the models array...
//                auto itNode = std::find_if(childNodes.begin(), childNodes.end(), [&nodeForModel](const ChildNodesValueType &pair) { return nodeForModel == pair.second; });
//                if (itNode == childNodes.end()) {
//                    childNodes.erase(itNode);
//                }

                return true;
            }

            bool HasChild(const std::string &nameToFind) {
                if(childNodes.find(nameToFind) == childNodes.end()) {
                    return false;
                }
                return true;
            }

            Node::Ref FindModel(const EditorModel::Ref searchModel) {
                for(auto &node : models) {
                    auto nodeForModel = node->FindModel(searchModel);
                    if (nodeForModel != nullptr) {
                        return nodeForModel;
                    }
                    if (node->GetModel() == searchModel) return node;
                }
                return nullptr;
            }

            Node::Ref GetOrAddChild(const std::string &childName) {
                if (HasChild(childName)) {
                    return childNodes[childName];
                }
                return AddChild(childName);
            }
            std::optional<Node::Ref> GetChild(const std::string &childName) {
                if (!HasChild(childName)) {
                    return {};
                }
                return childNodes[childName];
            }

            std::filesystem::path GetNodePath() {
                std::filesystem::path path;
                RecursiveGetNodePath(path);
                return path;
            }

            const std::vector<Node::Ref> &GetModels() {
                return models;
            }

            //////////////////
            // Functionality related to models of a node
            Node::Ref AddModel(EditorModel::Ref newModel) {
                auto node = Create(newModel);
                models.push_back(node);
                return node;
            }
            EditorModel::Ref GetModel() {
                return model;
            }


        private:
            void RecursiveGetNodePath(std::filesystem::path &path) {
                if (parent != nullptr) {
                    RecursiveGetNodePath(path);
                    path += name;
                } else {
                    path += name;
                }
            }

        private:
            std::string name = "";
            std::string displayName = "";
            Node::Ref parent = nullptr;
            EditorModel::Ref model = nullptr;   // This is only set for leaf nodes..



            std::unordered_map<std::string, Node::Ref> childNodes = {};
            std::vector<Node::Ref> models = {};

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

        bool OpenFolder(const std::string &folder);

        EditorModel::Ref NewEmptyModel();                       // Adds an empty model/file to the default workspace
        EditorModel::Ref NewEmptyModel(const Node::Ref parent); // Adds an empty model/file to a specific workspace

        // Adds a file-reference (i.e. doesn't load contents) to the default workspace
        EditorModel::Ref NewModelWithFileRef(const std::filesystem::path &pathFileName);
        // Adds a file-reference (i.e doesn't load contents) to a specific (named) workedspace
        EditorModel::Ref NewModelWithFileRef(Node::Ref parent, const std::filesystem::path &pathFileName);

        bool CloseModel(EditorModel::Ref model);

    protected:
        bool ReadFolderToNode(Node::Ref rootNode, const std::string &folder);
        Node::Ref GetOrAddNode(const std::string &name);
        std::optional<Node::Ref> NodeFromModel(EditorModel::Ref model);
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

        std::unordered_map<std::string, Node::Ref> rootNodes = {};

        std::vector<EditorModel::Ref> models;

    };
}


#endif //EDITOR_WORKSPACE_H
