//
// Created by gnilk on 08.04.23.
//

#include <memory>
#include "Core/RuntimeConfig.h"
#include "Core/Config/Config.h"
#include "EditorAPI.h"
#include "Core/RuntimeConfig.h"
#include "Core/Views/RootView.h"

using namespace gedit;


DocumentAPI::Ref EditorAPI::NewDocument(const char *name) {
    auto workspace = Editor::Instance().GetWorkspace();
    if (workspace == nullptr) {
        return nullptr;
    }
    auto node = workspace->NewModel(name);
    // This will also activate the model...
    Editor::Instance().OpenModelFromWorkspace(node);

    return DocumentAPI::Create(node);
}

DocumentAPI::Ref EditorAPI::GetActiveDocument() {
    auto workspaceNode = Editor::Instance().GetWorkspaceNodeForActiveModel();
    return DocumentAPI::Create(workspaceNode);
}

ThemeAPI::Ref EditorAPI::GetCurrentTheme() {
    auto theme = Editor::Instance().GetTheme();
    return std::make_shared<ThemeAPI>(theme);
}

std::vector<std::string> EditorAPI::GetRegisteredLanguages() {
    return Editor::Instance().GetRegisteredLanguages();
}

std::vector<PluginCommand::Ref> EditorAPI::GetRegisteredCommands() {
    return RuntimeConfig::Instance().GetPluginCommands();
}

const std::vector<std::string> EditorAPI::GetTopViews() {
    auto &rvBase = RuntimeConfig::Instance().GetRootView();
    RootView *rootView = static_cast<RootView *>(&rvBase);
    return rootView->GetTopViews();
}

ViewAPI::Ref EditorAPI::GetViewByName(const char *name) {
    auto &rvBase = RuntimeConfig::Instance().GetRootView();
    RootView *rootView = static_cast<RootView *>(&rvBase);
    std::string strName(name);
    auto viewRef = rootView->GetTopViewByName(strName);
    if (viewRef == nullptr) {
        return nullptr;
    }
    return std::make_shared<ViewAPI>(viewRef);
}

void EditorAPI::CloseActiveBuffer() {
    auto current = Editor::Instance().GetActiveModel();
    if (current != nullptr) {
        Editor::Instance().CloseModel(current);
    }
}


/*


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


*/