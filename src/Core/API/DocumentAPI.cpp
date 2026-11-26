//
// Created by gnilk on 29.08.23.
//

#include "Core/Editor.h"
#include "DocumentAPI.h"
#include "Core/ActionHelper.h"
#include "Core/Platform.h"
#include "Core/LineEnding.h"

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

bool DocumentAPI::IsModified() {
    if ((document == nullptr) || (document->GetDocument() == nullptr)) {
        return false;
    }
    return document->GetDocument()->IsModified();
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


bool DocumentAPI::IsChangedOnDisk() {
    if ((document == nullptr) || (document->GetDocument() == nullptr)) {
        return false;
    }
    auto doc = document->GetDocument();
    return doc->HasConflictOnDisk() || doc->IsChangedOnDisk();
}

const std::string DocumentAPI::GetEncoding() {
    if ((document == nullptr) || (document->GetDocument() == nullptr)) {
        return {};
    }
    return TextEncoding::Name(document->GetDocument()->GetTextBuffer()->GetEncoding());
}

const std::string DocumentAPI::GetLineEndings() {
    if ((document == nullptr) || (document->GetDocument() == nullptr)) {
        return {};
    }
    auto textBuffer = document->GetDocument()->GetTextBuffer();
    if (textBuffer->GetLineEndingCounts().IsMixed()) {
        return "mixed";
    }
    return LineEndings::Name(textBuffer->GetDominantLineEnding());
}

bool DocumentAPI::SetLineEndings(const std::string &name) {
    auto ending = LineEndings::FromName(name, Platform::Name());
    if (!ending.has_value() || (document == nullptr) || (document->GetDocument() == nullptr)) {
        return false;
    }
    document->GetDocument()->GetTextBuffer()->ConvertLineEndings(*ending);
    Editor::Instance().TriggerUIRedraw();
    return true;
}

const std::string DocumentAPI::GetPreferredLineEndings() {
    return LineEndings::Name(LineEndings::Preferred());
}

bool DocumentAPI::IsLineEndingsQuestion() {
    if ((document == nullptr) || (document->GetDocument() == nullptr)) {
        return false;
    }
    auto textBuffer = document->GetDocument()->GetTextBuffer();
    return textBuffer->GetLineEndingCounts().IsMixed() && !textBuffer->IsMixedLineEndingsKept()
           && (LineEndings::MixedPolicy() == LineEndings::kMixedPolicy::kAsk);
}

void DocumentAPI::KeepLineEndings() {
    if ((document == nullptr) || (document->GetDocument() == nullptr)) {
        return;
    }
    document->GetDocument()->GetTextBuffer()->KeepMixedLineEndings();
}

bool DocumentAPI::SetEncoding(const std::string &name) {
    auto newEncoding = TextEncoding::FromName(name);
    if (!newEncoding.has_value() || (document == nullptr) || (document->GetDocument() == nullptr)) {
        return false;
    }
    document->GetDocument()->GetTextBuffer()->SetEncoding(*newEncoding);
    Editor::Instance().TriggerUIRedraw();
    return true;
}

bool DocumentAPI::GotoLine(int lineNumber) {
    if ((document == nullptr) || (document->GetDocument() == nullptr) || (lineNumber <= 0)) {
        return false;
    }
    if (!ActionHelper::GotoLine(document->GetDocument(), (size_t)lineNumber)) {
        return false;
    }
    Editor::Instance().TriggerUIRedraw();
    return true;
}

bool DocumentAPI::Reload() {
    if ((document == nullptr) || (document->GetDocument() == nullptr)) {
        return false;
    }
    if (!document->GetDocument()->ReloadFromDisk()) {
        return false;
    }
    Editor::Instance().TriggerUIRedraw();
    return true;
}

bool DocumentAPI::ReformatFile() {
    if ((document == nullptr) || (document->GetDocument() == nullptr)) {
        return false;
    }
    if (!document->GetDocument()->ReformatFile()) {
        return false;
    }
    Editor::Instance().TriggerUIRedraw();
    return true;
}

bool DocumentAPI::ReformatBlock() {
    if ((document == nullptr) || (document->GetDocument() == nullptr)) {
        return false;
    }
    if (!document->GetDocument()->ReformatSelectionOrBlock()) {
        return false;
    }
    Editor::Instance().TriggerUIRedraw();
    return true;
}

bool DocumentAPI::IsReformatEnabled() {
    if ((document == nullptr) || (document->GetDocument() == nullptr)) {
        return false;
    }
    return document->GetDocument()->IsReindentable();
}

const std::string DocumentAPI::GetLanguage() {
    if ((document == nullptr) || (document->GetDocument() == nullptr)) {
        return "";
    }
    auto textBuffer = document->GetDocument()->GetTextBuffer();
    if ((textBuffer == nullptr) || !textBuffer->HaveLanguage()) {
        return "";
    }
    return textBuffer->GetLanguage().GetConfigNodeName();
}
