//
// Created by gnilk on 08.04.23.
//

#include <memory>
#include "Core/RuntimeConfig.h"
#include "EditorAPI.h"
#include "Core/RuntimeConfig.h"

using namespace gedit;
TextBufferAPI::Ref EditorAPI::GetActiveTextBuffer() {
    auto idxActiveModel = Editor::Instance().GetActiveModelIndex();
    auto model = Editor::Instance().GetModelFromIndex(idxActiveModel);
    return std::make_shared<TextBufferAPI>(model->GetTextBuffer());
}

std::vector<std::string> EditorAPI::GetRegisteredLanguages() {
    return Editor::Instance().GetRegisteredLanguages();
}

std::vector<PluginCommand::Ref> EditorAPI::GetRegisteredCommands() {
    return RuntimeConfig::Instance().GetPluginCommands();
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
    auto model = Editor::Instance().GetModelFromTextBuffer(activeBuffer->GetTextBuffer());
    if (model == nullptr) {
        return;
    }
    Editor::Instance().SetActiveModel(model);
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


