//
// Created by gnilk on 17.03.23.
//

#include "Editor.h"
#include "Core/BufferManager.h"
#include "Core/RuntimeConfig.h"
#include "Core/Config/Config.h"
#include "Core/KeyMapping.h"
#include "Core/StrUtil.h"

#include "Core/Language/LanguageBase.h"
#include "Core/Language/CPP/CPPLanguage.h"

// NCurses backend
#include "Core/NCurses/NCursesScreen.h"
#include "Core/NCurses/NCursesKeyboardDriver.h"

// SDL3 backend
#include "Core/SDL3/SDLScreen.h"
#include "Core/SDL3/SDLKeyboardDriver.h"

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

    // Language configuration must currently be done before we load editor models
    ConfigureLanguages();


    for(int i=1;i<argc;i++) {
        if (strutil::startsWith(argv[i], "--")) {
            std::string cmdSwitch = std::string(&argv[i][2]);
            if (cmdSwitch == "backend") {
                auto strBackend = argv[++i];
                Config::Instance()["main"].SetStr("backend",strBackend);
            } else {
                printf("Error: Unknown command line option: %s\n", argv[i]);
                exit(1);
            }
        } else {
            auto model = LoadEditorModelFromFile(argv[i]);
            if (model != nullptr) {
                models.push_back(model);
            }
        }
    }


    ConfigureSubSystems();
    ConfigureColorTheme();

    if (models.size() == 0) {
        EditController::Ref editController = std::make_shared<EditController>();
        auto textBuffer = BufferManager::Instance().NewBuffer("no_name");

        EditorModel::Ref editorModel = std::make_shared<EditorModel>();
        editorModel->Initialize(editController, textBuffer);
        models.push_back(editorModel);
    }

    RuntimeConfig::Instance().SetActiveEditorModel(models[0]);

    bool keyMapperOk = KeyMapping::Instance().IsInitialized();
    if (!keyMapperOk) {
        logger->Error("KeyMapper failed to initalize");
        return false;
    }


    isInitialized = true;
    return true;
}

EditorModel::Ref Editor::LoadEditorModelFromFile(const char *filename) {
    logger->Debug("Loading file: %s", filename);
    TextBuffer::Ref textBuffer;
    EditController::Ref editController = std::make_shared<EditController>();

    textBuffer = BufferManager::Instance().NewBufferFromFile(filename);
    if (textBuffer == nullptr) {
        logger->Error("Unable to load file: '%s'", filename);
        return nullptr;
    }
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
        auto &fg = colorConfig.GetColor(langClass);
        auto &bg = colorConfig.GetColor("background");
        logger->Debug("  %d:%s - fg=(%d,%d,%d) bg=(%d,%d,%d)",i,langClass.c_str(),
                      fg.RedAsInt(255),fg.GreenAsInt(255),fg.BlueAsInt(255),
                      bg.RedAsInt(255),bg.GreenAsInt(255),bg.BlueAsInt(255));
        screen->RegisterColor(i, colorConfig.GetColor(langClass), colorConfig.GetColor("background"));
    }
}

void Editor::ConfigureSubSystems() {
    auto backend = Config::Instance()["main"].GetStr("backend","ncurses");

    if (backend == "ncurses") {
        SetupNCurses();
    } else if (backend == "sdl") {
        SetupSDL();
    }


    RuntimeConfig::Instance().SetScreen(*screen);
    RuntimeConfig::Instance().SetKeyboard(*keyboardDriver);

    logger->Debug("Initialize Graphics subsystem");

    screen->Open();
    screen->Clear();
}
void Editor::SetupNCurses() {
    if (!keyboardMonitor.Start()) {
        logger->Error("Keyboard monitor failed to start");
        exit(1);
    }

    screen = new NCursesScreen();
    auto ncKeyboard = new NCursesKeyboardDriver();
    ncKeyboard->Begin(&keyboardMonitor);
    keyboardDriver = ncKeyboard;
}
void Editor::SetupSDL() {
    screen = new SDLScreen();
    keyboardDriver  = new SDLKeyboardDriver();
}
