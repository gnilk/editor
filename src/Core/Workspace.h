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
        public:
            // A node has a name and generally points to a directory - but we can create nodes a bit how we want..
            // Thus we can mimic VStudio with "Source", "Headers", etc.. which are virtual nodes but appear as
            // directories...
            explicit Node(const std::string &nodeName) : name(nodeName) {

            }
            virtual ~Node() = default;
            static Ref Create(const std::string &nodeName) {
                return std::make_shared<Node>(nodeName);
            }
            static Ref Create(EditorModel::Ref editorModel) {
                auto node = std::make_shared<Node>("");
                node->model = editorModel;
                return node;
            }
            const std::string &GetName() {
                return name;
            }
            const Node::Ref GetParent() {
                return parent;
            }

            size_t FlattenChilds(std::vector<Node::Ref> &outNodes) {
                for(auto &[key, value] : childNodes) {
                    outNodes.push_back(value);
                }
                return outNodes.size();
            }

            Node::Ref AddChild(const std::string &name) {
                auto child = Node::Create(name);
                child->parent = shared_from_this();
                childNodes[name] = child;
                // Resolve path??
                return child;
            }

            bool HasChild(const std::string &name) {
                if(childNodes.find(name) == childNodes.end()) {
                    return false;
                }
                return true;
            }
            Node::Ref GetOrAddChild(const std::string &name) {
                if (HasChild(name)) {
                    return childNodes[name];
                }
                return AddChild(name);
            }
            std::optional<Node::Ref> GetChild(const std::string &name) {
                if (!HasChild(name)) {
                    return {};
                }
                return childNodes[name];
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
            void AddModel(EditorModel::Ref model) {
                auto node = Create(model);

                models.push_back(node);
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

    protected:
        bool ReadFolderToNode(Node::Ref rootNode, const std::string &folder);
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

        std::unordered_map<std::string, Node::Ref> rootNodes = {};
        //Node::Ref rootNode = nullptr;

        // FIXME: Remove this
        std::vector<EditorModel::Ref> models;   // rename..

    };
}


#endif //EDITOR_WORKSPACE_H
