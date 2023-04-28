//
// Created by gnilk on 17.03.23.
//

#include <vector>
#include <string>
#include <filesystem>

#include "Editor.h"
#include "Core/BufferManager.h"
#include "Core/RuntimeConfig.h"
#include "Core/Config/Config.h"
#include "Core/KeyMapping.h"
#include "Core/StrUtil.h"

#include "Core/Language/LanguageBase.h"
#include "Core/Language/LanguageSupport/CPPLanguage.h"
#include "Core/Language/LanguageSupport/JSONLanguage.h"

// NCurses backend
#include "Core/NCurses/NCursesScreen.h"
#include "Core/NCurses/NCursesKeyboardDriver.h"

// SDL3 backend
#ifdef GEDIT_USE_SDL3
#include "Core/SDL3/SDLScreen.h"
#include "Core/SDL3/SDLKeyboardDriver.h"
#endif

#ifdef GEDIT_USE_SDL2
#include "Core/SDL3/SDLScreen.h"
#include "Core/SDL3/SDLKeyboardDriver.h"
#endif

// API stuff
#include "Core/API/EditorAPI.h"

using namespace gedit;


Editor &Editor::Instance() {
    static Editor glbSystem;
    return glbSystem;{}
}

bool Editor::Initialize(int argc, const char **argv) {
    if (isInitialized) {
        return true;
    }
    ConfigureLogger();
    // Makes it easier to detect starting in file-appending log-file...
    logger->Debug("*************** EDITOR STARTING ***************");
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

    ConfigureAPI();

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

void Editor::Close() {
    logger->Debug("Closing editor");
    for(auto &model : models) {
        model->Close();
    }
    models.clear();
}

EditorModel::Ref Editor::LoadEditorModelFromFile(const char *filename) {
    logger->Debug("Loading file: %s", filename);
    TextBuffer::Ref textBuffer;

    textBuffer = BufferManager::Instance().NewBufferFromFile(filename);
    if (textBuffer == nullptr) {
        logger->Error("Unable to load file: '%s'", filename);
        return nullptr;
    }
    logger->Debug("End Loading");


    std::filesystem::path pathName(filename);
    auto extension = pathName.extension();
    textBuffer->SetLanguage(Config::Instance().GetLanguageForExtension(extension));

    EditController::Ref editController = std::make_shared<EditController>();
    EditorModel::Ref editorModel = std::make_shared<EditorModel>();

    editorModel->Initialize(editController, textBuffer);

    return editorModel;
}


void Editor::ConfigureLogger() {
    static const char *sinkArgv[]={"autoflush","file","logfile.log"};
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
    auto cppLanguage = CPPLanguage::Create();
    cppLanguage->Initialize();
    Config::Instance().RegisterLanguage(".cpp", cppLanguage);

    auto jsonLanguage = JSONLanguage::Create();
    jsonLanguage->Initialize();
    Config::Instance().RegisterLanguage(".json", jsonLanguage);

}

// FIXME: this is using the kLanguageTokenClass as the color mapping index - this was lazy - and is not good...
void Editor::ConfigureColorTheme() {
    logger->Debug("Configuring colors and theme");
    // NOTE: This must be done after the screen has been opened as the color handling might require the underlying graphics
    //       context to be initialized...
    auto &colorConfig = Config::Instance().GetContentColors();
    for(int i=0;IsLanguageTokenClass(i);i++) {
        auto langClass = gedit::LanguageTokenClassToString(static_cast<kLanguageTokenClass>(i));
        if (!colorConfig.HasColor(langClass)) {
            logger->Warning("Missing color configuration for: %s", langClass.c_str());
        }
        auto &fg = colorConfig.GetColor(langClass);
        auto &bg = colorConfig.GetColor("background");

        // Store this..
        languageColorConfig[static_cast<kLanguageTokenClass>(i)] = std::make_pair(fg, bg);


        logger->Debug("  %d:%s - fg=(%d,%d,%d) bg=(%d,%d,%d)",i,langClass.c_str(),
                      fg.RedAsInt(255),fg.GreenAsInt(255),fg.BlueAsInt(255),
                      bg.RedAsInt(255),bg.GreenAsInt(255),bg.BlueAsInt(255));
        screen->RegisterColor(i, colorConfig.GetColor(langClass), colorConfig.GetColor("background"));
    }
}

void Editor::ConfigureAPI() {
    static EditorAPI editorApi;
    RegisterAPI(0x01, &editorApi);
}

static std::vector<std::string> glbSupportedBackends = {
        {"ncurses"},
        {"sdl"},
};

void Editor::ConfigureSubSystems() {
    auto backend = Config::Instance()["main"].GetStr("backend","ncurses");

    // See if supported, if not - print supported and die...
    // This is one of the few places where we exit..
    if (std::find(glbSupportedBackends.begin(), glbSupportedBackends.end(), backend) == glbSupportedBackends.end()) {
        logger->Error("Unknown backend: '%s'",backend.c_str());
        fprintf(stderr, "Unknown backend: '%s'\n",backend.c_str());
        fprintf(stderr, "Supported: \n");
        for(auto &be : glbSupportedBackends) {
            fprintf(stderr,"  %s\n", be.c_str());
        }
        exit(1);
    }

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
#ifndef GEDIT_LINUX
    if (!keyboardMonitor.Start()) {
        logger->Error("Keyboard monitor failed to start");
        printf("Unable to start keyboard monitor!");
        exit(1);
    }
#endif

    screen = new NCursesScreen();
    auto ncKeyboard = new NCursesKeyboardDriver();
    ncKeyboard->Begin(&keyboardMonitor);
    keyboardDriver = ncKeyboard;
}

void Editor::SetupSDL() {
    screen = new SDLScreen();
    keyboardDriver  = new SDLKeyboardDriver();
    keyboardDriver->Initialize();
}

