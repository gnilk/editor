//
// Created by gnilk on 17.03.23.
//

// On Linux setup asset loader search-paths according to XDG stuff
// see: https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html


#include <algorithm>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>

#include "Editor.h"
#include "Core/RuntimeConfig.h"
#include "Core/Config/Config.h"
#include "Core/KeyMapping.h"
#include "Core/KeyMappingCache.h"
#include "Core/StrUtil.h"
#include "Core/ActionHelper.h"

#include "Core/Language/LanguageBase.h"
#include "Core/Language/LanguageSupport/CPPLanguage.h"
#include "Core/Language/LanguageSupport/JSONLanguage.h"
#include "Core/Language/LanguageSupport/DefaultLanguage.h"
#include "Core/Plugins/PluginExecutor.h"


// NCurses backend - Removed
// #include "Core/Graphics/NCurses/NCursesScreen.h"
// #include "Core/Graphics/NCurses/NCursesKeyboardDriver.h"

// SDL3 backend
#ifdef GEDIT_USE_SDL3
#include "Core/Graphics/SDL3/SDLScreen.h"
#include "Core/Graphics/SDL3/SDLKeyboardDriver.h"
#endif

#ifdef GEDIT_USE_SDL2
#include "Core/Graphics/SDL2/SDLScreen.h"
#include "Core/Graphics/SDL2/SDLKeyboardDriver.h"
#endif

#include <sys/stat.h>
#include <wordexp.h>
#include <unistd.h>

#include "Core/XDGEnvironment.h"
#include "Core/UnicodeHelper.h"
// API stuff
#include "Core/API/EditorAPI.h"
#include "Core/Graphics/Headless/HLScreen.h"

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

// True if 'path' lies within the 'base' subtree (base itself counts as within).
// Uses relative() so it can't be fooled by a shared string prefix (/home/gnilkfoo
// is NOT under /home/gnilk).
static bool IsWithinTree(const std::filesystem::path &path, const std::filesystem::path &base) {
    std::error_code ec;
    auto rel = std::filesystem::relative(path, base, ec);
    if (ec || rel.empty()) {
        return false;
    }
    return *rel.begin() != "..";
}

// True if 'path' is strictly BELOW 'base' - excludes base itself ("." ) and anything
// outside it (".."). This is the "is this a real project directory" test.
static bool IsStrictSubdir(const std::filesystem::path &path, const std::filesystem::path &base) {
    std::error_code ec;
    auto rel = std::filesystem::relative(path, base, ec);
    if (ec || rel.empty()) {
        return false;
    }
    const auto &first = *rel.begin();
    return first != "." && first != "..";
}

bool Editor::Initialize(int argc, const char **argv) {
    if (isInitialized) {
        return true;
    }
    ParseArguments(argc, argv);

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

    // When started from the UI (someone clicking the icon) there is no meaningful CWD
    // pointing at a project - notably macOS Finder launches start at filesystem root.
    // Relative-path operations (open/save, default workspace) would then resolve under
    // '/' where nothing useful lives, so relocate to $HOME when CWD is outside the home tree.
    auto cwd = std::filesystem::current_path();
    if (!IsWithinTree(cwd, pathHome)) {
        logger->Warning("Working Directory (%s) outside of home directory, changing to home-root (%s)", cwd.c_str(), pathHome.c_str());
        std::filesystem::current_path(pathHome);
        cwd = std::filesystem::current_path();
    }
    logger->Debug("CWD is: %s", cwd.c_str());

    // A '.goatedit' directory in the project (mirroring '~/.goatedit') is the opt-in
    // marker for project-local assets/history. Register it as the kProject search path
    // only when it exists, and only for a real sub-directory of home - so '/', '$HOME'
    // itself and anything outside home (e.g. /var/log) can never become a project.
    auto projectDir = cwd / ".goatedit";
    if (IsStrictSubdir(cwd, pathHome) && std::filesystem::is_directory(projectDir)) {
        assetLoader.AddSearchPath(projectDir, AssetLoaderBase::kLocationType::kProject);
    }

    // Load and configure theme related details..
    ConfigureTheme();

    // Language configuration must currently be done before we load editor models
    ConfigureLanguages();

    // Create workspace
    workspace = Workspace::Create();

    bool createDefaultWorkspace = true;
    for (auto &file : pendingFiles) {
        if (OpenDocumentOrFolder(file)) {
            createDefaultWorkspace = false;
        } else {
            logger->Error("Error: No such file '%s'", file.c_str());
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
        auto node = workspace->NewDocument(tmpName);
        workspace->AddOpenDocument(node->GetDocument());
    }
    // Did we open any models during the argument parsing?
    auto &openDocuments = workspace->GetOpenDocuments();
    if (openDocuments.size() != 0) {
        // Just set the first as the active model..
        SetActiveDocument(openDocuments[0]);
    }



    auto editKeyMap = GetKeyMapForState(Editor::State::ViewState);
    if (editKeyMap == nullptr) {
        return false;
    }
    isInitialized = true;

    // This is a problem - we really don't handle a 'no-file' scenario - the EditorView expects a model!!
    if (openDocuments.size() == 0) {
    }

    return true;
}

// Grab stuff which controls initialization
void Editor::ParseArguments(int argc, const char **argv) {
    pendingFiles.clear();
    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);
        if (strutil::startsWith(arg, "--")) {
            std::string cmdSwitch = arg.substr(2);
            if (cmdSwitch == "console-logging") {
                keepConsoleLogger = true;
            } else if (cmdSwitch == "skip-user-config") {
                loadUserConfig = false;
            } else if (cmdSwitch == "backend") {
                if (i + 1 < argc) {
                    argBackend = argv[++i];
                    std::transform(argBackend.begin(), argBackend.end(), argBackend.begin(), ::tolower);
                }
            } else if (cmdSwitch == "help") {
                PrintHelpToConsole();
                exit(1);
            }
        } else if ((arg == "-h") || (arg == "-H") || (arg == "-?")) {
            PrintHelpToConsole();
            exit(1);
        } else {
            pendingFiles.push_back(arg);
        }
    }
}

void Editor::PrintHelpToConsole() {
    printf("%s - v%s, cmdline startup options\n", glbApplicationName.c_str(), glbVersionString.c_str());
    printf("use: %s <options> [files...]\n", glbApplicationName.c_str());
    printf("Options:\n");
    printf("  --console-logging, enables console logging to console, this is needed to get pre-initalization logging (before config has been loaded)\n");
    printf("  --skip-user-config, won't load user specific config (starts with defaults)\n");
    printf("  --backend <sdl | headless>, override the configuration file backend\n");
}


bool Editor::OpenScreen() {
    ConfigureSubSystems();
    return true;
}

void Editor::Close() {
    logger->Debug("Closing editor");
    for(auto &model : workspace->GetOpenDocuments()) {
        model->Close();
    }
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
    } else if (sinkName == "console") {
        // Console is already the default sink, no need to add it again
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
        Config::Instance().TryMergeUserConfig(configFile, true);
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
#if defined(GEDIT_USE_SDL3) || defined(GEDIT_USE_SDL2)
        {"sdl"},
#endif
    {"headless"}
};

extern "C" char ** environ;

void Editor::ConfigureSubSystems() {
    auto backend = argBackend.empty()
        ? Config::Instance()["main"].GetStr("backend", "sdl")
        : argBackend;
    // Normalise to lowercase
    std::transform(backend.begin(), backend.end(), backend.begin(), ::tolower);
    // See if supported; if not, print the list and exit.
    if (std::find(glbSupportedBackends.begin(), glbSupportedBackends.end(), backend) == glbSupportedBackends.end()) {
        logger->Error("Unknown backend: '%s'", backend.c_str());
        fprintf(stderr, "Unknown backend: '%s'\n", backend.c_str());
        fprintf(stderr, "Supported backends:\n");
        for (auto &be : glbSupportedBackends) {
            fprintf(stderr, "  %s\n", be.c_str());
        }
        exit(1);
    }
    logger->Debug("Initialize Graphics Backend: %s", backend.c_str());

    // This could probably be generalized to 'GEDIT_STARTED_FROM_UI' and also set through the Desktop file on Linux
    bool enforceSDL = false;
    bool isTerminal = XDGEnvironment::Instance().IsTerminalSession();
#ifdef GEDIT_MACOS
    if (std::getenv("GEDIT_MACOS_STARTED_FROM_UI") != nullptr) {
        enforceSDL = true;
        logger->Debug("Application started from desktop environment, enforcing SDL");
    }
#endif

    if (isTerminal && !enforceSDL) {
        logger->Error("Terminal session without enforced SDL — NCurses backend discontinued, exiting!");
        exit(1);
    }

    if (backend == "sdl" || enforceSDL) {
#ifdef GEDIT_USE_SDL3
        logger->Debug("Starting SDL3 backend");
        SetupSDL3();
        return;
#elif defined(GEDIT_USE_SDL2)
        logger->Debug("Starting SDL2 backend");
        SetupSDL2();
        return;
#else
        logger->Error("Binary built without any SDL backend");
        exit(1);
#endif
    }

    // Headless mode - allows to run without a graphical interface
    // also no keyboard support - must inject!
    if (backend == "headless") {
        logger->Debug("Starting Headless backend");
        SetupHeadless();
        return;
    }

    // Should never reach here — the supported-backend check above catches unknown values.
    logger->Error("No suitable backend for '%s'", backend.c_str());
    exit(1);
}

void Editor::SetupNCurses() {

    // auto screenDriver = NCursesScreen::Create();
    // auto kbDriver = NCursesKeyboardDriver::Create();
    // RuntimeConfig::Instance().SetKeyboard(kbDriver);
    // RuntimeConfig::Instance().SetScreen(screenDriver);
    //
    // screenDriver->Open();
    // screenDriver->Clear();
}

#ifdef GEDIT_USE_SDL3
void Editor::SetupSDL3() {
    auto screenDriver = SDL3::SDLScreen::Create();
    RuntimeConfig::Instance().SetScreen(screenDriver);
    screenDriver->Open();
    screenDriver->Clear();

    auto keyDriver = SDL3::SDLKeyboardDriver::Create();
    if (keyDriver == nullptr) {
        logger->Error("Failed to initialize SDL3 Keyboard driver!");
        printf("Failed to initialize SDL3 Keyboard driver!\n");
        exit(1);
    }
    RuntimeConfig::Instance().SetKeyboard(keyDriver);
}
#else
void Editor::SetupSDL3() {
    logger->Error("SDL3 backend not compiled in!");
    exit(1);
}
#endif

#ifdef GEDIT_USE_SDL2
void Editor::SetupSDL2() {
    auto screenDriver = SDL2::SDLScreen::Create();
    RuntimeConfig::Instance().SetScreen(screenDriver);
    screenDriver->Open();
    screenDriver->Clear();

    auto keyDriver = SDL2::SDLKeyboardDriver::Create();
    if (keyDriver == nullptr) {
        logger->Error("Failed to initialize SDL2 Keyboard driver!");
        printf("Failed to initialize SDL2 Keyboard driver!\n");
        exit(1);
    }
    RuntimeConfig::Instance().SetKeyboard(keyDriver);
}
#else
void Editor::SetupSDL2() {
    logger->Error("SDL2 backend not compiled in!");
    exit(1);
}
#endif

void Editor::SetupHeadless() {
    auto screenDriver = HLScreen::Create();
    RuntimeConfig::Instance().SetScreen(screenDriver);
    screenDriver->Open();
    screenDriver->Clear();

    auto keyDriver = HLKeyboardDriver::Create();
    if (keyDriver == nullptr) {
        logger->Error("Failed to initialize SDL3 Keyboard driver!");
        printf("Failed to initialize SDL3 Keyboard driver!\n");
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

// Sets the active model in the workspace, then relays out the UI to reflect it. The state lives
// in the Workspace; the UI side-effect (relayout) belongs here in the application layer.
void Editor::SetActiveDocument(Document::Ref model) {
    workspace->SetActiveDocument(model);
    if (workspace->GetActiveDocument() != model) {
        // Model wasn't open - nothing changed.
        return;
    }
    if (RuntimeConfig::Instance().HasRootView()) {
        RuntimeConfig::Instance().GetRootView().Initialize();
    }
}

void Editor::SetActiveDocumentFromIndex(size_t idxDocument) {
    auto model = GetDocumentFromIndex(idxDocument);
    if (model == nullptr) {
        return;
    }
    SetActiveDocument(model);
}

Workspace::Node::Ref Editor::GetWorkspaceNodeForActiveDocument() {
    auto model = GetActiveDocument();
    if (model == nullptr) {
        return nullptr;
    }
    return workspace->GetNodeFromDocument(model);
}
Workspace::Node::Ref Editor::GetWorkspaceNodeForDocument(Document::Ref model) {
    return workspace->GetNodeFromDocument(model);
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

// Return a keymapping based on name. The keymap cache (KeyMappingCache) owns loading, building and
// caching - including resolving the 'inherit' chain - so Editor is just a consumer here. Each named
// keymap is loaded exactly once and shared between Editor and inheritance resolution.
KeyMapping::Ref Editor::GetKeyMapping(const std::string &name) {
    return KeyMappingCache::Instance().GetOrLoad(name);
}

// This checks if a keymapping is already loaded/cached - not if it can be loaded!
bool Editor::HasKeyMapping(const std::string &name) {
    return KeyMappingCache::Instance().Has(name);
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

Document::Ref Editor::OpenDocumentFromWorkspace(Workspace::Node::Ref workspaceNode) {
    // Lazily build the model if this is a path-only (folder-scanned) node.
    auto model = workspace->EnsureDocumentForNode(workspaceNode);
    if (model == nullptr) {
        logger->Error("OpenDocumentFromWorkspace, node has no model (folder?): %s", workspaceNode->GetDisplayName().c_str());
        return nullptr;
    }
    if (IsDocumentOpen(model)) {
        SetActiveDocument(model);
        return model;
    }

    // Make sure we load it if not yet done...
    if (!workspaceNode->LoadData()) {
        logger->Error("OpenDocumentFromWorkspace, failed to load: %s", workspaceNode->GetDisplayName().c_str());
        return nullptr;
    }
    logger->Error("OpenDocumentFromWorkspace, loaded model: %s", workspaceNode->GetDisplayName().c_str());
    workspace->AddOpenDocument(model);
    logger->Debug("Activating new model");
    SetActiveDocument(model);

    return model;;
}

bool Editor::OpenDocumentOrFolder(const std::string &fileOrFolder) {
    auto pathName =std::filesystem::path(fileOrFolder);
    if (!std::filesystem::exists(pathName)) {
        // Create file here..
        logger->Debug("File doesn't exists, creating new");
        auto nodeModel = workspace->NewDocumentWithFileRef(pathName);
        if (nodeModel == nullptr) {
            logger->Error("Failed to create new file!");
            return false;
        }
        workspace->AddOpenDocument(nodeModel->GetDocument());
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

    auto model = LoadDocument(fileOrFolder);
    if (model == nullptr) {
        // errors dumped already...
        return false;
    }

    return true;

}

Document::Ref Editor::LoadDocument(const std::string &filename) {
    auto pathName =std::filesystem::path(filename);
    if (!std::filesystem::exists(pathName)) {
        logger->Error("File not found: %s", filename.c_str());
        return nullptr;
    }
    if (std::filesystem::is_directory(pathName)) {
        logger->Error("DO NOT CALL 'LoadDocument' with directories!!!");
        return nullptr;
    }
    auto node = workspace->NewDocumentWithFileRef(filename);
    if (node == nullptr) {
        logger->Error("Failed to create model");
        return nullptr;
    }

    node->LoadData();

    // Must be present

    bool bIsReadOnly = node->GetMeta<bool>(Workspace::Node::kMetaKey_ReadOnly, false);
    if (bIsReadOnly) {
        logger->Debug("File '%s' is readonly", filename.c_str());
        node->GetDocument()->GetTextBuffer()->SetReadOnly(true);
    }

    workspace->AddOpenDocument(node->GetDocument());

    return node->GetDocument();
}

// This will simply close the editing of the text-model
// NOTE: DO NOT add 'save confirmation' here - this is also called for external removal (such as someone doing rm on a file from the terminal)
bool Editor::CloseDocument(Document::Ref model) {
    auto node = workspace->GetNodeFromDocument(model);
    if (node == nullptr) {
        logger->Error("Model not part of workspace!!!!!");
        return false;
    }

    if (!IsDocumentOpen(model)) {
        logger->Error("Model '%s' not found in open models", node->GetDisplayName().c_str());
        return false;
    }
    logger->Debug("Ok, removing model '%s' from open models", node->GetDisplayName().c_str());

    // Figure out which one will be the next model... (computed against the pre-removal list)
    // The model list is a strict list which is visualized exactly as it is stored, thus - right most won't have a next and left-most won't have a left...
    // Priority to step 'right' from current when closing...
    // In case there are just 1 open - we set everything to null (this is the default when there are no open models)
    Document::Ref nextActive = nullptr;

    auto idxCurrent = GetActiveDocumentIndex();
    auto idxNext = NextDocumentIndex(idxCurrent);
    // Do we even have a 'next'??
    if (idxNext > idxCurrent) {
        nextActive = GetDocumentFromIndex(idxNext);
    } else if (PreviousDocumentIndex(idxCurrent) != idxCurrent) {
        nextActive = GetDocumentFromIndex(PreviousDocumentIndex(idxCurrent));
    }

    workspace->RemoveOpenDocument(model);
    model->Close();

    if (nextActive != nullptr) {
        SetActiveDocument(nextActive);
    }

    if (RuntimeConfig::Instance().HasRootView()) {
        RuntimeConfig::Instance().GetRootView().Initialize();
    }

    return true;
}







