//
// Created by gnilk on 09.05.23.
//
#include <filesystem>

#include "Core/Config/Config.h"
#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"
#include "Core/BufferManager.h"

#include "Workspace.h"

using namespace gedit;
namespace fs = std::filesystem;

static Workspace::Node::Ref GetOrAddNodePath(Workspace::Node::Ref rootNode, const fs::directory_entry &entry, const fs::path &rootPath);


Workspace::Workspace() {
    logger = gnilk::Logger::GetLogger("Editor");
}


Workspace::~Workspace() {
    rootNodes.clear();
}

Workspace::Ref Workspace::Create() {
    auto ref = std::make_shared<Workspace>();
    return ref;
}

const Workspace::Node::Ref Workspace::GetDefaultWorkspace() {
    // Default not created?  - create it...
    if (rootNodes.find("default") == rootNodes.end()) {
        logger->Debug("Default workspace does not exists, creating...");
        auto nameDefault = Config::Instance()["main"].GetStr("default_workspace_name", "default");
        rootNodes["default"] = Workspace::Node::Create(nameDefault);
    }

    return rootNodes["default"];
}

// Returns a named workspace - currently the workspace is named after the folder name (this is however not needed)
const Workspace::Node::Ref Workspace::GetNamedWorkspace(const std::string &name) {
    // Default not created?  - create it...
    if (rootNodes.find("name") == rootNodes.end()) {
        logger->Debug("Namespace '%s' does not exists", name.c_str());
        return nullptr;
    }
    return rootNodes[name];
}

// Create a new model with a file-reference but don't load the contents...
EditorModel::Ref Workspace::NewModelWithFileRef(const std::filesystem::path &pathFileName) {
    auto parent = GetDefaultWorkspace();
    if (parent == nullptr) {
        logger->Error("Can't find default workspace");
        exit(1);
    }
    return NewModelWithFileRef(parent, pathFileName);
}

// Create a new model/buffer
EditorModel::Ref Workspace::NewModelWithFileRef(Node::Ref parent, const std::filesystem::path &pathFileName) {
    EditController::Ref editController = std::make_shared<EditController>();

    auto textBuffer = TextBuffer::CreateFileReferenceBuffer(pathFileName);

    EditorModel::Ref editorModel = EditorModel::Create();
    editorModel->Initialize(editController, textBuffer);

    parent->AddModel(editorModel);

    NotifyChangeHandler();  // Note: This can be enabled/disabled - when reading a directory it is disabled and called once reading has completed./..

    return editorModel;
}

// Create an empty model in the default workspace
EditorModel::Ref Workspace::NewEmptyModel() {
    auto parent = GetDefaultWorkspace();
    if (parent == nullptr) {
        logger->Error("Can't find default workspace");
        exit(1);
    }
    return NewEmptyModel(parent);
}

// Create a new empty model under a specific parent
EditorModel::Ref Workspace::NewEmptyModel(const Node::Ref parent) {
    auto nodePath = parent->GetNodePath();
    char filename[32];
    snprintf(filename,31,"new_%d",newFileCounter);
    newFileCounter++;


    nodePath.append(filename);

    EditController::Ref editController = std::make_shared<EditController>();

    auto textBuffer = TextBuffer::CreateEmptyBuffer(nodePath.filename());
    textBuffer->SetPathName(nodePath);
    textBuffer->AddLine("");
    textBuffer->SetLanguage(Editor::Instance().GetLanguageForExtension("default"));


    EditorModel::Ref editorModel = EditorModel::Create();
    editorModel->Initialize(editController, textBuffer);

    parent->AddModel(editorModel);

    NotifyChangeHandler();

    return editorModel;
}


// Open a folder and create the workspace from the folder name...
bool Workspace::OpenFolder(const std::string &folder) {
    // Disable notifications - otherwise the callback is invoked for each added model...
    DisableNotifications();
    auto rootNode = GetOrAddNode(folder);
    if (!ReadFolderToNode(rootNode, folder)) {
        // Make sure we enable notification again...
        EnableNotifications();
        return false;
    }

    EnableNotifications();
    NotifyChangeHandler();
    return true;
}


bool Workspace::ReadFolderToNode(Node::Ref rootNode, const std::string &folder) {
    auto rootPath = fs::path(folder);
    for(const fs::directory_entry &entry : fs::recursive_directory_iterator(rootPath)) {
        if (!fs::is_regular_file(entry)) continue;
        auto currentNode = GetOrAddNodePath(rootNode, entry, rootPath);
        assert(currentNode != nullptr);
        auto model = NewModelWithFileRef(currentNode, entry.path());
    }
    return true;
}

// Get/Create node and add it to the map of nodes...
Workspace::Node::Ref Workspace::GetOrAddNode(const std::string &name) {
    Node::Ref rootNode = nullptr;
    if (rootNodes.find(name) != rootNodes.end()) {
        return rootNodes[name];
    }
    rootNode = Node::Create(name);
    rootNodes[name] = rootNode;
    return rootNode;
}

// Helper, create nodes for the whole path-tree and return the leaf node
static Workspace::Node::Ref GetOrAddNodePath(Workspace::Node::Ref rootNode, const fs::directory_entry &entry, const fs::path &rootPath) {
    auto currentNode = rootNode;

    if (entry.path().parent_path() != rootPath) {
        auto parentPath = entry.path().parent_path();
        for (auto const &elem: parentPath) {
            if (elem == rootPath) continue; // skip root
            currentNode = currentNode->GetOrAddChild(elem.string());
        }
        if (currentNode == nullptr) {
            return nullptr;
        }
    }
    return currentNode;
}
