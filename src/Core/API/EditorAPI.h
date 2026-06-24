//
// Created by gnilk on 08.04.23.
//

#ifndef EDITOR_EDITORAPI_H
#define EDITOR_EDITORAPI_H

#include <functional>
#include <vector>
#include <string>
#include "Core/Editor.h"
#include "Core/Runloop.h"
#include "Core/Plugins/PluginCommand.h"
#include "ThemeAPI.h"
#include "ViewAPI.h"
#include "DocumentAPI.h"

namespace gedit {
    class EditorAPI {
    public:
        using APIFunc = std::function<void(void)>;
    public:
        EditorAPI() = default;
        virtual ~EditorAPI() = default;
        void ExitEditor() {
            Runloop::StopRunLoop();
        }
        ThemeAPI::Ref GetCurrentTheme();
        std::vector<std::string> GetRegisteredLanguages();
        std::vector<PluginCommand::Ref> GetRegisteredCommands();

        DocumentAPI::Ref GetActiveDocument();
        DocumentAPI::Ref NewDocument(const char *name);
        // Like NewDocument, but pre-fills the new buffer from 'text' (split on '\n'). The seam that
        // lets a plugin drop a terminal block's output - Console.GetSelectedBlock() - into a fresh,
        // editable document (terminal-scrollback §5.5.3 / §9).
        DocumentAPI::Ref NewDocumentFromText(const char *name, const char *text);
        DocumentAPI::Ref LoadDocument(const char *filename);
        std::vector<DocumentAPI::Ref> GetDocuments();
        void CloseActiveDocument();


        const std::vector<std::string> GetTopViews();
        ViewAPI::Ref GetViewByName(const char *name);


    protected:
        APIFunc cbExitEditor = nullptr;

    };
}


#endif //EDITOR_EDITORAPI_H
