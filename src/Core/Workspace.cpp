//
// Created by gnilk on 09.05.23.
//
#include <filesystem>

#include "Core/Config/Config.h"
#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"

#include "Workspace.h"
#include "Core/PathUtil.h"

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

// The default desktop is handled a bit differently - as the CWD may change while we are running
const Workspace::Node::Ref Workspace::GetDefaultWorkspace() {
    // Default not created?  - create it...
    auto nameDefault = Config::Instance()["main"].GetStr("default_workspace_name", "default");
    if (rootNodes.find(nameDefault) == rootNodes.end()) {
        logger->Debug("Default workspace does not exists, creating...");
        auto rootDefault = std::filesystem::current_path();

        // Note: The default desktop does not have a file-monitor and does not require a create callback (at least now)
        auto desktop = Desktop::Create(nullptr, nullptr, rootDefault, nameDefault);
        rootNodes[nameDefault] = desktop;
    }

    return rootNodes["default"]->GetRootNode();
}

// Returns a named workspace - currently the workspace is named after the folder name (this is however not needed)
const Workspace::Node::Ref Workspace::GetNamedWorkspace(const std::string &name) {
    // Default not created?  - create it...
    if (rootNodes.find("name") == rootNodes.end()) {
        logger->Debug("Namespace '%s' does not exists", name.c_str());
        return nullptr;
    }
    return rootNodes[name]->GetRootNode();
}

// Create an empty model in the default workspace
Workspace::Node::Ref Workspace::NewModel(const std::string &name) {
    auto parent = activeFolderNode;
    if (parent == nullptr) {
        parent = GetDefaultWorkspace();
    } else if (!parent->IsFolder()) {
        parent = parent->GetParent();
    }

    if (parent == nullptr) {
        logger->Error("NewModel, parent is NULL or default workspace is gone");
        exit(1);
    }
    return NewModel(parent, name);
}

// FIXME: Revisit this one - I am not sure it works...
// Create a new empty model under a specific parent
Workspace::Node::Ref Workspace::NewModel(const Node::Ref parent, const std::string &name) {
    auto nodePath = parent->GetNodePath();

    // Not sure this works...
    nodePath.replace_filename(name);

    EditController::Ref editController = std::make_shared<EditController>();

    auto textBuffer = TextBuffer::CreateFileReferenceBuffer();
    textBuffer->SetLanguage(Editor::Instance().GetLanguageForExtension("default"));

    EditorModel::Ref editorModel = EditorModel::Create();
    editorModel->Initialize(editController, textBuffer);

    auto modelNode = parent->AddChild(name);
    modelNode->SetMeta<int>(Node::kMetaKey_NodeType, Node::kNodeFileRef);
    modelNode->SetModel(editorModel);
    modelNode->SetNodePath(nodePath);

    UpdateMetaDataForNode(modelNode);

    // NotifyChangeHandler();   // Removed while debugging change notifications from FolderMonitor

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

    auto node = NewModel(parent, pathFileName.filename());
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
    for(auto &[name, desktop] : rootNodes) {
        auto rootNode = desktop->GetRootNode();
        auto modelNode = rootNode->FindModel(model);
        if (modelNode != nullptr) {
            return modelNode;
        }
    }
    return nullptr;
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
    auto name = pathutil::LastNameOfPath(pathName);

    DisableNotifications();
    auto desktop = GetOrAddDesktop(pathName, name);
    auto rootNode = desktop->GetRootNode();

    rootNode->SetNodePath(pathName);
    rootNode->SetMeta<int>(Node::kMetaKey_NodeType, Node::kNodeFolder);
    if (!ReadFolderToNode(rootNode, pathName)) {
        // Make sure we enable notification again...
        EnableNotifications();
        return false;
    }

//    desktop->StartFolderMonitor([this, desktop](const std::string &pathName,FolderMonitor::kChangeFlags flags)->void {
//        auto logger = gnilk::Logger::GetLogger("Workspace");
//        logger->Debug("%s - 0x%x : %s", desktop->GetName().c_str(), static_cast<int>(flags), pathName.c_str());
//    });

    EnableNotifications();
    NotifyChangeHandler();
    return true;
}


bool Workspace::ReadFolderToNode(Node::Ref rootNode, const std::filesystem::path &folder) {
    logger->Debug("Reading folder: %s", folder.c_str());
    for(auto const &entry : fs::directory_iterator(folder)) {
        if (fs::is_directory(entry)) {
            const auto &name = entry.path().filename();
            // logger->Debug("D: %s", name.c_str());
            auto dirNode = rootNode->AddChild(name);
            dirNode->SetNodePath(entry.path());
            dirNode->SetMeta<int>(Node::kMetaKey_NodeType,Node::kNodeFolder);
            ReadFolderToNode(dirNode, entry);
        } else if (fs::is_regular_file(entry)) {
            const auto &name = entry.path().filename();
            // logger->Debug("F: %s", name.c_str());
            auto model = NewModelWithFileRef(rootNode, entry.path());
        }
    }
    return true;
}

// Root nodes are actual workspace instances...
Workspace::Desktop::Ref Workspace::GetOrAddDesktop(const std::filesystem::path &rootPath, const std::string &desktopName) {
    Desktop::Ref desktop = nullptr;
    auto fqDeskName = rootPath.string();
    if (rootNodes.find(fqDeskName) != rootNodes.end()) {
        return rootNodes[fqDeskName];
    }
    auto cbCreateNode = [this](Node::Ref parent, const std::filesystem::path &path) {
        return NewModelWithFileRef(parent, path);
    };

    auto cbDeleteNode = [this](Node::Ref node, const std::filesystem::path &path) -> void {
        logger->Debug("I should delete node: %s", node->GetDisplayName().c_str());
    };

    desktop = Desktop::Create(cbCreateNode, cbDeleteNode, rootPath, desktopName);
    rootNodes[fqDeskName] = desktop;
    return desktop;
}


///////////////////////
