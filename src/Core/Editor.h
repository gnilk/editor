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
#include "Core/Graphics/KeyboardDriverBase.h"
#include "Core/Workspace.h"
#include "Core/Document.h"
#include "Core/Workspace.h"
#include "Core/TypeUtil.h"
#include "Core/Controllers/QuickCommandController.h"
#include "Core/KeyMapping.h"
#include "Core/Theme/Theme.h"
#include "ClipBoard.h"
namespace gedit {
    //
    // Global class names for top-views in the view hierarchy...
    //
    static inline const std::string glbWorkSpaceView = "WorkspaceView";
    static inline const std::string glbTerminalView = "TerminalView";
    static inline const std::string glbEditorView = "EditorView";

    //
// This class represents the 'application'
//

    class Editor {
    public:
        typedef enum {
            ViewState,
            QuickCommandState,
        } State;
        using KeymapUpdateDelegate = std::function<void(KeyMapping::Ref newKeymap)>;
    public:
        static Editor &Instance();
        bool Initialize(int argc, const char **argv);
        bool OpenScreen();
        void Close();

        void RunPostInitalizationScript();

        void HandleGlobalAction(const KeyPressAction &kpAction);

        // Open-document accessors. The Workspace owns the open list + active document; these delegate.
        const std::vector<Document::Ref> &GetDocuments() {
            return workspace->GetOpenDocuments();
        }

        void SetActiveDocument(Document::Ref document);
        void SetActiveDocumentFromIndex(size_t idxDocument);
        size_t GetActiveDocumentIndex() {
            return workspace->GetActiveDocumentIndex();
        }
        Document::Ref GetActiveDocument() {
            return workspace->GetActiveDocument();
        }
        Workspace::Node::Ref GetWorkspaceNodeForActiveDocument();
        Workspace::Node::Ref GetWorkspaceNodeForDocument(Document::Ref document);
        Document::Ref GetDocumentFromIndex(size_t idxDocument) {
            return workspace->GetDocumentFromIndex(idxDocument);
        }

        bool IsDocumentOpen(Document::Ref document) {
            return workspace->IsDocumentOpen(document);
        }
        Document::Ref GetDocumentFromTextBuffer(TextBuffer::Ref textBuffer) {
            return workspace->GetDocumentFromTextBuffer(textBuffer);
        }

        size_t NextDocumentIndex(size_t idxCurrent) {
            return workspace->NextDocumentIndex(idxCurrent);
        }

        size_t PreviousDocumentIndex(size_t idxCurrent) {
            return workspace->PreviousDocumentIndex(idxCurrent);
        }

        // FIXME: should return a PluginCommand instead
        JSPluginEngine &GetJSEngine() {
            return jsEngine;
        }

        KeyMapping::Ref GetActiveKeyMap();
        KeyMapping::Ref GetKeyMapForState(State state);
        KeyMapping::Ref GetKeyMapping(const std::string &name);
        bool HasKeyMapping(const std::string &name);
        void SetActiveKeyMapping(const std::string &name);
        void SetKeymapUpdateDelegate(KeymapUpdateDelegate newKeymapDelegate) {
            cbKeymapUpdate = newKeymapDelegate;
        }
        void RestoreViewStateKeymapping();


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
        LanguageBase::Ref GetDefaultLanguage() {
            return defaultLanguage;
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

        const Theme::Ref GetTheme() {
            return theme;
        }

        const std::u32string &GetAppName();
        const std::u32string &GetVersion();


        Document::Ref OpenDocumentFromWorkspace(Workspace::Node::Ref workspaceNode);
        Document::Ref LoadDocument(const std::string &filename);

        bool CloseDocument(Document::Ref document);

        State GetState() {
            return state;
        }

        const QuickCommandController &GetQuickCommandController() {
            return quickCommandController;
        }

        void TriggerUIRedraw();
    protected:
        bool OpenDocumentOrFolder(const std::string &fileOrFolder);

        void ParseArguments(int argc, const char **argv);


        bool TryLoadConfig(const char *configFile);
        void ConfigurePreInitLogger();
        void ConfigureLogger();
        void ConfigureLanguages();
        void ConfigureTheme();
        void ConfigureSubSystems();
        void ConfigureGlobalAPIObjects();
        void ConfigureLogFilter();
        bool CheckCreateDirectory(const std::filesystem::path &path);

        void PrintHelpToConsole();

        // TEMP - backend configuration
        void SetupNCurses();
        void SetupSDL2();
        void SetupSDL3();
        void SetupHeadless();

        void ExecutePostScript(std::istream &stream);
    private:
        Editor() = default;
    private:
        bool isInitialized = false;
        gnilk::ILogger *logger = nullptr;
        bool keepConsoleLogger = false;
        bool loadUserConfig = true;
        std::string argBackend;
        std::vector<std::string> pendingFiles;

        Theme::Ref theme = nullptr;

        ClipBoard clipboard;
        Workspace::Ref workspace = nullptr;
        // Javascript API wrapper
        JSPluginEngine jsEngine;

        // This is probably not the correct place for this - consider moving
        std::unordered_map<kLanguageTokenClass, std::pair<ColorRGBA, ColorRGBA>> languageColorConfig;
        LanguageBase::Ref defaultLanguage = {};
        std::unordered_map<std::string, LanguageBase::Ref> extToLanguages;

        State state = ViewState;
        std::string viewStateKeymapName = "default_keymap";  // This is the default key-map for any 'view' related activity
        KeymapUpdateDelegate cbKeymapUpdate = nullptr;

//        KeyMapping::Ref mappingsForEditState = nullptr;
//        KeyMapping::Ref mappingsForCmdState = nullptr;
        QuickCommandController quickCommandController;  // Special - used when in 'QuickCommandState'

        // Hmm... this should perhaps be runtime config - or some kind of 'API' management code (which I don't have)
        std::unordered_map<std::string_view, void *> editorApiObjects;


    };

}
#endif //EDITOR_EDITOR_H
