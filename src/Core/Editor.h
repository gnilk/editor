//
// Created by gnilk on 17.03.23.
//

#ifndef EDITOR_EDITOR_H
#define EDITOR_EDITOR_H

#include <vector>

#include "Core/NCurses/NCursesScreen.h"
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
    protected:
        void ConfigureLogger();
        void ConfigureLanguages();
        void ConfigureColorTheme();
        void ConfigureSubSystems();

        EditorModel::Ref LoadEditorModelFromFile(const char *filename);
    private:
        Editor() = default;
    private:
        bool isInitialized = false;
        gnilk::ILogger *logger = nullptr;
        std::vector<EditorModel::Ref> models;   // rename..
        // This depends on the OS/Backend - consider creating a platform layer or something to handle this...
        NCursesScreen screen;
        MacOSKeyboardMonitor keyboardMonitor;
        NCursesKeyboardDriver keyboardDriver;
    };

}
#endif //EDITOR_EDITOR_H
