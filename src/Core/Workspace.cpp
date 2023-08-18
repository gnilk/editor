//
// Created by gnilk on 09.05.23.
//
#include <filesystem>

#include "Core/Config/Config.h"
#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"

#include "Workspace.h"

using namespace gedit;
namespace fs = std::filesystem;

Workspace::Workspace() {
    logger = gnilk::Logger::GetLogger("Workspace");
}


Workspace::~Workspace() {
    rootNodes.clear();
    models.clear();
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
        auto workspace = Workspace::Node::Create(".");
        workspace->SetDisplayName(nameDefault);
        rootNodes["default"] = workspace;
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

    auto node = parent->AddChild(pathFileName.filename());
    //parent->AddModel(editorModel);
    node->SetModel(editorModel);

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
    textBuffer->SetLanguage(Editor::Instance().GetLanguageForExtension("default"));


    EditorModel::Ref editorModel = EditorModel::Create();
    editorModel->Initialize(editController, textBuffer);

    auto modelNode = parent->AddChild(filename);
    modelNode->SetModel(editorModel);

//    auto node = parent->AddModel(editorModel);
//    node->SetParent(parent);

    NotifyChangeHandler();

    return editorModel;
}

bool Workspace::RemoveModel(EditorModel::Ref model) {
    auto node = NodeFromModel(model);
    if (!node.has_value()) {
        return false;
    }
    auto nodePtr = node.value();
    if (nodePtr->GetParent() == nullptr) {
        return false;
    }
    nodePtr->GetParent()->DelChild(nodePtr);
    nodePtr->SetModel(nullptr);
    return true;
}

std::optional<Workspace::Node::Ref> Workspace::NodeFromModel(EditorModel::Ref model) {
    for(auto &[name, node] : rootNodes) {
        auto modelNode = node->FindModel(model);
        if (modelNode != nullptr) {
            return modelNode;
        }
    }
    return {};
}

// This returns the last valid name of a full path
// /src/app/myapp  => myapp
// /src/app/myapp/ => myapp     <- this is the reason we have this function...

static std::string LastNameOfPath(const std::filesystem::path &pathName) {
    auto it = pathName.end();
    it--;
    if (it->string() != "") {
        return it->string();
    }
    if (it == pathName.begin()) {
        return "";
    }
    it--;
    return it->string();
}

// Open a folder and create the workspace from the folder name...
bool Workspace::OpenFolder(const std::string &folder) {
    // Disable notifications - otherwise the callback is invoked for each added model...

    // If it doesn't exists - just leave..
    auto pathName = fs::absolute(fs::path(folder));
    if (!fs::exists(pathName)) {
        return false;
    }
    if (!fs::is_directory(pathName)) {
        return false;
    }

    //auto name = pathName.filename();
    auto name = LastNameOfPath(pathName);

    DisableNotifications();
    auto rootNode = GetOrAddNode(name);
    if (!ReadFolderToNode(rootNode, pathName)) {
        // Make sure we enable notification again...
        EnableNotifications();
        return false;
    }

    EnableNotifications();
    NotifyChangeHandler();
    return true;
}


bool Workspace::ReadFolderToNode(Node::Ref rootNode, const std::filesystem::path &folder) {
    logger->Debug("Reading folder: %s", folder.c_str());
    for(auto const &entry : fs::directory_iterator(folder)) {
        if (fs::is_directory(entry)) {
            const auto &name = entry.path().filename();
            logger->Debug("D: %s", name.c_str());
            auto dirNode = rootNode->GetOrAddChild(name);
            ReadFolderToNode(dirNode, entry);
        } else if (fs::is_regular_file(entry)) {
            const auto &name = entry.path().filename();
            logger->Debug("F: %s", name.c_str());
            auto model = NewModelWithFileRef(rootNode, entry.path());
        }
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


