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
#include "Core/Language/LanguageSupport/DefaultLanguage.h"

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
    return glbSystem;
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

    // Parse cmd-line
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
            // FIXME: THIS IS NOT WORKING WITH WORKSPACE!!!
            auto model = LoadEditorModelFromFile(argv[i]);
            if (model != nullptr) {
                openModels.push_back(model);
            }
        }
    }

    ConfigureGlobalAPIObjects();
    // Open currently working folder...
    workspace = Workspace::Create();

    workspace->OpenFolder("Plugins");


    // create a model if cmd-line didn't specify any
    // this will cause editor to start with at least one new file...
    if (openModels.size() == 0) {
        // Default workspace will be created if not already..
        workspace->GetDefaultWorkspace();
        auto model = workspace->NewEmptyModel();
        openModels.push_back(model);
    }


    // Activate the first loaded file (or empty/new model)
    RuntimeConfig::Instance().SetActiveEditorModel(openModels[0]);

    bool keyMapperOk = KeyMapping::Instance().IsInitialized();
    if (!keyMapperOk) {
        logger->Error("KeyMapper failed to initalize");
        return false;
    }

    isInitialized = true;
    return true;
}
bool Editor::OpenScreen() {
    ConfigureSubSystems();
    ConfigureColorTheme();
    return true;
}

void Editor::Close() {
    logger->Debug("Closing editor");
    for(auto &model : openModels) {
        model->Close();
    }
    openModels.clear();
}

EditorModel::Ref Editor::OpenModelFromWorkspace(Workspace::Node::Ref workspaceNode) {
    auto model = workspaceNode->GetModel();
    if (IsModelOpen(model)) {
        SetActiveModel(model);
        return model;
    }


    logger->Debug("OpenModelFromWorkspace, new model selected: %s", model->GetTextBuffer()->GetName().c_str());
    // Make sure we load it if not yet done...
    if (model->GetTextBuffer()->GetBufferState() == TextBuffer::kBuffer_FileRef) {
        model->GetTextBuffer()->Load();
    }
    openModels.push_back(model);
    logger->Debug("Activating new model");
    SetActiveModel(model);

    return model;;
}



// Create a new model/buffer
EditorModel::Ref Editor::NewModel(const std::string &name) {
    auto model = workspace->NewEmptyModel();
    model->GetTextBuffer()->SetPathName(name);
    openModels.push_back(model);
    return model;
}

EditorModel::Ref Editor::LoadModel(const std::string &filename) {
    auto model = workspace->NewModelWithFileRef(filename);
    model->GetTextBuffer()->Load();
    openModels.push_back(model);

    return model;
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
    textBuffer->SetLanguage(GetLanguageForExtension(extension));

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
    if (logger == nullptr) {
        logger = gnilk::Logger::GetLogger("System");
    }
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
    RegisterLanguage(".cpp|.h|.c|.hpp", cppLanguage);

    auto jsonLanguage = JSONLanguage::Create();
    jsonLanguage->Initialize();
    RegisterLanguage(".json|.js", jsonLanguage);

    auto defaultLanguage = DefaultLanguage::Create();
    defaultLanguage->Initialize();
    RegisterLanguage("default", defaultLanguage);
    SetDefaultLanguage(defaultLanguage);

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

// Configure global/static API objects..
void Editor::ConfigureGlobalAPIObjects() {
    static EditorAPI editorApi;
//    static TextBufferAPI textBufferAPI;

    RegisterGlobalAPIObject<EditorAPI>(&editorApi);
//    RegisterAPI<TextBufferAPI>(&textBufferAPI);

    // Initialize the Javascript wrapper engine...
    jsEngine.Initialize();
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

void Editor::RegisterLanguage(const std::string &extension, LanguageBase::Ref languageBase) {
    std::vector<std::string> extensionList;
    strutil::split(extensionList, extension.c_str(), '|');
    for(auto &ext : extensionList) {
        extToLanguages[ext] = languageBase;
    }
}

LanguageBase::Ref Editor::GetLanguageForExtension(const std::string &extension) {

    if (extToLanguages.find(extension) == extToLanguages.end()) {
        return defaultLanguage;
    }
    return extToLanguages[extension];
}

std::vector<std::string> Editor::GetRegisteredLanguages() {
    std::vector<std::string> keys;
    for(auto &[key,value] : extToLanguages) {
        keys.push_back(key);
    }
    return keys;
}


void Editor::SetActiveModel(EditorModel::Ref model) {
    auto idxCurrent = GetActiveModelIndex();
    for(size_t i = 0; i < openModels.size(); i++) {
        if (openModels[i] == model) {
            openModels[idxCurrent]->SetActive(false);
            openModels[i]->SetActive(true);
            // THIS IS NOT A WORK OF BEAUTY
            RuntimeConfig::Instance().SetActiveEditorModel(openModels[i]);

            if (RuntimeConfig::Instance().HasRootView()) {
                RuntimeConfig::Instance().GetRootView().Initialize();
            }
            return;
        }
    }
}


bool Editor::IsModelOpen(EditorModel::Ref model) {
    auto idxCurrent = GetActiveModelIndex();
    for(size_t i = 0; i < openModels.size(); i++) {
        if (openModels[i] == model) {
            return true;
        }
    }
    return false;
}

EditorModel::Ref Editor::GetModelFromTextBuffer(TextBuffer::Ref textBuffer) {
    for(size_t i = 0; i < openModels.size(); i++) {
        if (openModels[i]->GetTextBuffer() == textBuffer) {
            return openModels[i];
        }
    }
    return nullptr;
}

