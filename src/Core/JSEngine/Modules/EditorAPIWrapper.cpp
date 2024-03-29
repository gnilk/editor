#include "EditorAPIWrapper.h"
#include "Core/API/EditorAPI.h"
#include "dukglue/dukglue.h"
#include "DocumentAPIWrapper.h"

using namespace gedit;

void EditorAPIWrapper::RegisterModule(duk_context *ctx) {
//    dukglue_register_constructor<EditorAPIWrapper>(ctx, "Editor");
    static EditorAPIWrapper editorApiWrapper;

    dukglue_push(ctx, &editorApiWrapper);
    duk_put_global_string(ctx, "Editor");

    dukglue_register_method(ctx, &EditorAPIWrapper::GetActiveDocument, "GetActiveDocument");
    dukglue_register_method(ctx, &EditorAPIWrapper::NewDocument, "NewDocument");
    dukglue_register_method(ctx, &EditorAPIWrapper::GetDocuments, "GetDocuments");
    dukglue_register_method(ctx, &EditorAPIWrapper::CloseActiveDocument, "CloseActiveDocument");

    dukglue_register_method(ctx, &EditorAPIWrapper::GetCurrentTheme, "GetCurrentTheme");
    dukglue_register_method(ctx, &EditorAPIWrapper::ExitEditor, "ExitEditor");
    dukglue_register_method(ctx, &EditorAPIWrapper::GetRegisteredLanguages, "GetRegisteredLanguages");

    dukglue_register_method(ctx, &EditorAPIWrapper::GetHelp, "GetCommandDescriptions");
    dukglue_register_method(ctx, &EditorAPIWrapper::GetRootViewNames, "GetViewNames");
    dukglue_register_method(ctx, &EditorAPIWrapper::GetViewByName, "GetViewByName");


    // Some test stuff...
    dukglue_register_method(ctx, &EditorAPIWrapper::GetTestArray, "GetTestArray");

}

//
// Impl API
//
DocumentAPIWrapper::Ref EditorAPIWrapper::GetActiveDocument() {
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    return DocumentAPIWrapper::Create(editorApi->GetActiveDocument());
}

DocumentAPIWrapper::Ref EditorAPIWrapper::NewDocument(const char *name) {
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    return DocumentAPIWrapper::Create(editorApi->NewDocument(name));
}

std::vector<DocumentAPIWrapper::Ref> EditorAPIWrapper::GetDocuments() {
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    auto documents = editorApi->GetDocuments();
    std::vector<DocumentAPIWrapper::Ref> docWrappers;
    docWrappers.reserve(documents.size());
    for(auto &doc : documents) {
        docWrappers.emplace_back(DocumentAPIWrapper::Create(doc));
    }
    return docWrappers;
}

void EditorAPIWrapper::CloseActiveDocument() {
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    editorApi->CloseActiveDocument();
}


ThemeAPIWrapper::Ref EditorAPIWrapper::GetCurrentTheme() {
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    return ThemeAPIWrapper::Create(editorApi->GetCurrentTheme());
}

void EditorAPIWrapper::ExitEditor() {
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    editorApi->ExitEditor();
}
std::vector<std::string> EditorAPIWrapper::GetRegisteredLanguages() {
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    return editorApi->GetRegisteredLanguages();
}

std::vector<std::string> EditorAPIWrapper::GetHelp() {
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    auto cmds = editorApi->GetRegisteredCommands();
    std::vector<std::string> cmdHelp;
    cmdHelp.push_back("Currently available plugin commands");
    for (auto &cmd : cmds) {
        char buffer[128];
        snprintf(buffer, 128, "%s (%s) - %s",
                 cmd->GetName().c_str(),
                 cmd->GetShortName().c_str(),
                 cmd->GetDescription().c_str());
        cmdHelp.push_back(buffer);
    }
    return cmdHelp;

}

std::vector<std::string> EditorAPIWrapper::GetRootViewNames() {
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    return editorApi->GetTopViews();
}

ViewAPIWrapper::Ref EditorAPIWrapper::GetViewByName(const char *name) {
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    auto viewRef = editorApi->GetViewByName(name);
    return ViewAPIWrapper::Create(viewRef);

}

//TextBufferAPIWrapper::Ref EditorAPIWrapper::GetActiveTextBuffer() {
//    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
//    return TextBufferAPIWrapper::Create(editorApi->GetActiveTextBuffer());
//}
//
//TextBufferAPI::Ref EditorAPIWrapper::LoadBuffer(const char *name) {
//    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
//    return editorApi->LoadBuffer(name);
//}
//void EditorAPIWrapper::SetActiveBuffer(TextBufferAPI::Ref activeBuffer) {
//    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
//    editorApi->SetActiveBuffer(activeBuffer);
//}
//std::vector<TextBufferAPIWrapper::Ref> EditorAPIWrapper::GetBuffers() {
//    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
//    auto buffers = editorApi->GetBuffers();
//    std::vector<TextBufferAPIWrapper::Ref> bufferWrappers;
//    for(auto &buf : buffers) {
//        bufferWrappers.push_back(std::make_shared<TextBufferAPIWrapper>(buf));
//    }
//    return bufferWrappers;
//}
//


//
// Test stuff
//
std::vector<std::string> EditorAPIWrapper::GetTestArray() {
    static std::vector<std::string> v = {"1","5","21","4"};
    return v;
}




