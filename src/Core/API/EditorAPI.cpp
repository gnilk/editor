//
// Created by gnilk on 08.04.23.
//

#include <memory>
#include "Core/RuntimeConfig.h"
#include "Core/Config/Config.h"
#include "EditorAPI.h"
#include "Core/RuntimeConfig.h"
#include "Core/UI/Views/RootView.h"
#include "Core/UnicodeHelper.h"
#include "Core/ClipBoard.h"

using namespace gedit;



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

DocumentAPI::Ref EditorAPI::NewDocument(const char *name) {
    auto workspace = Editor::Instance().GetWorkspace();
    if (workspace == nullptr) {
        return nullptr;
    }
    auto node = workspace->NewDocument(name);
    // This will also activate the document...
    Editor::Instance().OpenDocumentFromWorkspace(node);

    return DocumentAPI::Create(node);
}

DocumentAPI::Ref EditorAPI::NewDocumentFromText(const char *name, const char *text) {
    auto workspace = Editor::Instance().GetWorkspace();
    if (workspace == nullptr) {
        return nullptr;
    }
    auto node = workspace->NewDocument(name);
    auto textBuffer = node->GetTextBuffer();
    if (textBuffer != nullptr && text != nullptr) {
        // A fresh document buffer carries a single placeholder empty line (CreateEmptyBuffer); drop it
        // so the supplied text becomes the buffer's content rather than sitting under a leading blank
        // line. Guarded on IsEmpty() so a reused, already-loaded node is never wiped.
        if (textBuffer->IsEmpty()) {
            while (textBuffer->NumLines() > 0) {
                textBuffer->DeleteLineAt(0);
            }
        }
        // Split on '\n' into Lines. A non-existent new file is never loaded over this content
        // (Node::LoadData is a no-op when the path doesn't exist), so filling before OpenDocument is
        // safe and the document opens already showing the text.
        auto u32 = UnicodeHelper::utf8to32(text);
        size_t start = 0;
        while (true) {
            size_t nl = u32.find(U'\n', start);
            size_t len = (nl == std::u32string::npos) ? std::u32string::npos : (nl - start);
            std::u32string lineText = u32.substr(start, len);
            if (!lineText.empty() && lineText.back() == U'\r') {
                lineText.pop_back();   // tolerate CRLF input
            }
            textBuffer->AddLine(lineText);
            if (nl == std::u32string::npos) {
                break;
            }
            start = nl + 1;
        }
        textBuffer->Reparse();
    }
    // This will also activate the document...
    Editor::Instance().OpenDocumentFromWorkspace(node);

    return DocumentAPI::Create(node);
}

DocumentAPI::Ref EditorAPI::LoadDocument(const char *filename) {
    auto workspace = Editor::Instance().GetWorkspace();
    if (workspace == nullptr) {
        return nullptr;
    }
    auto node = workspace->NewDocumentWithFileRef(filename);
    if (node == nullptr) {
        return nullptr;
    }
    // This will load data and activate the document...
    if (Editor::Instance().OpenDocumentFromWorkspace(node) == nullptr) {
        return nullptr;
    }
    return DocumentAPI::Create(node);
}

DocumentAPI::Ref EditorAPI::GetActiveDocument() {
    auto workspaceNode = Editor::Instance().GetWorkspaceNodeForActiveDocument();
    return DocumentAPI::Create(workspaceNode);
}

std::vector<DocumentAPI::Ref> EditorAPI::GetDocuments() {
    std::vector<DocumentAPI::Ref> documents;

    auto &openDocuments = Editor::Instance().GetDocuments();
    documents.reserve(openDocuments.size());
    for(auto &document : openDocuments) {
        auto node = Editor::Instance().GetWorkspaceNodeForDocument(document);
        documents.emplace_back(DocumentAPI::Create(node));
    }

    return documents;
}

void EditorAPI::CloseActiveDocument() {
    auto current = Editor::Instance().GetActiveDocument();
    if (current != nullptr) {
        Editor::Instance().CloseDocument(current);
    }
}

void EditorAPI::CopyToClipboard(const char *text) {
    if (text == nullptr) {
        return;
    }
    auto u32 = UnicodeHelper::utf8to32(text);
    Editor::Instance().GetClipBoard().CopyText(u32);
}


/*


TextBufferAPI::Ref EditorAPI::LoadBuffer(const char *filename) {
    auto document =  Editor::Instance().LoadDocument(filename);
    if (document == nullptr) {
        return nullptr;
    }
    return std::make_shared<TextBufferAPI>(document->GetTextBuffer());
}

void EditorAPI::SetActiveBuffer(TextBufferAPI::Ref activeBuffer) {
    auto document = Editor::Instance().GetDocumentFromTextBuffer(activeBuffer->GetTextBuffer());
    if (document == nullptr) {
        return;
    }
    Editor::Instance().SetActiveDocument(document);
}

std::vector<TextBufferAPI::Ref> EditorAPI::GetBuffers() {
    auto documents = Editor::Instance().GetDocuments();
    std::vector<TextBufferAPI::Ref> buffers;
    for(auto &document : documents) {
        auto bufferApi = std::make_shared<TextBufferAPI>(document->GetTextBuffer());
        buffers.push_back(bufferApi);
    }
    return buffers;
}


*/