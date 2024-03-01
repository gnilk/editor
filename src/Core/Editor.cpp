//
// Created by gnilk on 17.03.23.
//

// On Linux setup asset loader search-paths according to XDG stuff
// see: https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html


#include <vector>
#include <string>
#include <filesystem>
#include <fstream>

#include "Editor.h"
#include "Core/RuntimeConfig.h"
#include "Core/Config/Config.h"
#include "Core/KeyMapping.h"
#include "Core/StrUtil.h"
#include "Core/ActionHelper.h"

#include "Core/Language/LanguageBase.h"
#include "Core/Language/LanguageSupport/CPPLanguage.h"
#include "Core/Language/LanguageSupport/JSONLanguage.h"
#include "Core/Language/LanguageSupport/DefaultLanguage.h"
#include "Core/Plugins/PluginExecutor.h"


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
#include "Core/UnicodeHelper.h"
// API stuff
#include "Core/API/EditorAPI.h"

#if defined(GEDIT_MACOS)
    #include "CoreFoundation/CoreFoundation.h"
    #include <unistd.h>
    #include <libgen.h>
    #include <mach-o/dyld.h>
#endif


using namespace gedit;

#define xstr(a) str(a)
#define xstrver(a,b,c) str(a) "." str(b) "." str(c)
#define str(a) #a

static const std::string glbApplicationName = xstr(GEDIT_APP_NAME);
static const std::string glbVersionString = xstrver(GEDIT_VERSION_MAJOR, GEDIT_VERSION_MINOR, GEDIT_VERSION_PATCH);
static std::u32string glbApplicationNameU32 = U"";
static std::u32string glbVersionStringU32 = U"";

#undef str
#undef xstrver
#undef xstr



Editor &Editor::Instance() {
    static Editor glbSystem;
    return glbSystem;
}

bool Editor::Initialize(int argc, const char **argv) {
    if (isInitialized) {
        return true;
    }
    PreParseArguments(argc, argv);

    // Default should be that the logger is completely disabled but if --enable-logging or similar is given we enable it...
    ConfigurePreInitLogger();

    // Makes it easier to detect starting in file-appending log-file...
    logger->Debug("*************** EDITOR STARTING ***************");

    auto pathHome = XDGEnvironment::Instance().GetUserHomePath();
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();

    // During development, we search in the current directory
    assetLoader.AddSearchPath("resources/", AssetLoaderBase::kLocationType::kSystem);

    // On macOS we add the bundle-root/Contents/SharedSupport to the search path..
#if defined(GEDIT_MACOS)
    // Add ".goatedit" in the root folder for the user
    assetLoader.AddSearchPath(pathHome / ".goatedit", AssetLoaderBase::kLocationType::kUser);
    // Add the Linux (Ubuntu?) .config folder
    assetLoader.AddSearchPath( pathHome / ".config/" / glbApplicationName, AssetLoaderBase::kLocationType::kUser);

    CFBundleRef bundle = CFBundleGetMainBundle();
    CFURLRef bundleURL = CFBundleCopyBundleURL(bundle);
    char path[PATH_MAX];
    Boolean success = CFURLGetFileSystemRepresentation(bundleURL, TRUE, (UInt8 *)path, PATH_MAX);
    assert(success);
    CFRelease(bundleURL);
    std::filesystem::path pathBundle = path;
    assetLoader.AddSearchPath(pathBundle / "Contents" / "SharedSupport", AssetLoaderBase::kLocationType::kSystem);
    logger->Debug("We are on macOS, bundle path: %s", path);
#elif defined(GEDIT_LINUX)
    // On Linux (and others) Add the usr/share directory - this is our default from the install script...
    logger->Debug("We are on Linux, resolving XDG paths");

    auto sysConfigPath = XDGEnvironment::Instance().GetFirstSystemDataPathWithSubDir("goatedit");
    if (sysConfigPath.has_value()) {
        assetLoader.AddSearchPath(sysConfigPath.value(), AssetLoaderBase::kLocationType::kSystem);
    } else {
        logger->Warning("No system install path, assuming dev-env");
    }

    // Add in the user data...
    auto userData = XDGEnvironment::Instance().GetUserDataPath();
    assetLoader.AddSearchPath(userData / "goatedit",AssetLoaderBase::kLocationType::kUser);

    // Add ".goatedit" in the root folder for the user - if someone is installing on old systems
    assetLoader.AddSearchPath(pathHome / ".goatedit",AssetLoaderBase::kLocationType::kUser);

#else
    logger->Error("Unknown or unsupported/untested OS - assuming unix-based");
    assetLoader.AddSearchPath("/usr/share/goatedit", AssetLoaderBase::kLocationType::kSystem);
    // Add ".goatedit" in the root folder for the user - if someone is installing on old systems
    assetLoader.AddSearchPath(pathHome / ".goatedit", AssetLoaderBase::kLocationType::kUser);
#endif
    // should probably rename the config-file to 'goatedit.yml' or something...
    if (!TryLoadConfig("config.yml")) {
        logger->Error("Configuration file missing - please reinstall!!");
        return false;
    }
    logger->Debug("Configuration loaded, proceeding with rest of startup");

    ConfigureLogger();
    ConfigureLogFilter();

    // From this point on we have proper logging to files....
    logger->Debug("*********** Config file was loaded, pre-boot completed **************");

    // Verify if this is a good idea...
    auto cwd = std::filesystem::current_path();
    if (!strutil::startsWith(cwd.string(), pathHome.string())) {
        logger->Warning("Working Directory (%s) outside of home directory, changing to home-root (%s)", cwd.c_str(), pathHome.c_str());
        std::filesystem::current_path(pathHome);
    }
    cwd = std::filesystem::current_path();
    logger->Debug("CWD is: %s", cwd.c_str());

    // Load and configure theme related details..
    ConfigureTheme();

    // Language configuration must currently be done before we load editor models
    ConfigureLanguages();

    logger->Debug("*********** Post-parsing cmd-line arguments ***********");

    // Create workspace
    workspace = Workspace::Create();

    bool createDefaultWorkspace = true;
    // Parse cmd-line
    for(int i=1;i<argc;i++) {
        if (strutil::startsWith(argv[i], "--")) {
            std::string cmdSwitch = std::string(&argv[i][2]);
            if (cmdSwitch == "backend") {
                auto strBackend = argv[++i];
                Config::Instance()["main"].SetStr("backend",strBackend);
            }
        } else {
            // If we open something, disable the auto-creation of the default workspace...
            if (OpenModelOrFolder(argv[i])) {
                createDefaultWorkspace = false;
            } else {
                logger->Error("Error: No such file '%s'\n", argv[i]);
            }
        }
    }

    ConfigureGlobalAPIObjects();

    // create a model if cmd-line didn't specify any
    // this will cause editor to start with at least one new file...
    if (createDefaultWorkspace) {
        // Default workspace will be created if not already..
        workspace->GetDefaultWorkspace();
        int counter = 0;
        char tmpName[32];
        do {
            snprintf(tmpName, 32, "newfile_%d", counter);
            counter++;
        } while(std::filesystem::exists(tmpName));
        auto node = workspace->NewModel(tmpName);
        openModels.push_back(node->GetModel());
    }
    // Did we open any models during the argument parsing?
    if (openModels.size() != 0) {
        // Just set the first as the active model..
        SetActiveModel(openModels[0]);
    }



    auto editKeyMap = GetKeyMapForState(Editor::State::ViewState);
    if (editKeyMap == nullptr) {
        return false;
    }
    isInitialized = true;

    // This is a problem - we really don't handle a 'no-file' scenario - the EditorView expects a model!!
    if (openModels.size() == 0) {
    }

    return true;
}

// Grab stuff which controls initialization
void Editor::PreParseArguments(int argc, const char **argv) {
    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);
        if (strutil::startsWith(arg, "--")) {
            std::string cmdSwitch = std::string(&argv[i][2]);
            if (cmdSwitch == "console-logging") {
                keepConsoleLogger = true;
            } else if (cmdSwitch == "skip-user-config") {
                loadUserConfig = false;
            } else if (cmdSwitch == "help") {
                    PrintHelpToConsole();
                    exit(1);
            }
        } else if ((arg == "-h") || (arg == "-H") || (arg == "-?")) {
            PrintHelpToConsole();
            exit(1);
        }
    }
}

void Editor::PrintHelpToConsole() {
    printf("%s - v%s, cmdline startup options\n", glbApplicationName.c_str(), glbVersionString.c_str());
    printf("use: %s <options> [files...]\n", glbApplicationName.c_str());
    printf("Options:\n");
    printf("  --console-logging, enables console logging to console, this is needed to get pre-initalization logging (before config has been loaded)\n");
    printf("  --skip-user-config, won't load user specific config (starts with defaults)\n");
    printf("  --backend <sdl | ncurses>, override the configuration file backend\n");
}


bool Editor::OpenScreen() {
    ConfigureSubSystems();
    return true;
}

void Editor::Close() {
    logger->Debug("Closing editor");
    for(auto &model : openModels) {
        model->Close();
    }
    openModels.clear();
    RuntimeConfig::Instance().GetKeyboard()->Close();
}

//
void Editor::RunPostInitalizationScript() {

    auto scriptFiles = Config::Instance()["main"].GetSequenceOfStr("bootstrap_scripts");
    for(auto &scriptFile : scriptFiles) {
        wordexp_t exp_result;
        wordexp(scriptFile.c_str(), &exp_result, 0);
        std::string strScript(exp_result.we_wordv[0]);
        wordfree(&exp_result);
        logger->Debug("Trying: %s", strScript);

        // Verify if shell exists...
        struct stat scriptStat = {};
        if (!stat(strScript.c_str(),&scriptStat)) {
            logger->Debug("Script '%s' is absolute - executing", strScript.c_str());
            std::ifstream f(strScript);
            ExecutePostScript(f);
        } else {
            auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
            auto scriptAsset = assetLoader.LoadTextAsset(strScript, AssetLoaderBase::kLocationType::kSystem);
            if (scriptAsset != nullptr) {
                logger->Debug("System Script '%s' found by asset loader, executing", strScript.c_str());
                std::stringstream f(scriptAsset->GetPtrAs<const char *>());
                ExecutePostScript(f);
            }
            scriptAsset = assetLoader.LoadTextAsset(strScript, AssetLoaderBase::kLocationType::kUser);
            if (scriptAsset != nullptr) {
                logger->Debug("User defined script '%s' found by asset loader, executing", strScript.c_str());
                std::stringstream f(scriptAsset->GetPtrAs<const char *>());
                ExecutePostScript(f);
            }

        }
    }
}

void Editor::ExecutePostScript(std::istream &stream) {
    auto comment = Config::Instance()["main"].GetStr("bootstrap_script_comment", "//");
    auto comment32 = UnicodeHelper::utf8to32(comment);

    while(!stream.eof()) {
        char buffer[128];
        stream.getline(buffer, 128);
        auto strcmd = UnicodeHelper::utf8to32(buffer);
        if (!strcmd.empty()) {
            if (!strutil::startsWith(strcmd, comment32)) {
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
       } else if (kpAction.action == kAction::kActionSwitchToTerminal) {
            logger->Debug("Should switch to terminal view");
            ActionHelper::SwitchToNamedView(glbTerminalView);
        } else if (kpAction.action == kAction::kActionSwitchToEditor) {
            logger->Debug("Should switch to editor view");
            ActionHelper::SwitchToNamedView(glbEditorView);
            // maximize?
        } else if (kpAction.action == kAction::kActionSwitchToProject) {
            logger->Debug("Should switch to editor view");
            ActionHelper::SwitchToNamedView(glbWorkSpaceView);
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

// This is the log-path before config file has been loaded - it will only be the console..
void Editor::ConfigurePreInitLogger() {

    // Need to have pipe here in order for shell-fork to work as expected...
    gnilk::Logger::UseIPCMechanism(gnilk::IPCMechanism::kPipe);

    gnilk::Logger::Initialize();
    logger = gnilk::Logger::GetLogger("System");
    if (!keepConsoleLogger) {
        gnilk::Logger::RemoveSink("console");
    }
/*
    // Keep this to hold logging during pre-initalization
    auto cfgLogging = Config::Instance().GetNode("logging");
    auto logFileName = "preinitlog.log";
    auto sinkName = "filesink";
    auto logPath = XDGEnvironment::Instance().GetUserStatePath();
    if (!CheckCreateDirectory(logPath)) {
        return;
    }

    logPath /= logFileName;
    auto fileSink = new gnilk::LogFileSink();
    const char *sinkArgv[]={"autoflush", "file", logPath.c_str()};
    gnilk::Logger::AddSink(fileSink, sinkName, 3, sinkArgv);
*/
}

// Called after config has been loaded
void Editor::ConfigureLogger() {
    gnilk::Logger::Initialize();

    auto cfgLogging = Config::Instance().GetNode("logging");
    auto logFileName = cfgLogging.GetStr("logfile","goatedit.log");
    auto sinkName = cfgLogging.GetStr("logsink","filesink");

#ifdef GEDIT_LINUX
    auto logPath = XDGEnvironment::Instance().GetUserStatePath();
#else
    auto logPath = XDGEnvironment::Instance().GetUserHomePath();
    logPath /= "Library/Logs/GoatEdit";
#endif
    if (!CheckCreateDirectory(logPath)) {
        logger->Error("Unable to create log output directory '%s'", logPath.c_str());
        return;
    }

    logPath /= logFileName;
    if (sinkName == "filesink") {
        auto fileSink = gnilk::LogFileSink::Create();
        const std::vector<std::string_view> sinkArgv={"autoflush", "file", logPath.c_str()};

        logger->Info("Logger filesink with logfile; %s", logPath.c_str());

        gnilk::Logger::AddSink(fileSink, sinkName, sinkArgv);
        // Remove the console sink (it is auto-created in debug-mode)
        if (!keepConsoleLogger) {
            gnilk::Logger::RemoveSink("console");
        }
    } else {
        logger->Error("Unknown sink: %s", sinkName);
        exit(1);
    }
}

bool Editor::CheckCreateDirectory(const std::filesystem::path &path) {
    if (!std::filesystem::exists(path)) {
        logger->Debug("LogPath root '%s' invalid, trying to create..", path.string());
        if (!std::filesystem::create_directories(path)) {
            logger->Error("Logpath '%s' invalid, keeping console logging...", path.string());
            return false;
        }
    }
    return true;
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
    auto isMainConfOk =  Config::Instance().LoadSystemConfig(configFile);

    // Merge with user conf..
    if (loadUserConfig) {
        Config::Instance().MergeUserConfig(configFile, true);
    } else {
        logger->Debug("User config loading disabled, skipping!");
    }

    return isMainConfOk;
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
void Editor::ConfigureTheme() {
    logger->Debug("Configuring colors and theme");

    auto mainNode = Config::Instance().GetNode("main");
    auto themeFile = mainNode.GetStr("theme", "default.theme.yml");

    theme = Theme::Create();
    if (theme == nullptr) {
        return;
    }
    if (!theme->Load(themeFile)) {
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
    bool isTerminal = XDGEnvironment::Instance().IsTerminalSession();
#ifdef GEDIT_MACOS
    if (std::getenv("GEDIT_MACOS_STARTED_FROM_UI") != nullptr) {
        enforceSDL = true;
        logger->Debug("Application started from desktop environment, enforcing SDL");
    }
#endif

    if (((backend == "sdl") || enforceSDL) && !isTerminal) {
        logger->Debug("Starting SDL backend");
        SetupSDL();
    } else {
        logger->Debug("Starting NCurses backend");
        SetupNCurses();
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
    auto screenDriver = SDLScreen::Create();

    RuntimeConfig::Instance().SetScreen(screenDriver);

    screenDriver->Open();
    screenDriver->Clear();

    auto keyDriver = SDLKeyboardDriver::Create();
    if (keyDriver == nullptr) {
        logger->Error("Failed to initalize SDL Keyboard driver!");
        printf("Failed to initalize SDL Keyboard driver!\n");
        exit(1);
    }
    RuntimeConfig::Instance().SetKeyboard(keyDriver);


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

Workspace::Node::Ref Editor::GetWorkspaceNodeForActiveModel() {
    auto model = GetActiveModel();
    if (model == nullptr) {
        return nullptr;
    }
    return workspace->GetNodeFromModel(model);
}
Workspace::Node::Ref Editor::GetWorkspaceNodeForModel(EditorModel::Ref model) {
    return workspace->GetNodeFromModel(model);
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
    logger->Debug("Keymap '%s' not found - creating", name.c_str());
#if defined(GEDIT_LINUX)
    auto osCfgRootNode = Config::Instance().GetNode("linux");
#elif defined(GEDIT_MACOS)
    auto osCfgRootNode = Config::Instance().GetNode("macos");
#else
    logger->Error("This should not happen...");
    exit(1);
#endif
    auto strKeymapFile = osCfgRootNode.GetStr(name, "Default/default_keymap.yml");
    auto assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    auto keymapAsset = assetLoader.LoadTextAsset(strKeymapFile);
    if (keymapAsset == nullptr) {
        logger->Error("Keymap '%s' not found via '%s'", name.c_str(), strKeymapFile.c_str());
        return nullptr;
    }
    auto keymapConfig = ConfigNode::FromString(keymapAsset->GetPtrAs<const char *>());
    if (!keymapConfig.has_value()) {
        logger->Error("Keymap could not be constructed from asset '%s'", strKeymapFile.c_str());
        return nullptr;
    }

    //auto cfgNode = Config::Instance().GetNode(name);
    auto cfgNode = keymapConfig->GetNode("keymap");
    auto keymap = KeyMapping::Create(cfgNode);
    if (keymap == nullptr) {
        logger->Error("No keymap with name '%s'", name.c_str());
        return nullptr;
    }
    if (!keymap->IsInitialized()) {
        logger->Error("Keymap '%s' failed to initialize");
        return nullptr;
    }

    logger->Debug("Ok, keymap '%s' (from '%s') initialized and cached!", name.c_str(), strKeymapFile.c_str());

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
        //logger->Debug("ViewState Keymapping Changed: %s -> %s", viewStateKeymapName.c_str(), name.c_str());
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
    // Trigger redraw by posting an empty message to the root view message queue
    if (Runloop::IsRunning()) {
        RuntimeConfig::Instance().GetRootView().PostMessage([]() {});
    }
}

const std::u32string &Editor::GetAppName() {
    if (glbApplicationNameU32.empty()) {
        UnicodeHelper::ConvertUTF8ToUTF32String(glbApplicationNameU32, glbApplicationName);
    }
    return glbApplicationNameU32;
}
const std::u32string &Editor::GetVersion() {
    if (glbVersionStringU32.empty()) {
        UnicodeHelper::ConvertUTF8ToUTF32String(glbVersionStringU32, glbVersionString);
    }
    return glbVersionStringU32;
}

//////////////////////
//
//
//
// Create a new model/buffer
EditorModel::Ref Editor::NewModel(const std::string &name) {
    return nullptr;
}



EditorModel::Ref Editor::OpenModelFromWorkspace(Workspace::Node::Ref workspaceNode) {
    auto model = workspaceNode->GetModel();
    if (IsModelOpen(model)) {
        SetActiveModel(model);
        return model;
    }

    // Make sure we load it if not yet done...
    if (!workspaceNode->LoadData()) {
        logger->Error("OpenModelFromWorkspace, failed to load: %s", workspaceNode->GetDisplayName().c_str());
        return nullptr;
    }
    logger->Error("OpenModelFromWorkspace, loaded model: %s", workspaceNode->GetDisplayName().c_str());
    openModels.push_back(model);
    logger->Debug("Activating new model");
    SetActiveModel(model);

    return model;;
}

bool Editor::OpenModelOrFolder(const std::string &fileOrFolder) {
    auto pathName =std::filesystem::path(fileOrFolder);
    if (!std::filesystem::exists(pathName)) {
        // Create file here..
        logger->Debug("File doesn't exists, creating new");
        auto nodeModel = workspace->NewModelWithFileRef(pathName);
        if (nodeModel == nullptr) {
            logger->Error("Failed to create new file!");
            return false;
        }
        openModels.push_back(nodeModel->GetModel());
        return true;
    }
    if (std::filesystem::is_directory(pathName)) {
        logger->Debug("Opening folder: %s", fileOrFolder.c_str());
        if (!workspace->OpenFolder(pathName)) {
            logger->Error("Unable to open folder: %s", fileOrFolder.c_str());
            return false;
        }
        if (pathName.is_absolute()) {
            logger->Debug("Changing directory to: %s", pathName.c_str());
            std::filesystem::current_path(pathName);
        }

        return true;
    }

    auto model = LoadModel(fileOrFolder);
    if (model == nullptr) {
        // errors dumped already...
        return false;
    }

    return true;

}

EditorModel::Ref Editor::LoadModel(const std::string &filename) {
    auto pathName =std::filesystem::path(filename);
    if (!std::filesystem::exists(pathName)) {
        logger->Error("File not found: %s", filename.c_str());
        return nullptr;
    }
    if (std::filesystem::is_directory(pathName)) {
        logger->Error("DO NOT CALL 'LoadModel' with directories!!!");
        return nullptr;
    }
    auto node = workspace->NewModelWithFileRef(filename);
    if (node == nullptr) {
        logger->Error("Failed to create model");
        return nullptr;
    }

    node->LoadData();

    // Must be present

    bool bIsReadOnly = node->GetMeta<bool>(Workspace::Node::kMetaKey_ReadOnly, false);
    if (bIsReadOnly) {
        logger->Debug("File '%s' is readonly", filename.c_str());
        node->GetModel()->GetTextBuffer()->SetReadOnly(true);
    }

    openModels.push_back(node->GetModel());

    return node->GetModel();
}

// This will simply close the editing of the text-model
// NOTE: DO NOT add 'save confirmation' here - this is also called for external removal (such as someone doing rm on a file from the terminal)
bool Editor::CloseModel(EditorModel::Ref model) {
    auto node = workspace->GetNodeFromModel(model);
    if (node == nullptr) {
        logger->Error("Model not part of workspace!!!!!");
        return false;
    }

    auto itModel = std::find(openModels.begin(), openModels.end(),model);
    if (itModel != openModels.end()) {
        logger->Debug("Ok, removing model '%s' from open models", node->GetDisplayName().c_str());

        // Figure out which one will be the next model...
        // The model list is a strict list which is visualized exactly as it is stored, thus - right most won't have a next and left-most won't have a left...
        // Priority to step 'right' from current when closing...
        // In case there are just 1 open - we set everything to null (this is the default when there are no open models)
        EditorModel::Ref nextActive = nullptr;

        auto idxCurrent = GetActiveModelIndex();
        auto idxNext = NextModelIndex(idxCurrent);
        // Do we even have a 'next'??
        if (idxNext > idxCurrent) {
            nextActive = GetModelFromIndex(idxNext);
        } else if (PreviousModelIndex(idxCurrent) != idxCurrent) {
            nextActive = GetModelFromIndex(PreviousModelIndex(idxCurrent));
        }

        openModels.erase(itModel);
        model->SetActive(false);
        model->Close();

        if (nextActive != nullptr) {
            SetActiveModel(nextActive);
        }

        if (RuntimeConfig::Instance().HasRootView()) {
            RuntimeConfig::Instance().GetRootView().Initialize();
        }

        return true;
    }
    logger->Error("Model '%s' not found in open models", node->GetDisplayName().c_str());
    return false;
}







