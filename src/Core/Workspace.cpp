//
// Created by gnilk on 09.05.23.
//
#include <filesystem>


#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"
#include "Core/BufferManager.h"

#include "Workspace.h"

using namespace gedit;
namespace fs = std::filesystem;

std::vector<EditorModel::Ref> &Workspace::GetModels() {
    return models;
}
size_t Workspace::GetActiveModelIndex() {
    for(size_t i=0;i<models.size();i++) {
        if (models[i]->IsActive()) {
            return i;
        }
    }
    auto logger = gnilk::Logger::GetLogger("Editor");
    logger->Error("No active model!!!!!");
    // THIS SHOULD NOT HAPPEN!!!
    return 0;
}

void Workspace::SetActiveModel(TextBuffer::Ref textBuffer) {
    auto idxCurrent = GetActiveModelIndex();
    for(size_t i = 0; i<models.size();i++) {
        if (models[i]->GetTextBuffer() == textBuffer) {
            models[idxCurrent]->SetActive(false);
            models[i]->SetActive(true);
            // THIS IS NOT A WORK OF BEAUTY
            RuntimeConfig::Instance().SetActiveEditorModel(models[i]);
            return;
        }
    }
}

size_t Workspace::NextModelIndex(size_t idxCurrent) {
    auto next = (idxCurrent + 1) % models.size();
    return next;
}

EditorModel::Ref Workspace::GetModelFromIndex(size_t idxModel) {
    if (idxModel > (models.size()-1)) {
        return nullptr;
    }
    return models[idxModel];
}


// Create a new model/buffer
EditorModel::Ref Workspace::NewModelWithFileRef(const std::filesystem::path &pathFileName) {
    EditController::Ref editController = std::make_shared<EditController>();

    auto textBuffer = TextBuffer::CreateFileReferenceBuffer(pathFileName);

    EditorModel::Ref editorModel = EditorModel::Create();
    editorModel->Initialize(editController, textBuffer);

    models.push_back(editorModel);
    return editorModel;
}

// Create a new model under a specific parent
EditorModel::Ref Workspace::NewModelWithParent(const Node::Ref parent) {
    auto nodePath = parent->GetNodePath();
    nodePath.append("new");
    EditController::Ref editController = std::make_shared<EditController>();

    auto textBuffer = TextBuffer::CreateEmptyBuffer(nodePath);

    EditorModel::Ref editorModel = EditorModel::Create();
    editorModel->Initialize(editController, textBuffer);

    models.push_back(editorModel);
    return editorModel;
}

// Helper, create nodes for the whole path-tree and return the leaf node
static Workspace::Node::Ref GetOrAddNodePath(Workspace::Node::Ref rootNode, const fs::directory_entry &entry, const fs::path &rootPath) {
    auto currentNode = rootNode;

    if (entry.path().parent_path() != rootPath) {
        auto parentPath = entry.path().parent_path();
        printf("%s, %s\n", parentPath.c_str(), entry.path().filename().c_str());
        for (auto const &elem: parentPath) {
            if (elem == rootPath) continue; // skip root

            currentNode = currentNode->GetOrAddChild(elem.string());
            if (currentNode->GetParent() != nullptr) {
                printf("   %s (p: %s)\n", elem.c_str(), currentNode->GetParent()->GetName().c_str());
            } else {
                printf("R: %s\n", elem.c_str());
            }
        }
        if (currentNode == nullptr) {
            // something went awfully wrong
            return nullptr;
        }
    } else {
        printf("[root] %s\n", entry.path().filename().c_str());
    }
    return currentNode;
}

// Open a folder and create the workspace tree
bool Workspace::OpenFolder(const std::string &folder) {
    rootNode = Node::Create(folder);
    auto rootPath = fs::path(folder);
    for(const fs::directory_entry &entry : fs::recursive_directory_iterator(rootPath)) {
        if (!fs::is_regular_file(entry)) continue;
        auto currentNode = GetOrAddNodePath(rootNode, entry, rootPath);
        assert(currentNode != nullptr);

        printf("Adding model with name: %s\n", entry.path().filename().c_str());
        auto model = NewModelWithFileRef(entry.path());
        currentNode->AddModel(model);
    }
    return true;
}

// tmp tmp tmp
EditorModel::Ref Workspace::LoadEditorModelFromFile(const char *filename) {
    logger->Debug("Loading file: %s", filename);
    TextBuffer::Ref textBuffer;

    textBuffer = BufferManager::Instance().NewBufferFromFile(filename);
    if (textBuffer == nullptr) {
        logger->Error("Unable to load file: '%s'", filename);
        return nullptr;
    }
    logger->Debug("End Loading");


    std::filesystem::path pathName(filename);
    auto extension = pathName.extension();
    textBuffer->SetLanguage(Editor::Instance().GetLanguageForExtension(extension));

    EditController::Ref editController = std::make_shared<EditController>();
    EditorModel::Ref editorModel = std::make_shared<EditorModel>();

    editorModel->Initialize(editController, textBuffer);

    return editorModel;
}

