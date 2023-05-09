//
// Created by gnilk on 09.05.23.
//

#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"
#include "Core/BufferManager.h"

#include "Workspace.h"

using namespace gedit;

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
EditorModel::Ref Workspace::NewModel(const char *name) {
    EditController::Ref editController = std::make_shared<EditController>();
    auto textBuffer = BufferManager::Instance().NewBuffer(name);
    textBuffer->AddLine("");
    textBuffer->SetLanguage(Editor::Instance().GetLanguageForExtension("default"));
    EditorModel::Ref editorModel = std::make_shared<EditorModel>();
    editorModel->Initialize(editController, textBuffer);

    models.push_back(editorModel);

    return editorModel;
}

EditorModel::Ref Workspace::LoadModel(const std::string &filename) {
    auto model = LoadEditorModelFromFile(filename.c_str());
    if (model == nullptr) {
        RuntimeConfig::Instance().OutputConsole()->WriteLine("Editor::LoadBuffer, Unable to load file");
        return nullptr;
    }

    models.push_back(model);

    return model;
}

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

