#ifndef GEDIT_EDITORWRAPPERAPI_H
#define GEDIT_EDITORWRAPPERAPI_H

#include "duktape.h"
#include "dukglue/dukvalue.h"
#include "ThemeAPIWrapper.h"
#include "ViewAPIWrapper.h"
#include "DocumentAPIWrapper.h"

namespace gedit {
    class EditorAPIWrapper {
    public:
        ThemeAPIWrapper::Ref GetCurrentTheme();


        void ExitEditor();
        std::vector<std::string> GetRegisteredLanguages();
        std::vector<std::string> GetLanguageDescriptions();
        std::vector<std::string> GetHelp();
        // Editor.GetAgentInfo() -> [lines] - the agent main.ai_integration picks (hitlist #78)
        std::vector<std::string> GetAgentInfo();
        // Editor.StartEditorAgent() -> line - the Editor Agent's tab ('.agent start')
        std::string StartEditorAgent();
        // Editor.SetEditorAgentModel(name) -> [lines] - '.agent model'; '' says which it is
        std::vector<std::string> SetEditorAgentModel(std::string name);
        // Editor.AskEditorAgent(text) -> [lines] - '.ask'
        std::vector<std::string> AskEditorAgent(std::string text);

        DocumentAPIWrapper::Ref GetActiveDocument();
        DocumentAPIWrapper::Ref NewDocument(const char *name);
        DocumentAPIWrapper::Ref NewDocumentFromText(const char *name, const char *text);
        DocumentAPIWrapper::Ref LoadDocument(const char *filename);
        std::vector<DocumentAPIWrapper::Ref> GetDocuments();
        void CloseActiveDocument();
        void CloseAllDocuments();
        void CopyToClipboard(const char *text);
        void Confirm(const char *message, std::vector<std::string> choices, DukValue onResult);

        std::vector<std::string> GetRootViewNames();
        ViewAPIWrapper::Ref GetViewByName(const char *name);

        static void RegisterModule(duk_context *ctx);
        // For testing purposes
        std::vector<std::string> GetTestArray();

    public:
    };
}

#endif