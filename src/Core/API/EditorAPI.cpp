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
void EditorAPI::NewBuffer(const char *name) {
    if (!Editor::Instance().NewBuffer(name)) {
        // log this..
        RuntimeConfig::Instance().OutputConsole()->WriteLine("Unable to create new buffer");
    }
}
TextBufferAPI::Ref EditorAPI::LoadBuffer(const char *filename) {
    auto idx =  Editor::Instance().LoadBuffer(filename);
    if (idx < 0) {
        return nullptr;
    }
    auto model = Editor::Instance().GetModelFromIndex(idx);
    if (model == nullptr) {
        return nullptr;
    }
    return std::make_shared<TextBufferAPI>(model->GetTextBuffer());
}
void EditorAPI::SetActiveBuffer(TextBufferAPI::Ref activeBuffer) {
    Editor::Instance().SetActiveModel(activeBuffer->GetTextBuffer());
    RuntimeConfig::Instance().GetRootView().InvalidateAll();
}
