//
// Created by gnilk on 29.08.23.
//

#include "Core/Editor.h"
#include "DocumentAPI.h"

using namespace gedit;

DocumentAPI::Ref DocumentAPI::Create(Workspace::Node::Ref workspaceNode) {
    return std::make_shared<DocumentAPI>(workspaceNode);
}

const std::string DocumentAPI::GetName() {
    if (document == nullptr) {
        return "";
    }
    return document->GetDisplayName();
}

const std::string DocumentAPI::GetFileName() {
    if (document == nullptr) {
        return "";
    }
    return document->GetNodePath().string();
}

void DocumentAPI::SetLanguage(const std::string &extension) {
    if (document == nullptr) {
        return;
    }
    auto textBuffer = document->GetTextBuffer();
    if (textBuffer == nullptr) {
        return;
    }

    auto lang = Editor::Instance().GetLanguageForExtension(extension);
    textBuffer->SetLanguage(lang);
    textBuffer->Reparse();
}

bool DocumentAPI::Save() {
    if (document == nullptr) {
        return false;
    }
    return document->SaveData();
}

bool DocumentAPI::SaveAs(const std::string &newFileName) {
    if (document == nullptr) {
        return false;
    }
    auto fsPath = document->GetNodePath();
    fsPath.replace_filename(newFileName);
    document->SetNodePath(fsPath);
    return document->SaveData();
}

