//
// Created by gnilk on 17.03.23.
//

#ifndef EDITOR_EDITOR_H
#define EDITOR_EDITOR_H

#include <vector>

#include "logger.h"

#include "Core/JSEngine/JSPluginEngine.h"
#include "Core/Language/LanguageBase.h"
#include "Core/RuntimeConfig.h"
#include "KeyboardDriverBase.h"
#include "Core/Workspace.h"
#include "Core/EditorModel.h"
#include "Core/Workspace.h"
#include "Core/TypeUtil.h"
#include "Core/Controllers/QuickCommandController.h"
#include "Core/KeyMapping.h"
#include "ClipBoard.h"
namespace gedit {
//
// This class represents the 'application'
//
    class Editor {
    public:
        typedef enum {
            EditState,
            QuickCommandState,
        } State;
    public:
        static Editor &Instance();
        bool Initialize(int argc, const char **argv);
        bool OpenScreen();
        void Close();

        void RunPostInitalizationScript();

        void HandleGlobalAction(const KeyPressAction &kpAction);

        bool LoadConfig(const char *configFile);

        // Move to 'workspace'
        std::vector<EditorModel::Ref> &GetModels() {
            return openModels;
        }

        void SetActiveModel(EditorModel::Ref model);
        void SetActiveModelFromIndex(size_t idxModel);
        size_t GetActiveModelIndex();
        EditorModel::Ref GetActiveModel();
        EditorModel::Ref GetModelFromIndex(size_t idxModel) {
            if (idxModel > (openModels.size() - 1)) {
                return nullptr;
            }
            return openModels[idxModel];
        }

        bool IsModelOpen(EditorModel::Ref model);
        EditorModel::Ref GetModelFromTextBuffer(TextBuffer::Ref textBuffer);

        size_t NextModelIndex(size_t idxCurrent) {
            auto next = (idxCurrent + 1) % openModels.size();
            return next;
        }

        size_t PreviousModelIndex(size_t idxCurrent) {
            auto next = (openModels.size() + (idxCurrent - 1)) % openModels.size();
            return next;
        }


        // -- End move to workspace

        // FIXME: should return a PluginCommand instead
        JSPluginEngine &GetJSEngine() {
            return jsEngine;
        }

        KeyMapping &GetActiveKeyMap();
        KeyMapping &GetKeyMapForState(State state);
        void LeaveCommandMode();

        std::pair<ColorRGBA, ColorRGBA> ColorFromLanguageToken(kLanguageTokenClass tokenClass) {
            if (languageColorConfig.find(tokenClass) == languageColorConfig.end()) {
                return {};
            }
            return languageColorConfig[tokenClass];
        }

        // API Object Handling for static/global objects
        // Specific instances (like TextBufferAPI) should be aquired through one of the global API objects
        // Example: auto currentTextBuffer = GetGlobalAPIObject<EditorAPI>()->GetActiveTextBuffer();
        template<class T>
        void RegisterGlobalAPIObject(void *apiObject) {
            auto typeName = gedit::type_name<T>();
            editorApiObjects.insert({typeName, apiObject});
        }
        template<class T>
        T *GetGlobalAPIObject() {
            auto typeName = gedit::type_name<T>();
            auto apiObject = editorApiObjects[typeName];
            return static_cast<T *>(apiObject);
        }

        void SetDefaultLanguage(LanguageBase::Ref newDefaultLanguage) {
            defaultLanguage = newDefaultLanguage;
        }
        void RegisterLanguage(const std::string &extension, LanguageBase::Ref languageBase);
        LanguageBase::Ref GetLanguageForExtension(const std::string &extension);
        std::vector<std::string> GetRegisteredLanguages();


        const Workspace::Ref GetWorkspace() {
            return workspace;
        }

        ClipBoard &GetClipBoard() {
            return clipboard;
        }


        EditorModel::Ref OpenModelFromWorkspace(Workspace::Node::Ref workspaceNode);
        EditorModel::Ref NewModel(const std::string &name);
        EditorModel::Ref LoadModel(const std::string &filename);

        bool CloseModel(EditorModel::Ref model);

        State GetState() {
            return state;
        }

        const QuickCommandController &GetQuickCommandController() {
            return quickCommandController;
        }
        void ConfigureLogger();

        void TriggerUIRedraw();
    protected:
        void ConfigureLanguages();
        void ConfigureColorTheme();
        void ConfigureKeyMappings();
        void ConfigureSubSystems();
        void ConfigureGlobalAPIObjects();

        // TEMP - backend configuration
        void SetupNCurses();
        void SetupSDL();

        void ExecutePostScript(const std::string &scriptFile);
    private:
        Editor() = default;
    private:
        bool isInitialized = false;
        gnilk::ILogger *logger = nullptr;

        // Holds all open models/buffers in the text editor
        std::vector<EditorModel::Ref> openModels = {};

        ClipBoard clipboard;
        Workspace::Ref workspace = nullptr;
        // Javascript API wrapper
        JSPluginEngine jsEngine;

        // This is probably not the correct place for this - consider moving
        std::unordered_map<kLanguageTokenClass, std::pair<ColorRGBA, ColorRGBA>> languageColorConfig;
        LanguageBase::Ref defaultLanguage = {};
        std::unordered_map<std::string, LanguageBase::Ref> extToLanguages;

        State state = EditState;
        KeyMapping mappingsForEditState;
        KeyMapping mappingsForCmdState;
        QuickCommandController quickCommandController;  // Special - used when in 'QuickCommandState'

        // Hmm... this should perhaps be runtime config - or some kind of 'API' management code (which I don't have)
        std::unordered_map<std::string_view, void *> editorApiObjects;


    };

}
#endif //EDITOR_EDITOR_H
