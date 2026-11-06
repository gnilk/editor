//
// ExclusionRules: 'workspace.exclude' + '.gitignore' (docs/code-index.md §11.7). The patterns are the ones in
// real use in the projects the index is judged on (§7.1), each pinned to what git itself does. Each folder test
// builds a real tree with real '.gitignore' files.
//
#include <testinterface.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Core/FileSystem/ExclusionRules.h"

using namespace gedit;
namespace fs = std::filesystem;
using kVerdict = ExclusionRules::kVerdict;

extern "C" {
    DLL_EXPORT int test_exclusion(ITesting *t);
    DLL_EXPORT int test_exclusion_parse(ITesting *t);
    DLL_EXPORT int test_exclusion_match_name(ITesting *t);
    DLL_EXPORT int test_exclusion_match_anchored(ITesting *t);
    DLL_EXPORT int test_exclusion_match_doublestar(ITesting *t);
    DLL_EXPORT int test_exclusion_rootonly(ITesting *t);
    DLL_EXPORT int test_exclusion_lastwins(ITesting *t);
    DLL_EXPORT int test_exclusion_parentignored(ITesting *t);
    DLL_EXPORT int test_exclusion_nested(ITesting *t);
    DLL_EXPORT int test_exclusion_kinds(ITesting *t);
    DLL_EXPORT int test_exclusion_off(ITesting *t);
    DLL_EXPORT int test_exclusion_abovetheroot(ITesting *t);
    DLL_EXPORT int test_exclusion_outside(ITesting *t);
    DLL_EXPORT int test_exclusion_invalidate(ITesting *t);
    DLL_EXPORT int test_exclusion_git(ITesting *t);
    DLL_EXPORT int test_exclusion_global(ITesting *t);
}

namespace {
    fs::path MakeRoot(const std::string &name) {
        auto root = fs::canonical(fs::temp_directory_path()) / ("goatedit_exclusion_" + name);
        fs::remove_all(root);
        fs::create_directories(root);
        return root;
    }

    void WriteFile(const fs::path &path, const std::string &text) {
        fs::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file << text;
    }

    bool LineMatches(const std::string &line, const std::string &path, bool isDirectory) {
        ExclusionRules::Pattern pattern;
        if (!ExclusionRules::ParseLine(line, pattern)) {
            return false;
        }
        std::vector<std::string> components;
        size_t idxStart = 0;
        while (idxStart <= path.size()) {
            auto idxEnd = path.find('/', idxStart);
            if (idxEnd == std::string::npos) {
                idxEnd = path.size();
            }
            components.push_back(path.substr(idxStart, idxEnd - idxStart));
            idxStart = idxEnd + 1;
        }
        return ExclusionRules::Matches(pattern, components, isDirectory);
    }
}

DLL_EXPORT int test_exclusion(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_exclusion_parse(ITesting *t) {
    ExclusionRules::Pattern pattern;
    TR_ASSERT(t, !ExclusionRules::ParseLine("", pattern));
    TR_ASSERT(t, !ExclusionRules::ParseLine("   ", pattern));
    TR_ASSERT(t, !ExclusionRules::ParseLine("# a comment", pattern));

    TR_ASSERT(t, ExclusionRules::ParseLine("!keep.h", pattern));
    TR_ASSERT(t, pattern.isNegated);

    // Escaped: literally a '#' and a '!'
    TR_ASSERT(t, ExclusionRules::ParseLine("\\#hash", pattern));
    TR_ASSERT(t, pattern.segments[0] == "#hash");
    TR_ASSERT(t, ExclusionRules::ParseLine("\\!bang", pattern));
    TR_ASSERT(t, !pattern.isNegated);
    TR_ASSERT(t, pattern.segments[0] == "!bang");

    // A real line from PuckoNew, trailing space and all
    TR_ASSERT(t, ExclusionRules::ParseLine("cmake-build-*/ ", pattern));
    TR_ASSERT(t, pattern.isDirectoryOnly);
    TR_ASSERT(t, !pattern.isAnchored);

    TR_ASSERT(t, ExclusionRules::ParseLine("name\r", pattern));
    TR_ASSERT(t, pattern.segments[0] == "name");
    return kTR_Pass;
}

DLL_EXPORT int test_exclusion_match_name(ITesting *t) {
    // No slash: the name, at any depth
    TR_ASSERT(t, LineMatches("*.o", "main.o", false));
    TR_ASSERT(t, LineMatches("*.o", "src/deep/main.o", false));
    TR_ASSERT(t, !LineMatches("*.o", "main.cpp", false));
    TR_ASSERT(t, LineMatches("tags", "a/b/tags", false));

    // Trailing slash: folders only
    TR_ASSERT(t, LineMatches("build/", "build", true));
    TR_ASSERT(t, LineMatches("build/", "src/build", true));
    TR_ASSERT(t, !LineMatches("build/", "build", false));
    TR_ASSERT(t, LineMatches("cmake-build-*/ ", "cmake-build-debug", true));

    TR_ASSERT(t, LineMatches("file?.h", "file1.h", false));
    TR_ASSERT(t, !LineMatches("file?.h", "file12.h", false));
    TR_ASSERT(t, LineMatches("[ab].h", "a.h", false));
    TR_ASSERT(t, !LineMatches("[ab].h", "c.h", false));
    return kTR_Pass;
}

DLL_EXPORT int test_exclusion_match_anchored(ITesting *t) {
    // Leading slash: only here. This repo's own rule - 'src/ext' holds vendored code that must stay
    TR_ASSERT(t, LineMatches("/ext/", "ext", true));
    TR_ASSERT(t, !LineMatches("/ext/", "src/ext", true));

    // A slash in the middle anchors too (vcpu)
    TR_ASSERT(t, LineMatches("src/ext/ELFIO/", "src/ext/ELFIO", true));
    TR_ASSERT(t, !LineMatches("src/ext/ELFIO/", "other/src/ext/ELFIO", true));

    // '*' stays inside one component (noc_goa)
    TR_ASSERT(t, LineMatches("app/osxtest/*_old*", "app/osxtest/engine_old_v2", false));
    TR_ASSERT(t, !LineMatches("app/osxtest/*_old*", "app/osxtest/sub/engine_old_v2", false));
    TR_ASSERT(t, !LineMatches("app/*.h", "app/sub/x.h", false));

    // PuckoNew
    TR_ASSERT(t, LineMatches("/examples/esp32_hello/.idea/workspace.xml", "examples/esp32_hello/.idea/workspace.xml", false));

    // noc_goa has './lib/' - which git never matches, a path has no '.' component
    TR_ASSERT(t, !LineMatches("./lib/", "lib", true));
    return kTR_Pass;
}

DLL_EXPORT int test_exclusion_match_doublestar(ITesting *t) {
    TR_ASSERT(t, LineMatches("**/foo", "foo", false));
    TR_ASSERT(t, LineMatches("**/foo", "a/b/foo", false));
    TR_ASSERT(t, LineMatches("a/**/b", "a/b", false));
    TR_ASSERT(t, LineMatches("a/**/b", "a/x/y/b", false));
    TR_ASSERT(t, !LineMatches("a/**/b", "x/a/b", false));
    TR_ASSERT(t, LineMatches("abc/**", "abc/x", false));
    TR_ASSERT(t, LineMatches("abc/**", "abc/x/y", false));
    TR_ASSERT(t, !LineMatches("abc/**", "abc", true));
    return kTR_Pass;
}

DLL_EXPORT int test_exclusion_rootonly(ITesting *t) {
    auto root = MakeRoot("rootonly");
    WriteFile(root / ".gitignore", "/ext/\nbuild/\n*.o\n");
    fs::create_directories(root / "ext");
    fs::create_directories(root / "src/ext");

    auto rules = ExclusionRules::Create(root, {}, true);
    TR_ASSERT(t, rules->Check(root / "ext", true) == kVerdict::kIgnored);
    TR_ASSERT(t, rules->Check(root / "ext/lib/dep.h", false) == kVerdict::kIgnored);
    TR_ASSERT(t, rules->Check(root / "src/ext", true) == kVerdict::kIncluded);
    TR_ASSERT(t, rules->Check(root / "src/ext/duktape.c", false) == kVerdict::kIncluded);
    TR_ASSERT(t, rules->Check(root / "src/build/x.h", false) == kVerdict::kIgnored);
    TR_ASSERT(t, rules->Check(root / "src/main.o", false) == kVerdict::kIgnored);
    TR_ASSERT(t, rules->Check(root / "src/main.cpp", false) == kVerdict::kIncluded);
    // A file called 'build' isn't a folder
    TR_ASSERT(t, rules->Check(root / "src/build", false) == kVerdict::kIncluded);
    fs::remove_all(root);
    return kTR_Pass;
}

DLL_EXPORT int test_exclusion_lastwins(ITesting *t) {
    auto root = MakeRoot("lastwins");
    WriteFile(root / ".gitignore", "*.h\n!keep.h\n");
    auto rules = ExclusionRules::Create(root, {}, true);
    TR_ASSERT(t, rules->Check(root / "a.h", false) == kVerdict::kIgnored);
    TR_ASSERT(t, rules->Check(root / "keep.h", false) == kVerdict::kIncluded);

    auto reversed = MakeRoot("lastwins2");
    WriteFile(reversed / ".gitignore", "!keep.h\n*.h\n");
    auto reversedRules = ExclusionRules::Create(reversed, {}, true);
    TR_ASSERT(t, reversedRules->Check(reversed / "keep.h", false) == kVerdict::kIgnored);
    fs::remove_all(root);
    fs::remove_all(reversed);
    return kTR_Pass;
}

// git: 'It is not possible to re-include a file if a parent directory of that file is excluded'
DLL_EXPORT int test_exclusion_parentignored(ITesting *t) {
    auto root = MakeRoot("parentignored");
    WriteFile(root / ".gitignore", "build/\n!build/keep.h\n");
    auto rules = ExclusionRules::Create(root, {}, true);
    TR_ASSERT(t, rules->Check(root / "build/keep.h", false) == kVerdict::kIgnored);

    // The usual workaround works: ignore what's in it, not the folder
    auto workaround = MakeRoot("parentignored2");
    WriteFile(workaround / ".gitignore", "build/*\n!build/keep.h\n");
    auto workaroundRules = ExclusionRules::Create(workaround, {}, true);
    TR_ASSERT(t, workaroundRules->Check(workaround / "build/keep.h", false) == kVerdict::kIncluded);
    TR_ASSERT(t, workaroundRules->Check(workaround / "build/other.h", false) == kVerdict::kIgnored);
    fs::remove_all(root);
    fs::remove_all(workaround);
    return kTR_Pass;
}

// PuckoNew has 43 of these
DLL_EXPORT int test_exclusion_nested(ITesting *t) {
    auto root = MakeRoot("nested");
    WriteFile(root / ".gitignore", "*.gen\n");
    WriteFile(root / "sub/.gitignore", "/local/\n!wanted.gen\n");
    fs::create_directories(root / "sub/local");
    fs::create_directories(root / "local");

    auto rules = ExclusionRules::Create(root, {}, true);
    // Anchored to its own folder, not the root's
    TR_ASSERT(t, rules->Check(root / "sub/local", true) == kVerdict::kIgnored);
    TR_ASSERT(t, rules->Check(root / "local", true) == kVerdict::kIncluded);
    TR_ASSERT(t, rules->Check(root / "other/sub/local", true) == kVerdict::kIncluded);
    // The deeper file beats the shallower one - below itself only
    TR_ASSERT(t, rules->Check(root / "sub/wanted.gen", false) == kVerdict::kIncluded);
    TR_ASSERT(t, rules->Check(root / "wanted.gen", false) == kVerdict::kIgnored);
    TR_ASSERT(t, rules->Check(root / "sub/other.gen", false) == kVerdict::kIgnored);
    fs::remove_all(root);
    return kTR_Pass;
}

// The two kinds stay apart: the tree shows one and never the other
DLL_EXPORT int test_exclusion_kinds(ITesting *t) {
    auto root = MakeRoot("kinds");
    WriteFile(root / ".gitignore", "bld/\n.git\n");
    auto rules = ExclusionRules::Create(root, {".git", "cmake-build-*"}, true);
    TR_ASSERT(t, rules->Check(root / ".git", true) == kVerdict::kExcluded);     // both say so - the hard one wins
    TR_ASSERT(t, rules->Check(root / ".git/config", false) == kVerdict::kExcluded);
    TR_ASSERT(t, rules->Check(root / "cmake-build-debug/x/y.o", false) == kVerdict::kExcluded);
    TR_ASSERT(t, rules->Check(root / "bld", true) == kVerdict::kIgnored);
    TR_ASSERT(t, rules->Check(root / ".gitignore", false) == kVerdict::kIncluded);
    TR_ASSERT(t, !rules->IsIncluded(root / "bld/x.h", false));
    fs::remove_all(root);
    return kTR_Pass;
}

DLL_EXPORT int test_exclusion_off(ITesting *t) {
    auto root = MakeRoot("off");
    WriteFile(root / ".gitignore", "*.h\n");
    auto rules = ExclusionRules::Create(root, {"bld"}, false);
    TR_ASSERT(t, rules->Check(root / "a.h", false) == kVerdict::kIncluded);
    TR_ASSERT(t, rules->Check(root / "bld/a.h", false) == kVerdict::kExcluded);
    fs::remove_all(root);
    return kTR_Pass;
}

// A sub folder of a repository opened as the root still follows the repository's rules
DLL_EXPORT int test_exclusion_abovetheroot(ITesting *t) {
    auto top = MakeRoot("above");
    fs::create_directories(top / ".git");
    WriteFile(top / ".gitignore", "*.gen\n/lib/skip/\n");
    fs::create_directories(top / "lib/skip");

    auto rules = ExclusionRules::Create(top / "lib", {}, true);
    TR_ASSERT(t, rules->Check(top / "lib/a.gen", false) == kVerdict::kIgnored);
    TR_ASSERT(t, rules->Check(top / "lib/skip", true) == kVerdict::kIgnored);
    TR_ASSERT(t, rules->Check(top / "lib/a.h", false) == kVerdict::kIncluded);
    fs::remove_all(top);
    return kTR_Pass;
}

DLL_EXPORT int test_exclusion_outside(ITesting *t) {
    auto root = MakeRoot("outside");
    WriteFile(root / ".gitignore", "*\n");
    auto rules = ExclusionRules::Create(root, {}, true);
    TR_ASSERT(t, rules->Check("/somewhere/else.h", false) == kVerdict::kIncluded);
    TR_ASSERT(t, rules->Check(root, true) == kVerdict::kIncluded);      // the root itself
    fs::remove_all(root);
    return kTR_Pass;
}

DLL_EXPORT int test_exclusion_invalidate(ITesting *t) {
    auto root = MakeRoot("invalidate");
    WriteFile(root / ".gitignore", "a.h\n");
    auto rules = ExclusionRules::Create(root, {}, true);
    TR_ASSERT(t, rules->Check(root / "a.h", false) == kVerdict::kIgnored);

    WriteFile(root / ".gitignore", "b.h\n");
    TR_ASSERT(t, rules->Check(root / "a.h", false) == kVerdict::kIgnored);      // read once, kept
    rules->Invalidate(root);
    TR_ASSERT(t, rules->Check(root / "a.h", false) == kVerdict::kIncluded);
    TR_ASSERT(t, rules->Check(root / "b.h", false) == kVerdict::kIgnored);
    fs::remove_all(root);
    return kTR_Pass;
}

// git itself is the reference: every verdict here is checked against 'git check-ignore'
DLL_EXPORT int test_exclusion_git(ITesting *t) {
    auto root = MakeRoot("git");
    WriteFile(root / ".gitignore", "/ext/\ncmake-build-*/ \n*.o\n!keep.o\napp/osxtest/*_old*\nsrc/**/gen\ndocs/*\n!docs/keep/\n");
    WriteFile(root / "src/.gitignore", "/local/\n!wanted.o\n");
    const std::vector<std::pair<std::string, bool>> paths = {
        {"ext/dep.h", false}, {"src/ext/dep.h", false}, {"cmake-build-debug/x.h", false}, {"src/cmake-build-x/y.h", false},
        {"a.o", false}, {"src/keep.o", false}, {"src/wanted.o", false}, {"wanted.o", false}, {"src/local/a.h", false},
        {"local/a.h", false}, {"app/osxtest/e_old_2.c", false}, {"app/osxtest/sub/e_old_2.c", false}, {"src/gen", false},
        {"src/a/b/gen", false}, {"docs/a.md", false}, {"docs/keep/a.md", false}, {"docs/keep", true}, {"main.cpp", false},
    };
    std::string command = "cd '" + root.string() + "' && git init -q . 2>/dev/null";
    TR_ASSERT(t, std::system(command.c_str()) == 0);

    auto rules = ExclusionRules::Create(root, {}, true);
    for (auto &[path, isDirectory] : paths) {
        fs::create_directories(root / fs::path(path).parent_path());
        if (isDirectory) {
            fs::create_directories(root / path);
        } else {
            WriteFile(root / path, "");
        }
        auto gitCommand = "cd '" + root.string() + "' && git check-ignore -q '" + path + (isDirectory ? "/" : "") + "'";
        bool isGitIgnored = std::system(gitCommand.c_str()) == 0;
        bool isIgnored = rules->Check(root / path, isDirectory) == kVerdict::kIgnored;
        if (isGitIgnored != isIgnored) {
            printf("  differs from git: %s - git says %d, we say %d\n", path.c_str(), isGitIgnored, isIgnored);
        }
        TR_ASSERT(t, isGitIgnored == isIgnored);
    }
    fs::remove_all(root);
    return kTR_Pass;
}

// The user's global file: below everything in the tree, and it follows the repository, not the opened folder
DLL_EXPORT int test_exclusion_global(ITesting *t) {
    auto top = MakeRoot("global");
    auto globalFile = MakeRoot("global_file") / "ignore";
    WriteFile(globalFile, "**/.claude/settings.local.json\n*.secret\n/sub/anchored.h\n");
    fs::create_directories(top / ".git");
    WriteFile(top / ".gitignore", "!keep.secret\n");

    auto rules = ExclusionRules::Create(top, {}, true, globalFile);
    TR_ASSERT(t, rules->Check(top / ".claude/settings.local.json", false) == kVerdict::kIgnored);
    TR_ASSERT(t, rules->Check(top / "sub/a.secret", false) == kVerdict::kIgnored);
    TR_ASSERT(t, rules->Check(top / "sub/anchored.h", false) == kVerdict::kIgnored);
    TR_ASSERT(t, rules->Check(top / "other/sub/anchored.h", false) == kVerdict::kIncluded);
    // A file in the tree has the last word
    TR_ASSERT(t, rules->Check(top / "keep.secret", false) == kVerdict::kIncluded);

    // Opened further down, '/sub/...' still means the repository's 'sub'
    auto below = ExclusionRules::Create(top / "sub", {}, true, globalFile);
    TR_ASSERT(t, below->Check(top / "sub/anchored.h", false) == kVerdict::kIgnored);

    auto off = ExclusionRules::Create(top, {}, false, globalFile);
    TR_ASSERT(t, off->Check(top / "a.secret", false) == kVerdict::kIncluded);
    auto none = ExclusionRules::Create(top, {}, true);
    TR_ASSERT(t, none->Check(top / "a.secret", false) == kVerdict::kIncluded);
    auto missing = ExclusionRules::Create(top, {}, true, top / "no/such/file");
    TR_ASSERT(t, missing->Check(top / "a.secret", false) == kVerdict::kIncluded);

    fs::remove_all(top);
    fs::remove_all(globalFile.parent_path());
    return kTR_Pass;
}
