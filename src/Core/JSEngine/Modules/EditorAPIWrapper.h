#ifndef GEDIT_EDITORWRAPPERAPI_H
#define GEDIT_EDITORWRAPPERAPI_H

#include "duktape.h"
#include "ThemeAPIWrapper.h"
#include "ViewAPIWrapper.h"
#include "DocumentAPIWrapper.h"

namespace gedit {
    class EditorAPIWrapper {
    public:
        ThemeAPIWrapper::Ref GetCurrentTheme();


        void ExitEditor();
        std::vector<std::string> GetRegisteredLanguages();
        std::vector<std::string> GetHelp();

        DocumentAPIWrapper::Ref NewDocument(const char *name);
        DocumentAPIWrapper::Ref GetActiveDocument();
        std::vector<DocumentAPIWrapper::Ref> GetDocuments();
        void CloseActiveDocument();

        std::vector<std::string> GetRootViewNames();
        ViewAPIWrapper::Ref GetViewByName(const char *name);

        static void RegisterModule(duk_context *ctx);
        // For testing purposes
        std::vector<std::string> GetTestArray();

    public:
    };
}

#endif