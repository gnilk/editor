//
// Created by gnilk on 17.03.23.
//

#include "Editor.h"
#include "Core/BufferManager.h"
#include "Core/RuntimeConfig.h"
#include "Core/Config/Config.h"

#include "Core/Language/LanguageBase.h"
#include "Core/Language/CPP/CPPLanguage.h"

using namespace gedit;


Editor &Editor::Instance() {
    static Editor glbSystem;
    return glbSystem;
}

bool Editor::Initialize(int argc, const char **argv) {
    if (isInitialized) {
        return true;
    }
    ConfigureLogger();

    LoadConfig("config.yml");

    ConfigureLanguages();
    ConfigureSubSystems();
    ConfigureColorTheme();

    // --
    // Encapsulate this

    if (argc > 1) {
        for(int i=1;i<argc;i++) {
            auto model = LoadEditorModelFromFile(argv[i]);
            models.push_back(model);
        }
    } else {
        EditController::Ref editController = std::make_shared<EditController>();
        auto textBuffer = BufferManager::Instance().NewBuffer("no_name");

        EditorModel::Ref editorModel = std::make_shared<EditorModel>();
        editorModel->Initialize(editController, textBuffer);
        models.push_back(editorModel);
    }

    RuntimeConfig::Instance().SetActiveEditorModel(models[0]);

    isInitialized = true;
    return true;
}

EditorModel::Ref Editor::LoadEditorModelFromFile(const char *filename) {
    logger->Debug("Loading file: %s", filename);
    TextBuffer::Ref textBuffer;
    EditController::Ref editController = std::make_shared<EditController>();

    textBuffer = BufferManager::Instance().NewBufferFromFile(filename);
    textBuffer->SetLanguage(Config::Instance().GetLanguageForFilename(filename));

    EditorModel::Ref editorModel = std::make_shared<EditorModel>();
    editorModel->Initialize(editController, textBuffer);

    return editorModel;
}


void Editor::ConfigureLogger() {
    char *sinkArgv[]={"autoflush","file","logfile.log"};
    gnilk::Logger::Initialize();
    auto fileSink = new gnilk::LogFileSink();
    gnilk::Logger::AddSink(fileSink, "fileSink", 3, sinkArgv);
    // Remove the console sink (it is auto-created in debug-mode)
    gnilk::Logger::RemoveSink("console");

    logger = gnilk::Logger::GetLogger("System");
}

bool Editor::LoadConfig(const char *configFile) {
    logger->Debug("Loading configuration");
    auto configOk = Config::Instance().LoadConfig(configFile);
    if (!configOk) {
        logger->Error("Unable to load default configuration from 'config.yml' - defaults will be used");
        return false;
    }
    return true;
}

void Editor::ConfigureLanguages() {
    logger->Debug("Configuring language parser(s)");
    auto cppLanguage = new CPPLanguage();
    cppLanguage->Initialize();
    Config::Instance().RegisterLanguage(".cpp", cppLanguage);
}

void Editor::ConfigureColorTheme() {
    logger->Debug("Configuring colors and theme");
    // NOTE: This must be done after the screen has been opened as the color handling might require the underlying graphics
    //       context to be initialized...
    auto &colorConfig = Config::Instance().ColorConfiguration();
    for(int i=0;gnilk::IsLanguageTokenClass(i);i++) {
        auto langClass = gnilk::LanguageTokenClassToString(static_cast<kLanguageTokenClass>(i));
        if (!colorConfig.HasColor(langClass)) {
            logger->Warning("Missing color configuration for: %s", langClass.c_str());
        }
        screen.RegisterColor(i, colorConfig.GetColor(langClass), colorConfig.GetColor("background"));
    }
}

void Editor::ConfigureSubSystems() {
    if (!keyboardMonitor.Start()) {
        logger->Error("Keyboard monitor failed to start");
        exit(1);
    }
    keyboardDriver.Begin(&keyboardMonitor);

    RuntimeConfig::Instance().SetScreen(screen);
    RuntimeConfig::Instance().SetKeyboard(keyboardDriver);

    logger->Debug("Initialize Graphics subsystem");

    screen.Open();
    screen.Clear();
}
