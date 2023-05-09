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

#include "logger.h"

#include "EditorModel.h"

namespace gedit {
    class Workspace {
        // Node define the structure of a workspace
    public:
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

            //////////////////
            // Functionality related to models of a node
            void AddModel(EditorModel::Ref model) {
                models.push_back(model);
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
            std::unordered_map<std::string, Node::Ref> childNodes = {};

            std::vector<EditorModel::Ref> models = {};
        };
    public:
        Workspace() = default;
        virtual ~Workspace() = default;

        std::vector<EditorModel::Ref> &GetModels();
        size_t GetActiveModelIndex();


        void SetActiveModel(TextBuffer::Ref textBuffer);

        size_t NextModelIndex(size_t idxCurrent);
        EditorModel::Ref GetModelFromIndex(size_t idxModel);

        bool OpenFolder(const std::string &folder);

        EditorModel::Ref NewModelWithFileRef(const std::filesystem::path &pathFileName);
        EditorModel::Ref NewModelWithParent(const Node::Ref parent);


    protected:
        EditorModel::Ref LoadEditorModelFromFile(const char *filename);

    private:
        gnilk::ILogger *logger = nullptr;
        Node::Ref rootNode = nullptr;
        // WorkspaceNode root;
        std::vector<EditorModel::Ref> models;   // rename..

    };
}


#endif //EDITOR_WORKSPACE_H
