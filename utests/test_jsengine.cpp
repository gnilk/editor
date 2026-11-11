//
// Created by gnilk on 02.05.23.
//
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/JSEngine/JSPluginEngine.h"
#include "Core/RuntimeConfig.h"
#include "Core/Config/Config.h"
#include "Core/API/ConfigAPI.h"
#include "Core/Editor/Controllers/TerminalController.h"
#include "Core/UnicodeHelper.h"
#include "Core/Runloop.h"
#include "Core/UI/UIHost.h"
#include "Core/UI/Views/RootView.h"
#include "Core/Plugins/PluginExecutor.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_jsengine(ITesting *t);
    DLL_EXPORT int test_jsengine_init(ITesting *t);
    DLL_EXPORT int test_jsengine_builtin(ITesting *t);
    DLL_EXPORT int test_jsengine_console(ITesting *t);
    DLL_EXPORT int test_jsengine_array(ITesting *t);
    DLL_EXPORT int test_jsengine_listlang(ITesting *t);
    DLL_EXPORT int test_jsengine_newbuffer(ITesting *t);
    DLL_EXPORT int test_jsengine_newdocumentfromtext(ITesting *t);
    DLL_EXPORT int test_jsengine_terminalapi(ITesting *t);
    DLL_EXPORT int test_jsengine_copytoclipboard(ITesting *t);
    DLL_EXPORT int test_jsengine_loadbuffer(ITesting *t);
    DLL_EXPORT int test_jsengine_listbuffers(ITesting *t);
    DLL_EXPORT int test_jsengine_configcmd(ITesting *t);
    DLL_EXPORT int test_jsengine_themecmd(ITesting *t);
    DLL_EXPORT int test_jsengine_closecmd(ITesting *t);
    DLL_EXPORT int test_jsengine_quitcmd(ITesting *t);
    DLL_EXPORT int test_jsengine_bangsuffix(ITesting *t);
    DLL_EXPORT int test_jsengine_reloadcmd(ITesting *t);
    DLL_EXPORT int test_jsengine_savecmd(ITesting *t);
    DLL_EXPORT int test_jsengine_closeallcmd(ITesting *t);
    DLL_EXPORT int test_jsengine_fmfilecmd(ITesting *t);
    DLL_EXPORT int test_jsengine_fmblockcmd(ITesting *t);
    DLL_EXPORT int test_jsengine_fmyaml(ITesting *t);
}

DLL_EXPORT int test_jsengine(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_jsengine_init(ITesting *t) {
    JSPluginEngine jsEngine;
    if (!jsEngine.Initialize()) {
        return kTR_Fail;
    }
    return kTR_Pass;
}

DLL_EXPORT int test_jsengine_builtin(ITesting *t) {
    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("setlanguage"));
    auto cmd = RuntimeConfig::Instance().GetPluginCommand("setlanguage");
    TR_ASSERT(t, cmd != nullptr);
    cmd->Execute({".cpp"});
    return kTR_Pass;
}

DLL_EXPORT int test_jsengine_console(ITesting *t) {
    JSPluginEngine jsEngine;
    jsEngine.Initialize();

    std::string script = "function main(args) {"\
    "Console.WriteLine(\"Hello\");"\
    "}";
    jsEngine.RunScriptOnce(script, {});

    return kTR_Pass;
}
DLL_EXPORT int test_jsengine_array(ITesting *t) {
    JSPluginEngine jsEngine;
    jsEngine.Initialize();

    std::string script = "function main(args) {"\
    "Console.WriteLine(\"Before\");"\
    "var v = Editor.GetTestArray();" \
    "Console.WriteLine(\"After\");"\
    "Console.WriteLine(\"len: \", v.length);"\
    "for(i=0; i<v.length;i++) { Console.WriteLine(v[i]); }"\
    "}";
    jsEngine.RunScriptOnce(script, {});

    return kTR_Pass;
}
DLL_EXPORT int test_jsengine_listlang(ITesting *t) {
    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("listlanguages"));
    auto cmd = RuntimeConfig::Instance().GetPluginCommand("listlanguages");
    TR_ASSERT(t, cmd != nullptr);
    cmd->Execute({});
    return kTR_Pass;
}

DLL_EXPORT int test_jsengine_newbuffer(ITesting *t) {
    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("newbuffer"));
    auto cmd = RuntimeConfig::Instance().GetPluginCommand("newbuffer");
    TR_ASSERT(t, cmd != nullptr);
    auto numBefore = Editor::Instance().GetDocuments().size();
    cmd->Execute({"mamma"});
    auto numAfter = Editor::Instance().GetDocuments().size();
    TR_ASSERT(t, numAfter > numBefore);
    return kTR_Pass;
}

// End-to-end for the TS-2c JS seam: Editor.NewDocumentFromText (a 2-arg const char* method, dukglue-
// registered) must create a new document AND pre-fill its buffer from the '\n'-split text. This is the
// back half of "last output to a buffer" (the front half, the terminal's last output, is covered at the
// controller level by test_terminalcontroller_commitline_marks_output).
DLL_EXPORT int test_jsengine_newdocumentfromtext(ITesting *t) {
    JSPluginEngine jsEngine;
    jsEngine.Initialize();

    auto numBefore = Editor::Instance().GetDocuments().size();
    std::string script = "function main(args) {"\
    "Editor.NewDocumentFromText(\"blockout\", \"alpha\\nbeta\\ngamma\");"\
    "}";
    jsEngine.RunScriptOnce(script, {});

    TR_ASSERT(t, Editor::Instance().GetDocuments().size() > numBefore);

    auto doc = Editor::Instance().GetActiveDocument();
    TR_ASSERT(t, doc != nullptr);
    auto buffer = doc->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);
    TR_ASSERT(t, buffer->NumLines() == 3);
    TR_ASSERT(t, buffer->LineAt(0)->Buffer() == std::u32string(U"alpha"));
    TR_ASSERT(t, buffer->LineAt(1)->Buffer() == std::u32string(U"beta"));
    TR_ASSERT(t, buffer->LineAt(2)->Buffer() == std::u32string(U"gamma"));

    return kTR_Pass;
}

// A command run in a controller with no shell: the prompt, the commit, then the shell's echo, output and next
// prompt as if off the pty. Its last output is "alpha\nbeta".
static void RunCommandWithOutput(TerminalController &controller) {
    auto feed = [&controller](const std::string &bytes) {
        controller.HandleTerminalData(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
    };
    feed("$ ");
    controller.GetInputLine()->Append(std::u32string(U"cmd1"));
    controller.CommitLine();
    feed("cmd1\r\nalpha\r\nbeta\r\n$ ");
}

// End-to-end for the `Terminal` JS surface (TerminalAPI/TerminalAPIWrapper). A real TerminalController is
// registered as the output console so TerminalAPI - which reads RuntimeConfig::OutputConsole() - resolves against
// it; a plugin script opens the last output as a document. Asserted through the new document's content.
DLL_EXPORT int test_jsengine_terminalapi(ITesting *t) {
    TerminalController controller;
    controller.Resize(20, 5);
    RunCommandWithOutput(controller);

    auto *prevConsole = RuntimeConfig::Instance().OutputConsole();
    controller.RegisterAsOutputConsole();   // IOutputConsole is a private base; the controller upcasts

    JSPluginEngine jsEngine;
    jsEngine.Initialize();

    auto numBefore = Editor::Instance().GetDocuments().size();
    std::string script = "function main(args) {"\
    "if (Terminal.GetLastOutput() === null) { return; }"\
    "Terminal.OpenLastOutputAsDocument();"\
    "}";
    jsEngine.RunScriptOnce(script, {});

    RuntimeConfig::Instance().SetOutputConsole(prevConsole);   // restore for the rest of the suite

    TR_ASSERT(t, Editor::Instance().GetDocuments().size() > numBefore);
    auto doc = Editor::Instance().GetActiveDocument();
    TR_ASSERT(t, doc != nullptr);
    auto buffer = doc->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);
    TR_ASSERT(t, buffer->NumLines() == 2);
    TR_ASSERT(t, buffer->LineAt(0)->Buffer() == std::u32string(U"alpha"));
    TR_ASSERT(t, buffer->LineAt(1)->Buffer() == std::u32string(U"beta"));

    return kTR_Pass;
}

// The clipboard JS seam: Editor.CopyToClipboard(text) (the general primitive) and
// Terminal.CopyLastOutputToClipboard() (the terminal composite). Both land literal text on the paste buffer
// (RuntimeConfig clipboard top item, AsText round-tripping the text). The OnUpdate->OS-pasteboard
// wiring is covered at the clipboard level by test_clipboard_copytext_notifies.
DLL_EXPORT int test_jsengine_copytoclipboard(ITesting *t) {
    // Editor.CopyToClipboard primitive: arbitrary text lands on the paste buffer.
    {
        JSPluginEngine jsEngine;
        jsEngine.Initialize();
        std::string script = "function main(args) {"\
        "Editor.CopyToClipboard(\"hello\\nworld\");"\
        "}";
        jsEngine.RunScriptOnce(script, {});

        auto top = Editor::Instance().GetClipBoard().Top();
        TR_ASSERT(t, top != nullptr);
        TR_ASSERT(t, top->AsText() == std::u32string(U"hello\nworld"));
    }

    // Terminal.CopyLastOutputToClipboard composite: the last output -> paste buffer
    {
        TerminalController controller;
        controller.Resize(20, 5);
        RunCommandWithOutput(controller);

        auto *prevConsole = RuntimeConfig::Instance().OutputConsole();
        controller.RegisterAsOutputConsole();

        JSPluginEngine jsEngine;
        jsEngine.Initialize();
        std::string script = "function main(args) {"\
        "Terminal.CopyLastOutputToClipboard();"\
        "}";
        jsEngine.RunScriptOnce(script, {});

        RuntimeConfig::Instance().SetOutputConsole(prevConsole);

        auto top = Editor::Instance().GetClipBoard().Top();
        TR_ASSERT(t, top != nullptr);
        TR_ASSERT(t, top->AsText() == std::u32string(U"alpha\nbeta"));
    }

    return kTR_Pass;
}

DLL_EXPORT int test_jsengine_loadbuffer(ITesting *t) {
    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("openfile"));
    auto cmd = RuntimeConfig::Instance().GetPluginCommand("openfile");
    TR_ASSERT(t, cmd != nullptr);
    // Don't assert on a raw count delta - some other test (or an earlier call to this same case, see
    // test_jsengine_listbuffers) may have already opened this fixture in the shared Editor singleton,
    // in which case re-opening it correctly reuses the existing Document (open-bugs.md #4) and the
    // count doesn't change. Assert the real invariant instead: the file ends up open.
    auto res = cmd->Execute({"testfiles/ConvertUTF.cpp"});
    auto isOpen = false;
    for (auto &document : Editor::Instance().GetDocuments()) {
        if (document->GetPath().filename() == "ConvertUTF.cpp") {
            isOpen = true;
            break;
        }
    }
    TR_ASSERT(t, isOpen);
    return kTR_Pass;
}

DLL_EXPORT int test_jsengine_listbuffers(ITesting *t) {
    // depends on loadbuffer having run first
    TR_ASSERT(t, test_jsengine_loadbuffer(t) == kTR_Pass);

    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("listbuffers"));
    auto cmd = RuntimeConfig::Instance().GetPluginCommand("listbuffers");
    TR_ASSERT(t, cmd != nullptr);
    cmd->Execute({});
    return kTR_Pass;
}

// Captures what a script writes through Console.WriteLine
namespace {
    class CaptureConsole : public IOutputConsole {
    public:
        void WriteLine(const std::u32string &str) override {
            lines.push_back(UnicodeHelper::utf32to8(str));
        }
        bool Contains(const std::string &text) const {
            for (auto &line : lines) {
                if (line.find(text) != std::string::npos) {
                    return true;
                }
            }
            return false;
        }
        std::vector<std::string> lines;
    };
}

// The '.cfg' plugin end-to-end (script -> Config wrapper -> ConfigAPI), at the project level only.
DLL_EXPORT int test_jsengine_configcmd(ITesting *t) {
    auto root = std::filesystem::temp_directory_path() / "goatedit_cfgcmd_test";
    auto projectDir = root / ".goatedit";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(projectDir);
    {
        std::ofstream f(projectDir / "config.yml");
        f << "# keep\ncfgjs:\n  value: one\n";
    }
    RuntimeConfig::Instance().GetAssetLoader().ReplaceSearchPath(projectDir, AssetLoaderBase::kLocationType::kProject);

    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("cfg"));
    auto cmd = RuntimeConfig::Instance().GetPluginCommand("cfg");
    auto *prevConsole = RuntimeConfig::Instance().OutputConsole();
    CaptureConsole console;
    RuntimeConfig::Instance().SetOutputConsole(&console);

    TR_ASSERT(t, cmd->Execute({"--project", "get", "cfgjs.value"}));
    TR_ASSERT(t, console.Contains("cfgjs.value = one"));

    // The value is everything after the key (the command line is split on spaces)
    console.lines.clear();
    TR_ASSERT(t, cmd->Execute({"--project", "set", "cfgjs.value", "two", "words"}));
    TR_ASSERT(t, console.Contains("written to"));
    std::ifstream f(projectDir / "config.yml");
    std::stringstream buffer;
    buffer << f.rdbuf();
    TR_ASSERT(t, buffer.str() == "# keep\ncfgjs:\n  value: \"two words\"\n");

    console.lines.clear();
    TR_ASSERT(t, cmd->Execute({"--project", "list", "cfgjs"}));
    TR_ASSERT(t, console.Contains("cfgjs.value = two words"));

    // Unknown action -> usage
    console.lines.clear();
    TR_ASSERT(t, cmd->Execute({"bogus"}));
    TR_ASSERT(t, console.Contains("use: .cfg"));

    RuntimeConfig::Instance().SetOutputConsole(prevConsole);
    Config::Instance().RemoveKey("cfgjs");
    std::filesystem::remove_all(root, ec);
    return kTR_Pass;
}

// '.theme --import' writes into the user config dir - pointed at a temp dir here, never the real one
DLL_EXPORT int test_jsengine_themecmd(ITesting *t) {
    auto root = std::filesystem::temp_directory_path() / "goatedit_themecmd_test";
    auto userDir = root / "user";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(userDir);
    {
        std::ofstream f(userDir / "config.yml");
        f << "# user\n";
    }
    {
        std::ofstream f(root / "My Theme.json");
        f << "{ \"name\": \"My Theme\", // comment\n"
             "  \"colors\": { \"editor.background\": \"#102030\", },\n"
             "  \"tokenColors\": [ { \"scope\": \"keyword\", \"settings\": { \"foreground\": \"#c080ff\" } } ] }\n";
    }
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    assetLoader.ReplaceSearchPath(userDir, AssetLoaderBase::kLocationType::kUser);
    // Refuse to run unless the import really lands in the temp dir
    auto userConfig = ConfigAPI().ConfigFilePath(ConfigAPI::kLevel::kUser);
    TR_ASSERT(t, userConfig.parent_path() == userDir);

    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("theme"));
    auto cmd = RuntimeConfig::Instance().GetPluginCommand("theme");
    auto *prevConsole = RuntimeConfig::Instance().OutputConsole();
    CaptureConsole console;
    RuntimeConfig::Instance().SetOutputConsole(&console);

    // The command line is split on spaces - the file name is glued back together
    auto pathStart = (root / "My").string();
    TR_ASSERT(t, cmd->Execute({"--import", pathStart, "Theme.json"}));
    TR_ASSERT(t, console.Contains("Imported 'My Theme'"));
    TR_ASSERT(t, std::filesystem::exists(userDir / "my-theme.theme.yml"));
    TR_ASSERT(t, std::filesystem::exists(userDir / "my-theme.colors.json"));

    // Again: refused unless forced
    console.lines.clear();
    TR_ASSERT(t, cmd->Execute({"--import", pathStart, "Theme.json"}));
    TR_ASSERT(t, console.Contains("already exists"));
    console.lines.clear();
    TR_ASSERT(t, cmd->Execute({"--import", pathStart, "Theme.json", "--force"}));
    TR_ASSERT(t, console.Contains("Imported"));

    // Quoted, with a name of its own (kept as given)
    console.lines.clear();
    TR_ASSERT(t, cmd->Execute({"--import", "\"" + pathStart, "Theme.json\"", "--name", "my_theme"}));
    TR_ASSERT(t, std::filesystem::exists(userDir / "my_theme.theme.yml"));

    console.lines.clear();
    TR_ASSERT(t, cmd->Execute({"bogus"}));
    TR_ASSERT(t, console.Contains("use: .theme"));

    RuntimeConfig::Instance().SetOutputConsole(prevConsole);
    assetLoader.ReplaceSearchPath(Editor::GetDefaultUserConfigDir(), AssetLoaderBase::kLocationType::kUser);
    std::filesystem::remove_all(root, ec);
    return kTR_Pass;
}

namespace {
    // The confirm dialogs need a modal host: a root view standing in for the app's, restored after
    struct ModalHostFixture {
        ModalHostFixture() {
            if (UIHost::Instance().GetScreen() == nullptr) {
                Editor::Instance().OpenScreen();
            }
            prevRootView = RuntimeConfig::Instance().HasRootView() ? &RuntimeConfig::Instance().GetRootView() : nullptr;
            rootView.Initialize();
            RuntimeConfig::Instance().SetRootView(&rootView);
        }
        ~ModalHostFixture() {
            RuntimeConfig::Instance().SetRootView(prevRootView);
        }
        RootView rootView;
        ViewBase *prevRootView = nullptr;
    };

    KeyPress Letter(char32_t key) {
        KeyPress keyPress;
        keyPress.isKeyValid = true;
        keyPress.key = key;
        return keyPress;
    }

    EditorAction Escape() {
        EditorAction action;
        action.uiAction = kUIAction::kUIActionCloseModal;
        return action;
    }
}

// '.cb': a buffer with unsaved changes asks Save / Discard / Cancel ('!' closes at once); a closed buffer
// that never reached disk leaves the workspace tree, one backed by a file stays.
DLL_EXPORT int test_jsengine_closecmd(ITesting *t) {
    namespace fs = std::filesystem;
    auto root = fs::temp_directory_path() / "goatedit_closecmd_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);
    auto pathGhost = (root / "ghost.cpp").lexically_normal();
    auto pathGhost2 = (root / "ghost2.cpp").lexically_normal();
    auto pathOnDisk = (root / "ondisk.cpp").lexically_normal();
    {
        std::ofstream f(pathOnDisk);
        f << "int x;\n";
    }

    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("cb"));
    auto cmd = RuntimeConfig::Instance().GetPluginCommand("cb");
    auto workspace = Editor::Instance().GetWorkspace();
    ModalHostFixture host;
    auto &rootView = host.rootView;

    // Never saved + edited: asks; Esc keeps it open
    auto ghost = workspace->NewDocumentWithFileRef(pathGhost);
    auto ghostDoc = Editor::Instance().OpenDocumentFromWorkspace(ghost);
    TR_ASSERT(t, ghostDoc != nullptr);
    ghostDoc->GetTextBuffer()->AddLineUTF8("edited");
    TR_ASSERT(t, ghostDoc->IsModified());

    TR_ASSERT(t, cmd->Execute({}));
    TR_ASSERT(t, rootView.HasModal());
    TR_ASSERT(t, Editor::Instance().IsDocumentOpen(ghostDoc));
    rootView.OnAction(Escape());
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, Editor::Instance().IsDocumentOpen(ghostDoc));

    // ...'D'iscard closes it - and its node is gone
    TR_ASSERT(t, cmd->Execute({}));
    rootView.HandleKeyPress(Letter('d'));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, !Editor::Instance().IsDocumentOpen(ghostDoc));
    auto ghostParent = ghost->GetParent();
    TR_ASSERT(t, (ghostParent != nullptr) && !ghostParent->HasChild(ghost));

    // '!' closes without asking
    auto ghost2 = workspace->NewDocumentWithFileRef(pathGhost2);
    auto ghost2Doc = Editor::Instance().OpenDocumentFromWorkspace(ghost2);
    ghost2Doc->GetTextBuffer()->AddLineUTF8("edited");
    TR_ASSERT(t, cmd->Execute({"!"}));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, !Editor::Instance().IsDocumentOpen(ghost2Doc));

    // A file on disk, edited: 'S'ave writes it, then closes; its node stays
    auto onDisk = workspace->NewDocumentWithFileRef(pathOnDisk);
    auto onDiskDoc = Editor::Instance().OpenDocumentFromWorkspace(onDisk);
    TR_ASSERT(t, onDiskDoc != nullptr);
    onDiskDoc->GetTextBuffer()->AddLineUTF8("edited");
    TR_ASSERT(t, cmd->Execute({}));
    TR_ASSERT(t, rootView.HasModal());
    rootView.HandleKeyPress(Letter('s'));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, !Editor::Instance().IsDocumentOpen(onDiskDoc));
    TR_ASSERT(t, onDisk->GetParent()->HasChild(onDisk));
    {
        std::ifstream f(pathOnDisk);
        std::stringstream buffer;
        buffer << f.rdbuf();
        TR_ASSERT(t, buffer.str().find("edited") != std::string::npos);
    }

    // Unmodified: closes without asking
    auto onDiskAgain = Editor::Instance().OpenDocumentFromWorkspace(onDisk);
    TR_ASSERT(t, onDiskAgain != nullptr);
    TR_ASSERT(t, !onDiskAgain->IsModified());
    TR_ASSERT(t, cmd->Execute({}));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, !Editor::Instance().IsDocumentOpen(onDiskAgain));

    fs::remove_all(root, ec);
    return kTR_Pass;
}

// '.q' with a buffer that has unsaved changes asks; Cancel keeps everything (exiting isn't testable here -
// it stops the run loop for good)
DLL_EXPORT int test_jsengine_quitcmd(ITesting *t) {
    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("q"));
    auto cmd = RuntimeConfig::Instance().GetPluginCommand("q");
    ModalHostFixture host;
    auto &rootView = host.rootView;

    auto node = Editor::Instance().GetWorkspace()->NewDocument("quitcmd.txt");
    auto doc = Editor::Instance().OpenDocumentFromWorkspace(node);
    TR_ASSERT(t, doc != nullptr);
    doc->GetTextBuffer()->AddLineUTF8("edited");
    TR_ASSERT(t, doc->IsModified());

    TR_ASSERT(t, cmd->Execute({}));
    TR_ASSERT(t, rootView.HasModal());
    rootView.HandleKeyPress(Letter('c'));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, !Runloop::IsQuitRequested());
    TR_ASSERT(t, Editor::Instance().IsDocumentOpen(doc));

    Editor::Instance().CloseDocument(doc);
    return kTR_Pass;
}

// '!' at the end of a command's name is its first argument (docs/usability-round.md §5): '.cb!' as '.cb !'
DLL_EXPORT int test_jsengine_bangsuffix(ITesting *t) {
    using Words = std::vector<std::string>;
    TR_ASSERT(t, (PluginExecutor::SplitCommandLine("cb!") == Words{"cb", "!"}));
    TR_ASSERT(t, (PluginExecutor::SplitCommandLine("cb !") == Words{"cb", "!"}));
    TR_ASSERT(t, (PluginExecutor::SplitCommandLine("cb") == Words{"cb"}));
    TR_ASSERT(t, (PluginExecutor::SplitCommandLine("sa! name.txt") == Words{"sa", "!", "name.txt"}));
    // Only the name's: an argument ending in '!' is left as it is
    TR_ASSERT(t, (PluginExecutor::SplitCommandLine("o wow!") == Words{"o", "wow!"}));
    // A name that is only '!' is a name, and no command has it
    TR_ASSERT(t, (PluginExecutor::SplitCommandLine("!") == Words{"!"}));
    TR_ASSERT(t, (PluginExecutor::SplitCommandLine("cb  !") == Words{"cb", "!"}));
    TR_ASSERT(t, PluginExecutor::SplitCommandLine("").empty());
    TR_ASSERT(t, PluginExecutor::SplitCommandLine("   ").empty());
    return kTR_Pass;
}

namespace {
    // Another program rewrites the file - a later modification time, so the document sees the change
    void RewriteFile(const std::filesystem::path &path, const std::string &text) {
        {
            std::ofstream f(path, std::ios::binary | std::ios::trunc);
            f << text;
        }
        static int numWrites = 0;
        numWrites++;
        std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now() + std::chrono::seconds(numWrites));
    }

    std::string ReadFile(const std::filesystem::path &path) {
        std::ifstream f(path);
        std::stringstream buffer;
        buffer << f.rdbuf();
        return buffer.str();
    }

    std::string FirstLine(const Document::Ref &doc) {
        return UnicodeHelper::utf32to8(doc->GetTextBuffer()->LineAt(0)->Buffer());
    }

    // An edit as typing makes one: the first line replaced
    void EditFirstLine(const Document::Ref &doc, const std::u32string &text) {
        auto line = doc->GetTextBuffer()->LineAt(0);
        line->Delete(0, (int)line->Length());
        line->Append(text);
    }
}

// '.reload' / '.rl': unedited, it takes the file at once; edited, it asks - Cancel keeps the edits, Reload drops them;
// '.rl!' doesn't ask
DLL_EXPORT int test_jsengine_reloadcmd(ITesting *t) {
    namespace fs = std::filesystem;
    auto root = fs::canonical(fs::temp_directory_path()) / "goatedit_reloadcmd_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);
    auto path = root / "file.txt";
    RewriteFile(path, "first\n");

    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("rl"));
    auto cmd = RuntimeConfig::Instance().GetPluginCommand("rl");
    ModalHostFixture host;
    auto &rootView = host.rootView;
    auto doc = Editor::Instance().OpenDocumentFromWorkspace(Editor::Instance().GetWorkspace()->NewDocumentWithFileRef(path));
    TR_ASSERT(t, doc != nullptr);

    // Unedited: no question
    RewriteFile(path, "second\n");
    TR_ASSERT(t, cmd->Execute({}));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, FirstLine(doc) == "second");

    // Edited: asks; Cancel keeps the edit
    EditFirstLine(doc, U"mine");
    RewriteFile(path, "third\n");
    TR_ASSERT(t, cmd->Execute({}));
    TR_ASSERT(t, rootView.HasModal());
    rootView.HandleKeyPress(Letter('c'));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, FirstLine(doc) == "mine");
    // ...'R'eload drops it
    TR_ASSERT(t, cmd->Execute({}));
    rootView.HandleKeyPress(Letter('r'));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, FirstLine(doc) == "third");
    TR_ASSERT(t, !doc->IsModified());

    // '.rl!' through the command line: no question
    EditFirstLine(doc, U"mine again");
    RewriteFile(path, "fourth\n");
    TR_ASSERT(t, PluginExecutor::ParseAndExecuteWithCmdPrefix(U".rl!"));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, FirstLine(doc) == "fourth");

    Editor::Instance().CloseDocument(doc);
    fs::remove_all(root, ec);
    return kTR_Pass;
}

// '.s' over a file another program rewrote asks: Cancel writes nothing, Overwrite writes, Reload takes theirs; '.s!'
// and a file nobody else touched save without asking
DLL_EXPORT int test_jsengine_savecmd(ITesting *t) {
    namespace fs = std::filesystem;
    auto root = fs::canonical(fs::temp_directory_path()) / "goatedit_savecmd_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);
    auto path = root / "file.txt";
    RewriteFile(path, "first\n");

    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("s"));
    auto cmd = RuntimeConfig::Instance().GetPluginCommand("s");
    ModalHostFixture host;
    auto &rootView = host.rootView;
    auto doc = Editor::Instance().OpenDocumentFromWorkspace(Editor::Instance().GetWorkspace()->NewDocumentWithFileRef(path));
    TR_ASSERT(t, doc != nullptr);

    // Nobody else touched it: saved, no question
    EditFirstLine(doc, U"mine");
    TR_ASSERT(t, cmd->Execute({}));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, ReadFile(path) == "mine\n");

    // Rewritten meanwhile: asks; Cancel writes nothing
    EditFirstLine(doc, U"mine 2");
    RewriteFile(path, "theirs\n");
    TR_ASSERT(t, cmd->Execute({}));
    TR_ASSERT(t, rootView.HasModal());
    rootView.HandleKeyPress(Letter('c'));
    TR_ASSERT(t, ReadFile(path) == "theirs\n");
    TR_ASSERT(t, FirstLine(doc) == "mine 2");
    // ...'O'verwrite writes ours
    TR_ASSERT(t, cmd->Execute({}));
    rootView.HandleKeyPress(Letter('o'));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, ReadFile(path) == "mine 2\n");

    // ...'R'eload takes theirs and writes nothing
    EditFirstLine(doc, U"mine 3");
    RewriteFile(path, "theirs 2\n");
    TR_ASSERT(t, cmd->Execute({}));
    rootView.HandleKeyPress(Letter('r'));
    TR_ASSERT(t, ReadFile(path) == "theirs 2\n");
    TR_ASSERT(t, FirstLine(doc) == "theirs 2");

    // '.s!' doesn't ask
    EditFirstLine(doc, U"mine 4");
    RewriteFile(path, "theirs 3\n");
    TR_ASSERT(t, PluginExecutor::ParseAndExecuteWithCmdPrefix(U".s!"));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, ReadFile(path) == "mine 4\n");

    Editor::Instance().CloseDocument(doc);
    fs::remove_all(root, ec);
    return kTR_Pass;
}

// '.closeall' / '.ca': with an edited buffer among them it asks - Cancel keeps all, Discard closes all and writes
// nothing, Save all writes then closes; '.ca!' and nothing edited close without asking
DLL_EXPORT int test_jsengine_closeallcmd(ITesting *t) {
    namespace fs = std::filesystem;
    auto root = fs::canonical(fs::temp_directory_path()) / "goatedit_closeallcmd_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);
    auto pathA = root / "a.txt";
    auto pathB = root / "b.txt";
    RewriteFile(pathA, "a\n");
    RewriteFile(pathB, "b\n");

    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("ca"));
    auto cmd = RuntimeConfig::Instance().GetPluginCommand("ca");
    ModalHostFixture host;
    auto &rootView = host.rootView;
    auto workspace = Editor::Instance().GetWorkspace();
    auto nodeA = workspace->NewDocumentWithFileRef(pathA);
    auto nodeB = workspace->NewDocumentWithFileRef(pathB);
    auto open = [&]() {
        auto docA = Editor::Instance().OpenDocumentFromWorkspace(nodeA);
        auto docB = Editor::Instance().OpenDocumentFromWorkspace(nodeB);
        return std::make_pair(docA, docB);
    };

    // One edited, one not: asks; Cancel keeps both
    auto [docA, docB] = open();
    TR_ASSERT(t, (docA != nullptr) && (docB != nullptr));
    EditFirstLine(docA, U"edited");
    TR_ASSERT(t, cmd->Execute({}));
    TR_ASSERT(t, rootView.HasModal());
    rootView.HandleKeyPress(Letter('c'));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, Editor::Instance().IsDocumentOpen(docA) && Editor::Instance().IsDocumentOpen(docB));

    // ...'D'iscard closes both, writes nothing
    TR_ASSERT(t, cmd->Execute({}));
    rootView.HandleKeyPress(Letter('d'));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, !Editor::Instance().IsDocumentOpen(docA) && !Editor::Instance().IsDocumentOpen(docB));
    TR_ASSERT(t, Editor::Instance().GetDocuments().empty());
    TR_ASSERT(t, ReadFile(pathA) == "a\n");

    // ...'S'ave all writes, then closes
    std::tie(docA, docB) = open();
    EditFirstLine(docA, U"saved");
    TR_ASSERT(t, cmd->Execute({}));
    rootView.HandleKeyPress(Letter('s'));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, Editor::Instance().GetDocuments().empty());
    TR_ASSERT(t, ReadFile(pathA) == "saved\n");

    // '.ca!' through the command line: no question, nothing written
    std::tie(docA, docB) = open();
    EditFirstLine(docB, U"dropped");
    TR_ASSERT(t, PluginExecutor::ParseAndExecuteWithCmdPrefix(U".ca!"));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, Editor::Instance().GetDocuments().empty());
    TR_ASSERT(t, ReadFile(pathB) == "b\n");

    // Nothing edited: no question
    open();
    TR_ASSERT(t, cmd->Execute({}));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, Editor::Instance().GetDocuments().empty());

    fs::remove_all(root, ec);
    return kTR_Pass;
}

// ---------------------------------------------------------------------------------------------------
// '.fm_file' / '.fm_block' - reformat from the command line (docs/usability-round.md §8)
// ---------------------------------------------------------------------------------------------------
namespace {
    // A file on disk, opened as the active document (the language follows the extension)
    Document::Ref OpenTempFile(const std::string &dirName, const std::string &fileName, const std::string &text) {
        namespace fs = std::filesystem;
        auto root = fs::canonical(fs::temp_directory_path()) / dirName;
        std::error_code ec;
        fs::remove_all(root, ec);
        fs::create_directories(root);
        auto path = root / fileName;
        RewriteFile(path, text);
        auto doc = Editor::Instance().OpenDocumentFromWorkspace(Editor::Instance().GetWorkspace()->NewDocumentWithFileRef(path));
        if (doc != nullptr) {
            gedit::Rect rect(40, 40);
            doc->OnViewInit(rect);
        }
        return doc;
    }

    void CloseTempFile(const Document::Ref &doc, const std::string &dirName) {
        namespace fs = std::filesystem;
        Editor::Instance().CloseDocument(doc);
        std::error_code ec;
        fs::remove_all(fs::canonical(fs::temp_directory_path()) / dirName, ec);
    }

    std::string LineOf(const Document::Ref &doc, size_t idxLine) {
        return UnicodeHelper::utf32to8(doc->GetTextBuffer()->LineAt(idxLine)->Buffer());
    }
}

// '.fm_file' reindents every line (the last one too), and one undo brings the file back
DLL_EXPORT int test_jsengine_fmfilecmd(ITesting *t) {
    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("fm_file"));
    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("fmf"));
    ModalHostFixture host;
    auto doc = OpenTempFile("goatedit_fmfile_test", "file.cpp", "void f() {\na();\nif (x) {\nb();\n}\n     }\n");
    TR_ASSERT(t, doc != nullptr);
    TR_ASSERT(t, doc->IsReindentable());

    TR_ASSERT(t, PluginExecutor::ParseAndExecuteWithCmdPrefix(U".fm_file"));
    TR_ASSERT(t, LineOf(doc, 0) == "void f() {");
    TR_ASSERT(t, LineOf(doc, 1) == "    a();");
    TR_ASSERT(t, LineOf(doc, 3) == "        b();");
    TR_ASSERT(t, LineOf(doc, 4) == "    }");
    TR_ASSERT(t, LineOf(doc, 5) == "}");

    doc->OnAction({gedit::kAction::kActionUndo});
    TR_ASSERT(t, LineOf(doc, 1) == "a();");
    TR_ASSERT(t, LineOf(doc, 5) == "     }");

    CloseTempFile(doc, "goatedit_fmfile_test");
    return kTR_Pass;
}

// '.fm_block' with a selection reindents only the selected lines; without one, the enclosing block
DLL_EXPORT int test_jsengine_fmblockcmd(ITesting *t) {
    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("fmb"));
    ModalHostFixture host;
    auto doc = OpenTempFile("goatedit_fmblock_test", "file.cpp", "void f() {\na();\nb();\n}\nvoid g() {\nc();\n}\n");
    TR_ASSERT(t, doc != nullptr);

    // Shift+Down from the start of line 1: the selection ends at column 0 of line 2, so only line 1 is in it
    doc->SetCursorPosition(1, 0);
    EditorAction shiftDown = {.uiAction = gedit::kUIAction::kUIActionLineDown,
                              .actionModifier = kActionModifier::kActionModifierSelection};
    doc->OnAction(shiftDown);
    TR_ASSERT(t, doc->IsSelectionActive());
    TR_ASSERT(t, PluginExecutor::ParseAndExecuteWithCmdPrefix(U".fmb"));
    TR_ASSERT(t, LineOf(doc, 1) == "    a();");
    TR_ASSERT(t, LineOf(doc, 2) == "b();");

    // No selection, caret in g(): its block, not f()'s
    doc->CancelSelection();
    doc->SetCursorPosition(5, 0);
    TR_ASSERT(t, PluginExecutor::ParseAndExecuteWithCmdPrefix(U".fm_block"));
    TR_ASSERT(t, LineOf(doc, 5) == "    c();");
    TR_ASSERT(t, LineOf(doc, 2) == "b();");

    CloseTempFile(doc, "goatedit_fmblock_test");
    return kTR_Pass;
}

// YAML has reformat off: both commands leave it byte for byte, and the document unmodified
DLL_EXPORT int test_jsengine_fmyaml(ITesting *t) {
    ModalHostFixture host;
    auto doc = OpenTempFile("goatedit_fmyaml_test", "file.yml", "a:\n      b: 1\n  c: |\n        text\n");
    TR_ASSERT(t, doc != nullptr);
    TR_ASSERT(t, !doc->IsReindentable());

    TR_ASSERT(t, PluginExecutor::ParseAndExecuteWithCmdPrefix(U".fmf"));
    TR_ASSERT(t, PluginExecutor::ParseAndExecuteWithCmdPrefix(U".fmb"));
    TR_ASSERT(t, LineOf(doc, 1) == "      b: 1");
    TR_ASSERT(t, LineOf(doc, 2) == "  c: |");
    TR_ASSERT(t, LineOf(doc, 3) == "        text");
    TR_ASSERT(t, !doc->IsModified());

    CloseTempFile(doc, "goatedit_fmyaml_test");
    return kTR_Pass;
}
