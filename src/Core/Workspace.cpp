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
    projectRoots.clear();
    defaultRoot = nullptr;
    activeDocument = nullptr;
    openDocuments.clear();
}

//
// Open-document management. The Workspace owns the open-model list and the active-model pointer;
// these are pure data operations - Editor adds the UI redraw/relayout around SetActiveDocument/close.
//
void Workspace::AddOpenDocument(Document::Ref document) {
    if (document == nullptr) {
        return;
    }
    if (IsDocumentOpen(document)) {
        return;
    }
    openDocuments.push_back(document);
}

bool Workspace::RemoveOpenDocument(Document::Ref document) {
    auto it = std::find(openDocuments.begin(), openDocuments.end(), document);
    if (it == openDocuments.end()) {
        return false;
    }
    openDocuments.erase(it);
    if (activeDocument == document) {
        activeDocument = nullptr;
    }
    return true;
}

bool Workspace::IsDocumentOpen(Document::Ref document) {
    return std::find(openDocuments.begin(), openDocuments.end(), document) != openDocuments.end();
}

void Workspace::SetActiveDocument(Document::Ref document) {
    if (!IsDocumentOpen(document)) {
        return;
    }
    activeDocument = document;
}

size_t Workspace::GetActiveDocumentIndex() {
    for (size_t i = 0; i < openDocuments.size(); i++) {
        if (openDocuments[i] == activeDocument) {
            return i;
        }
    }
    // No active model yet (e.g. startup) - index 0 is the conventional fallback.
    return 0;
}

Document::Ref Workspace::GetDocumentFromIndex(size_t idxDocument) {
    if (idxDocument >= openDocuments.size()) {
        return nullptr;
    }
    return openDocuments[idxDocument];
}

Document::Ref Workspace::GetDocumentFromTextBuffer(TextBuffer::Ref textBuffer) {
    for (auto &model : openDocuments) {
        if (model->GetTextBuffer() == textBuffer) {
            return model;
        }
    }
    return nullptr;
}

size_t Workspace::NextDocumentIndex(size_t idxCurrent) {
    if (openDocuments.empty()) {
        return 0;
    }
    return (idxCurrent + 1) % openDocuments.size();
}

size_t Workspace::PreviousDocumentIndex(size_t idxCurrent) {
    if (openDocuments.empty()) {
        return 0;
    }
    return (openDocuments.size() + (idxCurrent - 1)) % openDocuments.size();
}

Workspace::Ref Workspace::Create() {
    auto ref = std::make_shared<Workspace>();
    return ref;
}

// The default ProjectRoot is the CWD-based root for new/loose files; created lazily on first use.
Workspace::ProjectRoot::Ref Workspace::GetDefaultRoot() {
    if (defaultRoot == nullptr) {
        logger->Debug("Default root does not exist, creating...");
        auto nameDefault = Config::Instance()["main"].GetStr("default_workspace_name", "default");
        auto rootDefault = std::filesystem::current_path();

        // The default root has no file-monitor and no create/delete callbacks (at least for now).
        defaultRoot = ProjectRoot::Create(nullptr, nullptr, rootDefault, nameDefault);
        // Display name defaults to the last path component; force it to the configured name.
        defaultRoot->GetRootNode()->SetDisplayName(nameDefault);

        projectRoots.push_back(defaultRoot);
    }
    return defaultRoot;
}

const Workspace::Node::Ref Workspace::GetDefaultWorkspace() {
    return GetDefaultRoot()->GetRootNode();
}

// Create an empty Document in the workspace
Workspace::Node::Ref Workspace::NewDocument(const std::string &name) {
    auto parent = activeFolderNode;
    if (parent == nullptr) {
        parent = GetDefaultWorkspace();
    } else if (!parent->IsFolder()) {
        parent = parent->GetParent();
    }

    if (parent == nullptr) {
        logger->Error("NewDocument, parent is NULL or default workspace is gone");
        exit(1);
    }
    return NewDocumentWithFileRef(parent, name);
}

// Create a new empty Document under a specific parent. A brand-new file is meant to be edited
// immediately, so its model is created eagerly (unlike folder-scanned nodes, which stay path-only).
Workspace::Node::Ref Workspace::NewDocument(const Node::Ref parent, const std::string &name) {
    auto parentPath = parent->GetNodePath();
    // Parent MUST be a directory
    if (!is_directory(parentPath)) {
        logger->Error("Parent nodePath must be directory!!!");
        return nullptr;
    }
    auto node = AddFileNode(parent, parentPath / name);
    EnsureDocumentForNode(node);
    NotifyChangeHandler();   // Removed while debugging change notifications from FolderMonitor
    return node;
}

// Create a new Document with a file-reference but don't load the contents...
Workspace::Node::Ref Workspace::NewDocumentWithFileRef(const std::filesystem::path &pathFileName) {
    auto parent = GetDefaultWorkspace();
    if (parent == nullptr) {
        logger->Error("Can't find default workspace");
        exit(1);
    }
    return NewDocumentWithFileRef(parent, pathFileName);
}

// Add a file-ref node AND eagerly create its Document (the caller is explicitly opening this file).
// Contrast with the folder scan, which adds path-only nodes via AddFileNode.
Workspace::Node::Ref Workspace::NewDocumentWithFileRef(Node::Ref parent, const std::filesystem::path &pathFileName) {
    DisableNotifications();
    auto node = AddFileNode(parent, pathFileName);
    EnsureDocumentForNode(node);
    EnableNotifications();

    NotifyChangeHandler();  // Note: This can be enabled/disabled - when reading a directory it is disabled and called once reading has completed./..

    return node;
}

// Add a path-only file node (no Document). The document is created lazily when the node is opened.
Workspace::Node::Ref Workspace::AddFileNode(Node::Ref parent, const std::filesystem::path &pathName) {
    auto node = parent->AddChild(pathName.filename().string());
    node->SetNodePath(pathName);    // also sets displayName from the path
    node->SetMeta<int>(Node::kMetaKey_NodeType, Node::kNodeFileRef);
    UpdateMetaDataForNode(node);
    return node;
}

// Lazily build the document/controller/buffer for a file node. Returns the existing model if present,
// or null for folder nodes. The language is derived from the node-path extension.
Document::Ref Workspace::EnsureDocumentForNode(Node::Ref node) {
    if (node == nullptr) {
        return nullptr;
    }
    if (node->GetDocument() != nullptr) {
        return node->GetDocument();
    }
    auto nodeType = node->GetMeta<int>(Node::kMetaKey_NodeType, Node::kNodeFolder);
    if (nodeType == Node::kNodeFolder) {
        return nullptr;
    }

    auto textBuffer = TextBuffer::CreateFileReferenceBuffer();
    auto ext = node->GetNodePath().extension();
    textBuffer->SetLanguage(Editor::Instance().GetLanguageForExtension(ext.string()));

    Document::Ref document = Document::Create(textBuffer);
    EditController::Ref editController = EditController::Create(document);

    node->SetDocument(document);        // also syncs the model's path from the node
    node->SetController(editController);

    return document;
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


bool Workspace::RemoveDocument(Document::Ref document) {
    auto node = GetNodeFromDocument(document);
    return RemoveNode(node);
}

bool Workspace::RemoveNode(Node::Ref node) {
    if (node == nullptr) {
        return false;
    }
    if (node->GetParent() == nullptr) {
        return false;
    }
    // Note: can't use 'IsFolder' here - since it will check the filesystem::path property
    //       and if we have externally deleted that path - it will simply crash.
    auto nodeType = node->GetMeta<int>(Node::kMetaKey_NodeType, Node::kNodeFolder);
    if ((nodeType != Node::kNodeFolder) && (node->GetDocument() != nullptr)) {
        if (node->GetDocument() == activeDocument) {
            Editor::Instance().CloseDocument(node->GetDocument());
        }
    }

    node->GetParent()->DelChild(node);
    node->SetDocument(nullptr);

    NotifyChangeHandler();

    return true;

}


Workspace::Node::Ref Workspace::GetNodeFromDocument(Document::Ref document) {
    for(auto &projectRoot : projectRoots) {
        auto node = projectRoot->GetRootNode()->FindDocument(document);
        if (node != nullptr) {
            return node;
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

    auto name = pathutil::LastNameOfPath(pathName);

    DisableNotifications();
    auto projectRoot = GetOrAddProjectRoot(pathName, name);
    auto rootNode = projectRoot->GetRootNode();

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
        auto node = ApplyFsEntry(rootNode, entry.path());
        if (node == nullptr) {
            continue;
        }
        // Recurse into directories.
        if (fs::is_directory(entry)) {
            ReadFolderToNode(node, entry);
        }
    }
    return true;
}

// THE single filesystem->tree mutator. Maps one filesystem entry to a node under parent:
// directories become folder nodes, regular files become path-only file nodes (model built lazily).
// Both the initial scan (ReadFolderToNode) and the folder monitor's create-callback go through here,
// so scan and live updates can never drift apart.
Workspace::Node::Ref Workspace::ApplyFsEntry(Node::Ref parent, const std::filesystem::path &path) {
    if (fs::is_directory(path)) {
        return NewFolderNode(parent, path);
    }
    if (fs::is_regular_file(path)) {
        return AddFileNode(parent, path);
    }
    return nullptr;
}

Workspace::Node::Ref Workspace::NewFolderNode(Node::Ref parent, const std::filesystem::path &pathName) {
    if (!fs::is_directory(pathName)) {
        return nullptr;
    }
    auto name = pathutil::LastNameOfPath(pathName);
    auto dirNode = parent->AddChild(name);
    dirNode->SetNodePath(pathName);
    dirNode->SetMeta<int>(Node::kMetaKey_NodeType,Node::kNodeFolder);

    return dirNode;
}


// Return the existing ProjectRoot for rootPath, or create and register a new one.
Workspace::ProjectRoot::Ref Workspace::GetOrAddProjectRoot(const std::filesystem::path &rootPath, const std::string &rootName) {
    for (auto &projectRoot : projectRoots) {
        if (projectRoot->GetRootPath() == rootPath) {
            return projectRoot;
        }
    }

    // The monitor's create-callback routes through the SAME mutator as the initial scan.
    auto cbCreateNode = [this](Node::Ref parent, const std::filesystem::path &path) {
        auto node = ApplyFsEntry(parent, path);
        NotifyChangeHandler();
        return node;
    };

    auto cbDeleteNode = [this](Node::Ref node, const std::filesystem::path &path) -> void {
        logger->Debug("Removing node: %s", node->GetDisplayName().c_str());
        RemoveNode(node);
    };

    auto projectRoot = ProjectRoot::Create(cbCreateNode, cbDeleteNode, rootPath, rootName);
    projectRoots.push_back(projectRoot);
    return projectRoot;
}


///////////////////////
