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

#include "KeyboardDriverBase.h"
#include "KeyboardBaseMonitor.h"

#include "ScreenBase.h"
#include "Core/EditorModel.h"
#include "Core/TypeUtil.h"

namespace gedit {
//
// The global editing class...
//
    class Editor {
    public:
        static Editor &Instance();
        bool Initialize(int argc, const char **argv);
        bool OpenScreen();
        void Close();
        bool LoadConfig(const char *configFile);
        std::vector<EditorModel::Ref> &GetModels() {
            return models;
        }
        size_t GetActiveModelIndex() {
            for(size_t i=0;i<models.size();i++) {
                if (models[i]->IsActive()) {
                    return i;
                }
            }
            auto logger = gnilk::Logger::GetLogger("Editor");
            logger->Error("No active model!!!!!");
            // THIS SHOULD NOT HAPPEN!!!
            return 0;
        }

        void SetActiveModel(TextBuffer::Ref textBuffer) {
            auto idxCurrent = GetActiveModelIndex();
            for(size_t i = 0; i<models.size();i++) {
                if (models[i]->GetTextBuffer() == textBuffer) {
                    models[idxCurrent]->SetActive(false);
                    models[i]->SetActive(true);
                    return;
                }
            }
        }

        size_t NextModelIndex(size_t idxCurrent) {
            auto next = (idxCurrent + 1) % models.size();
            return next;
        }
        EditorModel::Ref GetModelFromIndex(size_t idxModel) {
            if (idxModel > (models.size()-1)) {
                return nullptr;
            }
            return models[idxModel];
        }

        // FIXME: should return a PluginCommand instead
        JSPluginEngine &GetJSEngine() {
            return jsEngine;
        }


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

        bool NewBuffer(const std::string &name);
        // Returns the index to the model if loaded or negative if failed
        int LoadBuffer(const std::string &filename);

        void ConfigureLogger();
    protected:
        void ConfigureLanguages();
        void ConfigureColorTheme();
        void ConfigureSubSystems();
        void ConfigureGlobalAPIObjects();

        // TEMP - backend configuration
        void SetupNCurses();
        void SetupSDL();


        EditorModel::Ref LoadEditorModelFromFile(const char *filename);
        EditorModel::Ref NewModel(const char *name);

    private:
        Editor() = default;
    private:
        bool isInitialized = false;
        gnilk::ILogger *logger = nullptr;
        std::vector<EditorModel::Ref> models;   // rename..

        std::unordered_map<kLanguageTokenClass, std::pair<ColorRGBA, ColorRGBA>> languageColorConfig;

#ifdef GEDIT_MACOS
        // This depends on the OS/Backend - consider creating a platform layer or something to handle this...
        MacOSKeyboardMonitor keyboardMonitor;
#else
        KeyboardBaseMonitor keyboardMonitor;
#endif
        ScreenBase *screen = nullptr;
        KeyboardDriverBase *keyboardDriver = nullptr;
        std::unordered_map<std::string_view, void *> editorApiObjects;

        // Javascript API wrapper
        JSPluginEngine jsEngine;

        LanguageBase::Ref defaultLanguage = {};
        std::unordered_map<std::string, LanguageBase::Ref> extToLanguages;

    };

}
#endif //EDITOR_EDITOR_H
