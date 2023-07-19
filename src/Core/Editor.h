//
// Created by gnilk on 17.03.23.
//

#ifndef EDITOR_EDITOR_H
#define EDITOR_EDITOR_H

#include <vector>

#include "logger.h"
#include "Core/macOS/MacOSKeyboardMonitor.h"

#include "Core/JSEngine/JSPluginEngine.h"
#include "Core/Language/LanguageBase.h"
#include "Core/RuntimeConfig.h"
#include "KeyboardDriverBase.h"
#include "KeyboardBaseMonitor.h"
#include "Core/Workspace.h"
#include "ScreenBase.h"
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

        size_t GetActiveModelIndex() {
            for(size_t i=0; i < openModels.size(); i++) {
                if (openModels[i]->IsActive()) {
                    return i;
                }
            }
            logger->Error("No active model!!!!!");
            // THIS SHOULD NOT HAPPEN!!!
            return 0;
        }
        EditorModel::Ref GetActiveModel() {
            for(size_t i=0; i < openModels.size(); i++) {
                if (openModels[i]->IsActive()) {
                    return openModels[i];
                }
            }
            logger->Error("No active model!!!!!");
            // THIS SHOULD NOT HAPPEN!!!
            return nullptr;
        }

        void SetActiveModel(EditorModel::Ref model);
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

        EditorModel::Ref GetModelFromIndex(size_t idxModel) {
            if (idxModel > (openModels.size() - 1)) {
                return nullptr;
            }
            return openModels[idxModel];
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
        std::unordered_map<kLanguageTokenClass, std::pair<ColorRGBA, ColorRGBA>> languageColorConfig;

        State state = EditState;

#ifdef GEDIT_MACOS
        // This depends on the OS/Backend - consider creating a platform layer or something to handle this...
        MacOSKeyboardMonitor keyboardMonitor;
#else
        KeyboardBaseMonitor keyboardMonitor;
#endif
        ScreenBase *screen = nullptr;
        KeyboardDriverBase *keyboardDriver = nullptr;
        std::unordered_map<std::string_view, void *> editorApiObjects;

        ClipBoard clipboard;
        Workspace::Ref workspace;
        // Javascript API wrapper
        JSPluginEngine jsEngine;
        QuickCommandController quickCommandController;

        LanguageBase::Ref defaultLanguage = {};
        std::unordered_map<std::string, LanguageBase::Ref> extToLanguages;

        KeyMapping mappingsForEditState;
        KeyMapping mappingsForCmdState;

    };

}
#endif //EDITOR_EDITOR_H
