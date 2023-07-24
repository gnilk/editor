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
#include "TextBufferAPI.h"
#include "ThemeAPI.h"
#include "ViewAPI.h"

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
        TextBufferAPI::Ref GetActiveTextBuffer();
        ThemeAPI::Ref GetCurrentTheme();
        std::vector<std::string> GetRegisteredLanguages();
        std::vector<PluginCommand::Ref> GetRegisteredCommands();

        TextBufferAPI::Ref NewBuffer(const char *name);
        TextBufferAPI::Ref LoadBuffer(const char *filename);

        const std::vector<std::string> GetTopViews();
        ViewAPI::Ref GetViewByName(const char *name);


        std::vector<TextBufferAPI::Ref> GetBuffers();

        void SetActiveBuffer(TextBufferAPI::Ref activeBuffer);
    protected:
        APIFunc cbExitEditor = nullptr;

    };
}


#endif //EDITOR_EDITORAPI_H
