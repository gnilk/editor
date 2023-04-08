//
// Created by gnilk on 17.03.23.
//

#ifndef EDITOR_EDITOR_H
#define EDITOR_EDITOR_H

#include <vector>

#include "logger.h"
#include "Core/macOS/MacOSKeyboardMonitor.h"

#include "KeyboardDriverBase.h"
#include "KeyboardBaseMonitor.h"

#include "ScreenBase.h"
#include "Core/EditorModel.h"

namespace gedit {
//
// The global editing class...
//
    class Editor {
    public:
        static Editor &Instance();
        bool Initialize(int argc, const char **argv);
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
        // API Object Handling
        void RegisterAPI(int id, void *apiObject) {
            editorApiObjects.insert({id, apiObject});
        }
        template<class T>
        T *GetAPI(int id) {
            auto apiObject = editorApiObjects[id];
            return static_cast<T *>(apiObject);
        }
    protected:
        void ConfigureLogger();
        void ConfigureLanguages();
        void ConfigureColorTheme();
        void ConfigureSubSystems();

        void ConfigureAPI();

        // TEMP - backend configuration
        void SetupNCurses();
        void SetupSDL();


        EditorModel::Ref LoadEditorModelFromFile(const char *filename);
    private:
        Editor() = default;
    private:
        bool isInitialized = false;
        gnilk::ILogger *logger = nullptr;
        std::vector<EditorModel::Ref> models;   // rename..
#ifdef GEDIT_MACOS
        // This depends on the OS/Backend - consider creating a platform layer or something to handle this...
        MacOSKeyboardMonitor keyboardMonitor;
#else
        KeyboardBaseMonitor keyboardMonitor;
#endif
        ScreenBase *screen = nullptr;
        KeyboardDriverBase *keyboardDriver = nullptr;

        std::unordered_map<int, void *> editorApiObjects;

    };

}
#endif //EDITOR_EDITOR_H
