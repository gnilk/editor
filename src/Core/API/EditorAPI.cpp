//
// Created by gnilk on 08.04.23.
//

#include <memory>
#include "Core/RuntimeConfig.h"
#include "EditorAPI.h"


using namespace gedit;
TextBufferAPI::Ref EditorAPI::GetActiveTextBuffer() {
    auto idxActiveModel = Editor::Instance().GetActiveModelIndex();
    auto model = Editor::Instance().GetModelFromIndex(idxActiveModel);
    return std::make_shared<TextBufferAPI>(model->GetTextBuffer());
}

std::vector<std::string> EditorAPI::GetRegisteredLanguages() {
    return Editor::Instance().GetRegisteredLanguages();
}

TextBufferAPI::Ref EditorAPI::NewBuffer(const char *name) {
    auto model = Editor::Instance().NewModel(name);
    if (model == nullptr) {
        RuntimeConfig::Instance().OutputConsole()->WriteLine("Unable to create new buffer");
        return nullptr;
    }
    return std::make_shared<TextBufferAPI>(model->GetTextBuffer());
}

TextBufferAPI::Ref EditorAPI::LoadBuffer(const char *filename) {
    auto model =  Editor::Instance().LoadModel(filename);
    if (model == nullptr) {
        return nullptr;
    }
    return std::make_shared<TextBufferAPI>(model->GetTextBuffer());
}

void EditorAPI::SetActiveBuffer(TextBufferAPI::Ref activeBuffer) {
    Editor::Instance().SetActiveModel(activeBuffer->GetTextBuffer());

    // If running headless (unit-testing) this is no true
    if (RuntimeConfig::Instance().HasRootView()) {
        RuntimeConfig::Instance().GetRootView().InvalidateAll();
    }
}

std::vector<TextBufferAPI::Ref> EditorAPI::GetBuffers() {
    auto models = Editor::Instance().GetModels();
    std::vector<TextBufferAPI::Ref> buffers;
    for(auto &model : models) {
        auto bufferApi = std::make_shared<TextBufferAPI>(model->GetTextBuffer());
        buffers.push_back(bufferApi);
    }
    return buffers;
}


