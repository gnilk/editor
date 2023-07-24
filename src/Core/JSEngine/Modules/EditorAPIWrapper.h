#ifndef GEDIT_EDITORWRAPPERAPI_H
#define GEDIT_EDITORWRAPPERAPI_H

#include "duktape.h"
#include "TextBufferAPIWrapper.h"
#include "ThemeAPIWrapper.h"
#include "ViewAPIWrapper.h"

namespace gedit {
    class EditorAPIWrapper {
    public:
        TextBufferAPIWrapper::Ref GetActiveTextBuffer();
        ThemeAPIWrapper::Ref GetCurrentTheme();


        void ExitEditor();
        std::vector<std::string> GetRegisteredLanguages();
        std::vector<std::string> GetHelp();
        void NewBuffer(const char *name);
        TextBufferAPI::Ref LoadBuffer(const char *name);
        void SetActiveBuffer(TextBufferAPI::Ref activeBuffer);
        std::vector<TextBufferAPIWrapper::Ref> GetBuffers();

        std::vector<std::string> GetRootViewNames();
        ViewAPIWrapper::Ref GetViewByName(const char *name);

        static void RegisterModule(duk_context *ctx);
        // For testing purposes
        std::vector<std::string> GetTestArray();

    public:
    };
}

#endif