//
// Languages as data (docs/support-languages.md): the inherit merge, the definition parse and its problems,
// and the shipped json.yml as the editor registers it.
//

#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/Config/Config.h"
#include "Core/Language/Data/LanguageFileSet.h"
#include "Core/Language/Data/LanguageDefinition.h"
#include "Core/Language/Data/DataLanguage.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_datalanguage(ITesting *t);
DLL_EXPORT int test_datalanguage_merge_listreplaces(ITesting *t);
DLL_EXPORT int test_datalanguage_merge_append(ITesting *t);
DLL_EXPORT int test_datalanguage_merge_inputsunchanged(ITesting *t);
DLL_EXPORT int test_datalanguage_inherit_chain(ITesting *t);
DLL_EXPORT int test_datalanguage_inherit_cycle(ITesting *t);
DLL_EXPORT int test_datalanguage_inherit_twice(ITesting *t);
DLL_EXPORT int test_datalanguage_inherit_unknownparent(ITesting *t);
DLL_EXPORT int test_datalanguage_defaultoverrides(ITesting *t);
DLL_EXPORT int test_datalanguage_parse_problems(ITesting *t);
DLL_EXPORT int test_datalanguage_parse_mixedwholeword(ITesting *t);
DLL_EXPORT int test_datalanguage_longestfirst(ITesting *t);
DLL_EXPORT int test_datalanguage_usersettings(ITesting *t);
DLL_EXPORT int test_datalanguage_json_registered(ITesting *t);
}

DLL_EXPORT int test_datalanguage(ITesting *t) {
    return kTR_Pass;
}

static const char *defaultFile = R"yml(
language:
  name: default
  tabsize: 4
  insert_spaces: yes
indent:
  indent_after: ["{", "[", "("]
  dedent_on: ["}", "]", ")"]
)yml";

static std::vector<std::string> AsStrings(const YAML::Node &node) {
    std::vector<std::string> out;
    for (auto element : node) {
        out.push_back(element.as<std::string>());
    }
    return out;
}

// A list from the child replaces the parent's whole - not by position, as YAMLMerge does (open-bugs #28)
DLL_EXPORT int test_datalanguage_merge_listreplaces(ITesting *t) {
    auto base = YAML::Load("list: [a, b, c]\nkeep: 1");
    auto child = YAML::Load("list: [x]");
    auto merged = LanguageFileSet::Merge(base, child);
    TR_ASSERT(t, AsStrings(merged["list"]) == std::vector<std::string>({"x"}));
    TR_ASSERT(t, merged["keep"].as<int>() == 1);
    return kTR_Pass;
}

DLL_EXPORT int test_datalanguage_merge_append(ITesting *t) {
    auto base = YAML::Load("section: { list: [a, b], other: 1 }");
    auto child = YAML::Load("section: { list+: [c], other: 2 }");
    auto merged = LanguageFileSet::Merge(base, child);
    TR_ASSERT(t, AsStrings(merged["section"]["list"]) == std::vector<std::string>({"a", "b", "c"}));
    TR_ASSERT(t, merged["section"]["other"].as<int>() == 2);
    TR_ASSERT(t, !merged["section"]["list+"].IsDefined());

    // Nothing to add to: the child's list
    auto merged2 = LanguageFileSet::Merge(YAML::Load("a: 1"), YAML::Load("list+: [z]"));
    TR_ASSERT(t, AsStrings(merged2["list"]) == std::vector<std::string>({"z"}));
    return kTR_Pass;
}

// The merged tree is new: neither the parent's nor the child's nodes are written into
DLL_EXPORT int test_datalanguage_merge_inputsunchanged(ITesting *t) {
    auto base = YAML::Load("m: { a: 1, list: [p, q] }");
    auto child = YAML::Load("m: { a: 2, list+: [r] }");
    auto merged = LanguageFileSet::Merge(base, child);
    TR_ASSERT(t, merged["m"]["a"].as<int>() == 2);
    TR_ASSERT(t, base["m"]["a"].as<int>() == 1);
    TR_ASSERT(t, base["m"]["list"].size() == 2);
    TR_ASSERT(t, child["m"]["a"].as<int>() == 2);
    TR_ASSERT(t, !child["m"]["list"].IsDefined());
    return kTR_Pass;
}

// Two levels: json <- c_style <- default. A middle file's change reaches its children; the child's own keys win.
DLL_EXPORT int test_datalanguage_inherit_chain(ITesting *t) {
    auto fileSet = LanguageFileSet::Create();
    TR_ASSERT(t, fileSet->AddFromString(defaultFile, "default.yml"));
    TR_ASSERT(t, fileSet->AddFromString(R"yml(
language: { name: c_style, inherit: default }
indent: { dedent_on: ["}"] }
)yml", "c_style.yml"));
    TR_ASSERT(t, fileSet->AddFromString(R"yml(
language: { name: lang, inherit: c_style, tabsize: 2 }
)yml", "lang.yml"));

    auto lang = fileSet->Resolve("lang");
    TR_ASSERT(t, lang.IsDefined());
    TR_ASSERT(t, lang["language"]["name"].as<std::string>() == "lang");
    TR_ASSERT(t, lang["language"]["tabsize"].as<int>() == 2);
    TR_ASSERT(t, lang["language"]["insert_spaces"].as<bool>());
    TR_ASSERT(t, !lang["language"]["inherit"].IsDefined());
    TR_ASSERT(t, AsStrings(lang["indent"]["dedent_on"]) == std::vector<std::string>({"}"}));
    TR_ASSERT(t, AsStrings(lang["indent"]["indent_after"]) == std::vector<std::string>({"{", "[", "("}));
    TR_ASSERT(t, fileSet->Problems().empty());
    return kTR_Pass;
}

// Resolving doesn't change the files: a second set of resolves (after a new file clears the cache) is the same
DLL_EXPORT int test_datalanguage_inherit_twice(ITesting *t) {
    auto fileSet = LanguageFileSet::Create();
    fileSet->AddFromString(defaultFile, "default.yml");
    fileSet->AddFromString("language: { name: lang, inherit: default, tabsize: 2 }", "lang.yml");
    auto first = YAML::Dump(fileSet->Resolve("lang"));
    fileSet->AddFromString("language: { name: other, inherit: lang }", "other.yml");
    auto second = YAML::Dump(fileSet->Resolve("lang"));
    TR_ASSERT(t, first == second);
    TR_ASSERT(t, fileSet->Resolve("other")["language"]["tabsize"].as<int>() == 2);
    TR_ASSERT(t, fileSet->Resolve("default")["language"]["tabsize"].as<int>() == 4);
    return kTR_Pass;
}

DLL_EXPORT int test_datalanguage_inherit_cycle(ITesting *t) {
    auto fileSet = LanguageFileSet::Create();
    fileSet->AddFromString("language: { name: a, inherit: b }", "a.yml");
    fileSet->AddFromString("language: { name: b, inherit: a }", "b.yml");
    TR_ASSERT(t, !fileSet->Resolve("a").IsDefined());
    TR_ASSERT(t, !fileSet->Problems().empty());
    return kTR_Pass;
}

DLL_EXPORT int test_datalanguage_inherit_unknownparent(ITesting *t) {
    auto fileSet = LanguageFileSet::Create();
    fileSet->AddFromString("language:\n  name: a\n  inherit: nothere\n", "a.yml");
    TR_ASSERT(t, !fileSet->Resolve("a").IsDefined());
    TR_ASSERT(t, fileSet->Problems().size() == 1);
    TR_ASSERT(t, fileSet->Problems()[0].origin == "a.yml");
    TR_ASSERT(t, fileSet->Problems()[0].line == 3);
    return kTR_Pass;
}

// config.yml 'languages.default' sits on the root file: it reaches a language without a convention of its own,
// and doesn't flatten one that has (a global tabsize 8 leaves YAML's 2)
DLL_EXPORT int test_datalanguage_defaultoverrides(ITesting *t) {
    auto fileSet = LanguageFileSet::Create();
    fileSet->AddFromString(defaultFile, "default.yml");
    fileSet->AddFromString("language: { name: plain, inherit: default }", "plain.yml");
    fileSet->AddFromString("language: { name: yamlish, inherit: default, tabsize: 2 }", "yamlish.yml");
    fileSet->SetDefaultOverrides(YAML::Load("tabsize: 8"));

    TR_ASSERT(t, fileSet->Resolve("plain")["language"]["tabsize"].as<int>() == 8);
    TR_ASSERT(t, fileSet->Resolve("yamlish")["language"]["tabsize"].as<int>() == 2);
    return kTR_Pass;
}

static std::vector<LanguageDefinition::Problem> ParseProblems(const std::string &yaml) {
    std::vector<LanguageDefinition::Problem> problems;
    auto definition = LanguageDefinition::Parse(YAML::Load(yaml), problems);
    if (definition.has_value()) {
        problems.clear();
    }
    return problems;
}

static bool HasProblemAt(const std::vector<LanguageDefinition::Problem> &problems, int line, const std::string &part) {
    for (auto &problem : problems) {
        if ((problem.line == line) && (problem.reason.find(part) != std::string::npos)) {
            return true;
        }
    }
    return false;
}

// A bad file is rejected whole, each problem with its line
DLL_EXPORT int test_datalanguage_parse_problems(ITesting *t) {
    auto problems = ParseProblems(
R"yml(language:
  name: bad
  extensions: [".bad"]
tokenizer:
  start: main
  states:
    main:
      tokens:
        - { class: keywrd, words: "if" }
        - { class: string, words: "\"", push: in_strng }
        - { class: string, words: "'", colour: red }
      numbers: generic
)yml");
    TR_ASSERT(t, HasProblemAt(problems, 9, "keywrd"));
    TR_ASSERT(t, HasProblemAt(problems, 8, "in_strng"));
    TR_ASSERT(t, HasProblemAt(problems, 11, "colour"));
    TR_ASSERT(t, HasProblemAt(problems, 12, "numbers"));

    // A data language without a tokenizer
    TR_ASSERT(t, !ParseProblems("language: { name: x, extensions: ['.x'] }").empty());
    // A base without extensions needs none
    TR_ASSERT(t, ParseProblems("language: { name: base }").empty());
    return kTR_Pass;
}

// The tokenizer keeps one list per class in a state, so a class's entries must agree on whole_word
DLL_EXPORT int test_datalanguage_parse_mixedwholeword(ITesting *t) {
    auto problems = ParseProblems(R"yml(
language: { name: m, extensions: [".m"] }
tokenizer:
  states:
    main:
      tokens:
        - { class: keyword, whole_word: yes, words: "if" }
        - { class: keyword, words: "else" }
)yml");
    TR_ASSERT(t, !problems.empty());
    return kTR_Pass;
}

static std::vector<std::pair<int, kLanguageTokenClass>> Tokenize(LanguageBase &language, const std::string &utf8) {
    std::vector<std::pair<int, kLanguageTokenClass>> tokens;
    language.Tokenizer().TokenizeText(UnicodeHelper::utf8to32(utf8), [&tokens](size_t, const LangToken &token) {
        tokens.emplace_back(token.idxOrigStr, token.classification);
    });
    return tokens;
}

static DataLanguage::Ref MakeLanguage(ITesting *t, const std::string &yaml) {
    std::vector<LanguageDefinition::Problem> problems;
    YAML::Node root;
    try {
        root = YAML::Load(yaml);
    } catch (const YAML::Exception &e) {
        printf("YAML: %s\n", e.what());
        return nullptr;
    }
    auto definition = LanguageDefinition::Parse(root, problems);
    for (auto &problem : problems) {
        printf("line %d: %s\n", problem.line, problem.reason.c_str());
    }
    if (!definition.has_value()) {
        return nullptr;
    }
    auto language = DataLanguage::Create(std::move(*definition));
    language->Initialize();
    return language;
}

// '"' listed before '"""' still lets '"""' match - the loader sorts longest first
DLL_EXPORT int test_datalanguage_longestfirst(ITesting *t) {
    auto language = MakeLanguage(t, R"yml(
language: { name: lf, extensions: [".lf"] }
tokenizer:
  states:
    main:
      postfix: "\" \"\"\""
      tokens:
        - { class: string, words: "\"", push: in_dq }
        - { class: string, words: "\"\"\"", push: in_tq }
    in_dq:
      regular: string
      tokens:
        - { class: string, words: "\"", pop: yes }
    in_tq:
      regular: commented_text
      tokens:
        - { class: string, words: "\"\"\"", pop: yes }
)yml");
    TR_ASSERT(t, language != nullptr);
    // Inside '"""' a word is commented_text (in_tq); a single-quote string would make it string
    auto tokens = Tokenize(*language, "\"\"\" abc \"\"\"");
    bool sawCommented = false;
    for (auto &[pos, tokenClass] : tokens) {
        if (tokenClass == kLanguageTokenClass::kCommentedText) {
            sawCommented = true;
        }
    }
    TR_ASSERT(t, sawCommented);
    return kTR_Pass;
}

// config.yml 'languages.<name>' wins over the file's settings
DLL_EXPORT int test_datalanguage_usersettings(ITesting *t) {
    auto languages = Config::Instance()["languages"];
    auto hadNode = languages.HasKey("settingstest");
    Config::Instance().SetScalarAtPath({"languages", "settingstest", "tabsize"}, "7");

    auto language = MakeLanguage(t, "language: { name: settingstest, tabsize: 3, insert_spaces: yes }");
    TR_ASSERT(t, language != nullptr);
    auto tabSize = language->GetTabSize();
    auto insertSpaces = language->GetBool("insert_spaces", false);
    if (!hadNode) {
        Config::Instance()["languages"].RemoveKey("settingstest");
    }
    TR_ASSERT(t, tabSize == 7);
    TR_ASSERT(t, insertSpaces);
    return kTR_Pass;
}

// The editor registers json.yml for .json and .js 
DLL_EXPORT int test_datalanguage_json_registered(ITesting *t) {
    auto language = Editor::Instance().GetLanguageForExtension(".json");
    TR_ASSERT(t, language != nullptr);
    TR_ASSERT(t, language->Identifier() == U"json");
    TR_ASSERT(t, std::dynamic_pointer_cast<DataLanguage>(language) != nullptr);
    TR_ASSERT(t, Editor::Instance().GetLanguageForExtension(".js") == language);
    TR_ASSERT(t, language->GetConfigNodeName() == "json");
    TR_ASSERT(t, language->GetTabSize() > 0);

    // '.ll' names the file it came from
    bool isListed = false;
    for (auto &line : Editor::Instance().GetLanguageDescriptions()) {
        if (line.starts_with("json") && (line.find("json.yml") != std::string::npos)) {
            isListed = true;
        }
    }
    TR_ASSERT(t, isListed);
    return kTR_Pass;
}
