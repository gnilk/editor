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

// Create an empty model in the default workspace
Workspace::Node::Ref Workspace::NewModel() {
    auto parent = activeFolderNode;
    if (parent == nullptr) {
        parent = GetDefaultWorkspace();
    }
    if (parent == nullptr) {
        logger->Error("Can't find default workspace");
        exit(1);
    }
    return NewModel(parent);
}

// Create a new empty model under a specific parent
Workspace::Node::Ref Workspace::NewModel(const Node::Ref parent) {
    auto nodePath = parent->GetNodePath();

    // I want to remove file-name handling from there!!!!
    char filename[32];
    snprintf(filename,31,"new_%d",newFileCounter);
    newFileCounter++;

    nodePath.append(filename);

    EditController::Ref editController = std::make_shared<EditController>();

    auto textBuffer = TextBuffer::CreateFileReferenceBuffer();
    textBuffer->SetLanguage(Editor::Instance().GetLanguageForExtension("default"));

    EditorModel::Ref editorModel = EditorModel::Create();
    editorModel->Initialize(editController, textBuffer);

    auto modelNode = parent->AddChild(filename);
    modelNode->SetMeta<int>(Node::kMetaKey_NodeType, Node::kNodeFileRef);
    modelNode->SetModel(editorModel);
    modelNode->SetNodePath(nodePath);

    UpdateMetaDataForNode(modelNode);

    NotifyChangeHandler();

    return modelNode;
}

// Create a new model with a file-reference but don't load the contents...
Workspace::Node::Ref Workspace::NewModelWithFileRef(const std::filesystem::path &pathFileName) {
    auto parent = GetDefaultWorkspace();
    if (parent == nullptr) {
        logger->Error("Can't find default workspace");
        exit(1);
    }
    return NewModelWithFileRef(parent, pathFileName);
}

// Create a new model/buffer
Workspace::Node::Ref Workspace::NewModelWithFileRef(Node::Ref parent, const std::filesystem::path &pathFileName) {
    EditController::Ref editController = std::make_shared<EditController>();
    auto node = NewModel(parent);
    node->SetNodePath(pathFileName);

    auto ext = pathFileName.extension();
    auto lang = Editor::Instance().GetLanguageForExtension(ext.string());
    node->GetModel()->GetTextBuffer()->SetLanguage(lang);

    UpdateMetaDataForNode(node);

    NotifyChangeHandler();  // Note: This can be enabled/disabled - when reading a directory it is disabled and called once reading has completed./..

    return node;
}





void Workspace::UpdateMetaDataForNode(Node::Ref node) {
    auto pathFileName = node->GetNodePath();
    if (std::filesystem::is_directory(pathFileName)) {
        node->SetMeta<int>(Node::kMetaKey_NodeType, Node::kNodeFolder);
        return;
    }

    node->SetMeta<int>(Node::kMetaKey_NodeType, Node::kNodeFileRef);

    if (!std::filesystem::exists(pathFileName)) {
        return;
    }

    node->SetMeta<size_t>(Node::kMetaKey_FileSize, std::filesystem::file_size(pathFileName));
    // Deduce read-only flag, in essence, if we as the owner can't write we simply mark as readonly...
    auto perms = std::filesystem::status(pathFileName).permissions();
    auto bCanWrite = (std::filesystem::perms::none == (perms & std::filesystem::perms::owner_write))?false:true;
    node->SetMeta<bool>(Node::kMetaKey_ReadOnly, !bCanWrite);
}


bool Workspace::RemoveModel(EditorModel::Ref model) {
    auto node = GetNodeFromModel(model);
    if (node == nullptr) {
        return false;
    }
    if (node->GetParent() == nullptr) {
        return false;
    }
    node->GetParent()->DelChild(node);
    node->SetModel(nullptr);
    return true;
}

Workspace::Node::Ref Workspace::GetNodeFromModel(EditorModel::Ref model) {
    for(auto &[name, node] : rootNodes) {
        auto modelNode = node->FindModel(model);
        if (modelNode != nullptr) {
            return modelNode;
        }
    }
    return nullptr;
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
    rootNode->SetNodePath(pathName);
    rootNode->SetMeta<int>(Node::kMetaKey_NodeType, Node::kNodeFolder);
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
            dirNode->SetNodePath(entry.path());
            dirNode->SetMeta<int>(Node::kMetaKey_NodeType,Node::kNodeFolder);
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


///////////////////////
