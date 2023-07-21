//
// Created by gnilk on 17.03.23.
//

#include <vector>
#include <string>
#include <filesystem>

#include "Editor.h"
#include "Core/RuntimeConfig.h"
#include "Core/Config/Config.h"
#include "Core/KeyMapping.h"
#include "Core/StrUtil.h"

#include "Core/Language/LanguageBase.h"
#include "Core/Language/LanguageSupport/CPPLanguage.h"
#include "Core/Language/LanguageSupport/JSONLanguage.h"
#include "Core/Language/LanguageSupport/DefaultLanguage.h"
#include "Core/Plugins/PluginExecutor.h"
#include <fstream>

// NCurses backend
#include "Core/NCurses/NCursesScreen.h"
#include "Core/NCurses/NCursesKeyboardDriver.h"

// SDL3 backend
#ifdef GEDIT_USE_SDL3
#include "Core/SDL3/SDLScreen.h"
#include "Core/SDL3/SDLKeyboardDriver.h"
#endif

#ifdef GEDIT_USE_SDL2
#include "Core/SDL2/SDLScreen.h"
#include "Core/SDL2/SDLKeyboardDriver.h"
#endif

#include <sys/stat.h>
#include <wordexp.h>

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

    // When we have the config we can set up the log-filter...
    ConfigureLogFilter();

    // Language configuration must currently be done before we load editor models
    ConfigureLanguages();

    // Create workspace
    workspace = Workspace::Create();

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
            if (!LoadModel(argv[i])) {
                printf("Error: No such file '%s'\n", argv[i]);
            }
        }
    }

    ConfigureGlobalAPIObjects();

    // FIXME: Remove this..
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
    //RuntimeConfig::Instance().SetActiveEditorModel(openModels[0]);
    SetActiveModel(openModels[0]);

    auto editKeyMap = GetKeyMapForState(Editor::State::ViewState);
    if (editKeyMap == nullptr) {
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

void Editor::RunPostInitalizationScript() {

    auto scriptFiles = Config::Instance()["main"].GetSequenceOfStr("bootstrap_scripts");
    for(auto &scriptFile : scriptFiles) {
        wordexp_t exp_result;
        wordexp(scriptFile.c_str(), &exp_result, 0);
        std::string strScript(exp_result.we_wordv[0]);
        wordfree(&exp_result);

        logger->Debug("Trying: %s", strScript.c_str());
        // Verify if shell exists...
        struct stat scriptStat = {};
        if (stat(strScript.c_str(),&scriptStat)) {
            logger->Error("Can't find bootstrap script '%s' - please verify path\n", strScript.c_str());
            continue;
        }
        logger->Debug("Ok, loading and executing '%s'", strScript.c_str());
        ExecutePostScript(strScript);
    }
}

void Editor::ExecutePostScript(const std::string &scriptFile) {
    std::ifstream f(scriptFile);
    auto comment = Config::Instance()["main"].GetStr("bootstrap_script_comment", "//");

    while(!f.eof()) {
        char buffer[128];
        f.getline(buffer, 128);
        std::string strcmd(buffer);
        if (!strcmd.empty()) {
            if (!strutil::startsWith(strcmd, comment)) {
                PluginExecutor::ParseAndExecuteWithCmdPrefix(strcmd);
            }
        }
    }
}

void Editor::HandleGlobalAction(const KeyPressAction &kpAction) {
    logger->Debug("Handling global actions!!");
    if (state == ViewState) {
        if (kpAction.action == kAction::kActionEnterCommandMode) {
            logger->Debug("Entering command mode!");
            state = QuickCommandState;
            quickCommandController.Enter();
        }
    } else if (state == QuickCommandState) {
        if (kpAction.action == kAction::kActionLeaveCommandMode) {
            LeaveCommandMode();
            RestoreViewStateKeymapping();
        }
    }
}

void Editor::LeaveCommandMode() {
    logger->Debug("Leaving command mode!");
    quickCommandController.Leave();
    state = ViewState;
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

bool Editor::CloseModel(EditorModel::Ref model) {
    return workspace->CloseModel(model);
}

// Must be called before 'LoadConfig' - as the config loader will output debug info...
void Editor::ConfigureLogger() {
    static const char *sinkArgv[]={"autoflush","file","logfile.log"};
    gnilk::Logger::Initialize();
    auto fileSink = new gnilk::LogFileSink();
    gnilk::Logger::AddSink(fileSink, "fileSink", 3, sinkArgv);
    // Remove the console sink (it is auto-created in debug-mode)
    gnilk::Logger::RemoveSink("console");
    logger = gnilk::Logger::GetLogger("System");

}

// This must be called after 'LoadConfig' - part of initialization process
void Editor::ConfigureLogFilter() {
    if (Config::Instance()["logging"].GetBool("disable_all", false)) {
        gnilk::Logger::DisableAllLoggers();
    }

    auto loggersEnabled = Config::Instance()["logging"].GetSequenceOfStr("enable_modules");
    for(auto &logName : loggersEnabled) {
        gnilk::Logger::EnableLogger(logName.c_str());
    }
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

    {
        auto language = JSONLanguage::Create();
        language->Initialize();
        RegisterLanguage(".json|.js", language);
    }

    {
        auto language = DefaultLanguage::Create();
        language->Initialize();
        RegisterLanguage("default", language);
        SetDefaultLanguage(language);
    }

}

void Editor::ConfigureColorTheme() {
    logger->Debug("Configuring colors and theme");
    // NOTE: This must be done after the screen has been opened as the color handling might require the underlying graphics
    //       context to be initialized...
    auto &colorConfig = Config::Instance().GetContentColors();
    auto screen = RuntimeConfig::Instance().GetScreen();
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

    RegisterGlobalAPIObject<EditorAPI>(&editorApi);

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

    logger->Debug("Initialize Graphics Backend - %s", backend.c_str());

    if (backend == "ncurses") {
        SetupNCurses();
    } else if (backend == "sdl") {
        SetupSDL();
    }
}

void Editor::SetupNCurses() {

    auto screenDriver = NCursesScreen::Create();
    auto kbDriver = NCursesKeyboardDriver::Create();
    RuntimeConfig::Instance().SetKeyboard(kbDriver);
    RuntimeConfig::Instance().SetScreen(screenDriver);

    screenDriver->Open();
    screenDriver->Clear();
}

void Editor::SetupSDL() {
    auto keyDriver = SDLKeyboardDriver::Create();
    if (keyDriver == nullptr) {
        logger->Error("Failed to initalize SDL Keyboard driver!");
        printf("Failed to initalize SDL Keyboard driver!\n");
        exit(1);
    }

    auto screenDriver = SDLScreen::Create();

    RuntimeConfig::Instance().SetKeyboard(keyDriver);
    RuntimeConfig::Instance().SetScreen(screenDriver);

    screenDriver->Open();
    screenDriver->Clear();
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
    auto currentModel = GetActiveModel();
    for(size_t i = 0; i < openModels.size(); i++) {
        if (openModels[i] == model) {
            if (currentModel != nullptr) {
                currentModel->SetActive(false);
            }
            openModels[i]->SetActive(true);

            if (RuntimeConfig::Instance().HasRootView()) {
                RuntimeConfig::Instance().GetRootView().Initialize();
            }
            return;
        }
    }
}

void Editor::SetActiveModelFromIndex(size_t idxModel) {
    auto model = GetModelFromIndex(idxModel);
    if (model == nullptr) {
        return;
    }
    SetActiveModel(model);
}

size_t Editor::GetActiveModelIndex() {
    for(size_t i=0; i < openModels.size(); i++) {
        if (openModels[i]->IsActive()) {
            return i;
        }
    }
    // This will happen the first time...
    return 0;
}

EditorModel::Ref Editor::GetActiveModel() {
    for(size_t i=0; i < openModels.size(); i++) {
        if (openModels[i]->IsActive()) {
            return openModels[i];
        }
    }
    // This can happen if there is no model yet assigned (like startup)
    return nullptr;
}

bool Editor::IsModelOpen(EditorModel::Ref model) {
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

KeyMapping::Ref Editor::GetActiveKeyMap() {
    return GetKeyMapForState(state);
}

KeyMapping::Ref Editor::GetKeyMapForState(State paramState) {
    // We allow views to map keys differently (workspace view can have a different keymap from the regular edit-view)
    if (paramState == ViewState) {
        if (HasKeyMapping(viewStateKeymapName)) {
            //logger->Debug("Current ViewState keymap: %s", viewStateKeymapName.c_str());
            return GetKeyMapping(viewStateKeymapName);
        }
        logger->Error("keymap with name '%s' not found - reverting to default!", viewStateKeymapName.c_str());
        return GetKeyMapping("default_keymap");
    }

    // If we are not in the 'ViewState' we are in QuickCommandMode - it has a special keymap!
    return GetKeyMapping("quickmode_keymap");
}

// Return a keymapping based on name
KeyMapping::Ref Editor::GetKeyMapping(const std::string &name) {
    // If we have it - just return it...
    if (HasKeyMapping(name)) {
        return keymappings[name];
    }
    auto keymap = KeyMapping::Create(name);
    if (keymap == nullptr) {
        logger->Error("No keymap with name '%s'", name.c_str());
        return nullptr;
    }
    if (!keymap->IsInitialized()) {
        logger->Error("Keymap '%s' failed to initialize");
        return nullptr;
    }
    // Add to the 'cache'
    keymappings[name] = keymap;
    return keymap;
}

// This checks if a keymapping is loaded - not if it has been configured!!
bool Editor::HasKeyMapping(const std::string &name) {
    return (keymappings.find(name) != keymappings.end());
}

void Editor::SetActiveKeyMapping(const std::string &name) {
    auto newKeyMap = GetKeyMapping(name);
    if (newKeyMap == nullptr) {
        logger->Error("No such keymapping '%s'", name.c_str());
        return;
    }

    if (cbKeymapUpdate != nullptr) {
        cbKeymapUpdate(newKeyMap);
    }

    // In case of view-state we save the active keymap - not quite sure if this is needed...
    if (state == ViewState) {
        logger->Debug("ViewState Keymapping Changed: %s -> %s", viewStateKeymapName.c_str(), name.c_str());
        viewStateKeymapName = name;
    }
}

void Editor::RestoreViewStateKeymapping() {
    if (viewStateKeymapName.empty()) {
        return;
    }
    SetActiveKeyMapping(viewStateKeymapName);
}

void Editor::TriggerUIRedraw() {
    // FIXME: Can't trigger this before the UI is up and running...
    // Trigger redraw by posting an empty message to the root view message queue
    if (Runloop::IsRunning()) {
        RuntimeConfig::Instance().GetRootView().PostMessage([]() {});
    }
}

