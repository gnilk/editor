//
// Created by gnilk on 17.03.23.
//

// On Linux setup asset loader search-paths according to XDG stuff
// see: https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html


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
#include <unistd.h>
#include "Core/XDGEnvironment.h"
// API stuff
#include "Core/API/EditorAPI.h"

#if defined(GEDIT_MACOS)
    #include "CoreFoundation/CoreFoundation.h"
    #include <unistd.h>
    #include <libgen.h>
    #include <mach-o/dyld.h>
#endif


using namespace gedit;

static const std::string glbApplicationName = "goatedit";

Editor &Editor::Instance() {
    static Editor glbSystem;
    return glbSystem;
}

bool Editor::Initialize(int argc, const char **argv) {
    if (isInitialized) {
        return true;
    }
    ConfigurePreInitLogger();

    // Makes it easier to detect starting in file-appending log-file...
    logger->Debug("*************** EDITOR STARTING ***************");

    auto pathHome = XDGEnvironment::Instance().GetUserHomePath();
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();

    // During development, we search in the current directory
    assetLoader.AddSearchPath("goatedit/");

    // On macOS we add the bundle-root/Contents/SharedSupport to the search path..
#ifdef GEDIT_MACOS
    // Add ".goatedit" in the root folder for the user
    assetLoader.AddSearchPath(pathHome / ".goatedit");
    // Add the Linux (Ubuntu?) .config folder
    assetLoader.AddSearchPath( pathHome / ".config/" / glbApplicationName);

    CFBundleRef bundle = CFBundleGetMainBundle();
    CFURLRef bundleURL = CFBundleCopyBundleURL(bundle);
    char path[PATH_MAX];
    Boolean success = CFURLGetFileSystemRepresentation(bundleURL, TRUE, (UInt8 *)path, PATH_MAX);
    assert(success);
    CFRelease(bundleURL);
    std::filesystem::path pathBundle = path;
    assetLoader.AddSearchPath(pathBundle / "Contents" / "SharedSupport");
    logger->Debug("We are on macOS, bundle path: %s", path);
#elifdef GEDIT_LINUX
    // On Linux (and others) Add the usr/share directory - this is our default from the install script...
    logger->Debug("We are on Linux, resolving XDG paths");
    auto usrLocalPath = XDGEnvironment::Instance().GetFirstSystemDataPathWithPrefix("/usr/local");
    if (usrLocalPath.has_value()) {
        assetLoader.AddSearchPath(usrLocalPath.value());
    } else {
        auto defaultSysPath = XDGEnvironment::Instance().GetFirstSystemDataPath();
        assetLoader.AddSearchPath(defaultSysPath);
    }
    // Add in the user data...
    auto userData = XDGEnvironment::Instance().GetUserDataPath();
    assetLoader.AddSearchPath(userData);

    // Add ".goatedit" in the root folder for the user - if someone is installing on old systems
    assetLoader.AddSearchPath(pathHome / ".goatedit");

#else
    logger->Error("Unknown or unsupported/untested OS - assuming unix-based");
    assetLoader.AddSearchPath("/usr/share/goatedit");
    // Add ".goatedit" in the root folder for the user - if someone is installing on old systems
    assetLoader.AddSearchPath(pathHome / ".goatedit");
#endif
    // should probably rename the config-file to 'goatedit.yml' or something...
    if (!TryLoadConfig("config.yml")) {
        logger->Error("Configuration file missing - please reinstall!!");
        return false;
    }

    ConfigureLogger();
    logger->Debug("*********** Config file was loaded, pre-boot completed **************");

    // Verify if this is a good idea...
    auto cwd = std::filesystem::current_path();
    if (!strutil::startsWith(cwd.string(), pathHome.string())) {
        logger->Warning("Working Directory (%s) outside of home directory, changing to home-root (%s)", cwd.c_str(), pathHome.c_str());
        std::filesystem::current_path(pathHome);
    }
    cwd = std::filesystem::current_path();
    logger->Debug("CWD is: %s", cwd.c_str());


    // here - try merge the current configuration with '.goatedit.yml' from ~/.goatedit
    // or just skip that and let the user change '~/.config/goatedit/goatedit.yml'

    logger->Debug("Configuration loaded, proceeding with rest of startup");

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
    if (!std::filesystem::exists(std::filesystem::path(filename))) {
        logger->Error("File not found: %s", filename.c_str());
        return nullptr;
    }
    auto model = workspace->NewModelWithFileRef(filename);
    model->GetTextBuffer()->Load();
    openModels.push_back(model);

    return model;
}

bool Editor::CloseModel(EditorModel::Ref model) {
    return workspace->CloseModel(model);
}

// This is the log-path before config file has been loaded - it will only be the console..
void Editor::ConfigurePreInitLogger() {
    gnilk::Logger::Initialize();
    logger = gnilk::Logger::GetLogger("System");
}

// Must be called before 'LoadConfig' - as the config loader will output debug info...
void Editor::ConfigureLogger() {
//    static const char *sinkArgv[]={"autoflush","file","logfile.log"};
    gnilk::Logger::Initialize();

    auto cfgLogging = Config::Instance().GetNode("logging");
    auto logFileName = cfgLogging.GetStr("logfile","goatedit.log");
    auto sinkName = cfgLogging.GetStr("logsink","filesink");

#ifdef GEDIT_LINUX
    auto logPath = XDGEnvironment::Instance().GetUserStatePath();
    if (!std::filesystem::exists(logPath)) {
        logger->Debug("LogPath root '%s' invalid, trying to create..", logPath.c_str());
        if (!std::filesystem::create_directories(logPath)) {
            logger->Error("Logpath '%s' invalid, keeping console logging...", logPath.c_str());
            return;
        }
    }
#else
    // TODO: this needs more work
    auto logPath = XDGEnvironment::Instance().GetUserHomePath();
#endif

    logPath /= logFileName;
    if (sinkName == "filesink") {
        auto fileSink = new gnilk::LogFileSink();
        const char *sinkArgv[]={"autoflush", "file", logPath.c_str()};
        gnilk::Logger::AddSink(fileSink, sinkName.c_str(), 3, sinkArgv);
        // Remove the console sink (it is auto-created in debug-mode)
        gnilk::Logger::RemoveSink("console");
    } else {
        logger->Error("Unknown sink: %s", sinkName.c_str());
        exit(1);
    }
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


bool Editor::TryLoadConfig(const char *configFile) {
    return Config::Instance().LoadConfig(configFile);
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

// FIXME: This is more than a bit ugly - not really sure where to stuff it..
// Could put in the theme class but than I would depend on LanguageToken stuff - which I quite don't like..
void Editor::ConfigureColorTheme() {
    logger->Debug("Configuring colors and theme");

    auto theme = Config::Instance().GetTheme();
    if (theme == nullptr) {
        return;
    }

    // This map's the various content colors to tokenizer classes for syntax highlighting

    auto &colorConfig = theme->GetContentColors();
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
    }
}

// Configure global/static API objects..
void Editor::ConfigureGlobalAPIObjects() {
    static EditorAPI editorApi;

    RegisterGlobalAPIObject<EditorAPI>(&editorApi);

    // Initialize the Javascript wrapper engine...
    logger->Debug("Initialize JSEngine and Preload Plugins");
    jsEngine.Initialize();
}

static std::vector<std::string> glbSupportedBackends = {
        {"ncurses"},
        {"sdl"},
};

extern "C" char ** environ;

void Editor::ConfigureSubSystems() {
    auto backend = Config::Instance()["main"].GetStr("backend","ncurses");

    // When debugging the actual Bundle on macOS there is no way to launch the app from within CLion
    // Instead we start from command-line and use 'attach to process' - use this loop to 'wait' for it..

//    bool bWait = true;
//    while(bWait) {
//        std::this_thread::yield();
//    }

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
    logger->Debug("Initialize Graphics Backend");

    // This could probably be generalized to 'GEDIT_STARTED_FROM_UI' and also set through the Desktop file on Linux
    // In case the default config specifies a non-graphical backend (like ncurses) we override it...
    bool enforceSDL = false;
#ifdef GEDIT_MACOS
    if (std::getenv("GEDIT_MACOS_STARTED_FROM_UI") != nullptr) {
        enforceSDL = true;
        logger->Debug("Application started from desktop environment, enforcing SDL");
    }
#endif

    if ((backend == "ncurses") && (!enforceSDL)) {
        logger->Debug("Starting NCurses backend");
        SetupNCurses();
    } else if ((backend == "sdl") || (enforceSDL)) {
        logger->Debug("Starting SDL backend");
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

