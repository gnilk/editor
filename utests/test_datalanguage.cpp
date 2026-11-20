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
#include "Core/Language/Data/GenericNumberMatcher.h"
#include "Core/UnicodeHelper.h"
#include "Core/Language/IndentCache.h"

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
DLL_EXPORT int test_datalanguage_numbers(ITesting *t);
DLL_EXPORT int test_datalanguage_caseinsensitive(ITesting *t);
DLL_EXPORT int test_datalanguage_cmake_registered(ITesting *t);
DLL_EXPORT int test_datalanguage_cmake_tokens(ITesting *t);
DLL_EXPORT int test_datalanguage_prefix(ITesting *t);
DLL_EXPORT int test_datalanguage_python_registered(ITesting *t);
DLL_EXPORT int test_datalanguage_python_tokens(ITesting *t);
DLL_EXPORT int test_datalanguage_python_strings(ITesting *t);
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
      numbers: regex
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

// 'numbers: generic' (GenericNumberMatcher): what is a number, and what only starts like one
DLL_EXPORT int test_datalanguage_numbers(ITesting *t) {
    auto matcher = GenericNumberMatcher::Create(U"jJ");
    auto match = [&matcher](const char32_t *text) {
        return matcher->Match(std::u32string_view(text));
    };
    TR_ASSERT(t, match(U"42") == 2);
    TR_ASSERT(t, match(U"42)") == 2);
    TR_ASSERT(t, match(U"1_000.5e-3 ") == 10);
    TR_ASSERT(t, match(U"0x_FF") == 5);
    TR_ASSERT(t, match(U"0b1010") == 6);
    TR_ASSERT(t, match(U"0o17") == 4);
    TR_ASSERT(t, match(U".5") == 2);
    TR_ASSERT(t, match(U"3j") == 2);
    TR_ASSERT(t, match(U"3.28.1") == 4);       // CMake versions: '3.28' then '.1'
    TR_ASSERT(t, match(U"1e") == 0);           // 'e' without digits makes a word of it
    TR_ASSERT(t, match(U"1.") == 1);           // a dot without a digit after it isn't the number's
    // Glued to a word: no number
    TR_ASSERT(t, match(U"64bit") == 0);
    TR_ASSERT(t, match(U"2nd") == 0);
    TR_ASSERT(t, match(U"0x") == 0);
    TR_ASSERT(t, match(U"abc") == 0);
    TR_ASSERT(t, match(U".x") == 0);
    TR_ASSERT(t, match(U"") == 0);
    // Without suffixes, '3j' is a word
    TR_ASSERT(t, GenericNumberMatcher::Create()->Match(std::u32string_view(U"3j")) == 0);
    return kTR_Pass;
}

static kLanguageTokenClass ClassAt(const std::vector<std::pair<int, kLanguageTokenClass>> &tokens, int pos) {
    for (auto &[tokenPos, tokenClass] : tokens) {
        if (tokenPos == pos) {
            return tokenClass;
        }
    }
    return kLanguageTokenClass::kUnknown;
}

// 'case_sensitive: no' folds every whole-word list; an entry's own 'case_sensitive: yes' keeps its list exact
DLL_EXPORT int test_datalanguage_caseinsensitive(ITesting *t) {
    auto language = MakeLanguage(t, R"yml(
language: { name: ci, extensions: [".ci"], case_sensitive: no }
tokenizer:
  states:
    main:
      postfix: "( )"
      tokens:
        - { class: operator, words: "( )" }
        - { class: function, whole_word: yes, words: "add_executable" }
        - { class: known_type, whole_word: yes, case_sensitive: yes, words: "PUBLIC" }
)yml");
    TR_ASSERT(t, language != nullptr);
    auto tokens = Tokenize(*language, "ADD_EXECUTABLE(Add_Executable add_executable PUBLIC public)");
    TR_ASSERT(t, ClassAt(tokens, 0) == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, ClassAt(tokens, 15) == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, ClassAt(tokens, 30) == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, ClassAt(tokens, 45) == kLanguageTokenClass::kKnownType);
    TR_ASSERT(t, ClassAt(tokens, 52) == kLanguageTokenClass::kRegular);
    // Entries of one class must agree
    auto problems = ParseProblems(R"yml(
language: { name: m, extensions: [".m"] }
tokenizer:
  states:
    main:
      tokens:
        - { class: keyword, whole_word: yes, words: "if" }
        - { class: keyword, whole_word: yes, case_sensitive: no, words: "else" }
)yml");
    TR_ASSERT(t, !problems.empty());
    return kTR_Pass;
}

// cmake.yml is registered for '*.cmake' and by name for 'CMakeLists.txt' - not for every '.txt'
DLL_EXPORT int test_datalanguage_cmake_registered(ITesting *t) {
    auto &editor = Editor::Instance();
    auto language = editor.GetLanguageForFile("project/CMakeLists.txt");
    TR_ASSERT(t, language != nullptr);
    TR_ASSERT(t, language->Identifier() == U"cmake");
    TR_ASSERT(t, editor.GetLanguageForFile("cmake/Toolchain.cmake") == language);
    TR_ASSERT(t, editor.GetLanguageForFile("notes.txt") != language);
    TR_ASSERT(t, editor.GetLanguageForFile("src/main.cpp")->Identifier() != U"cmake");
    TR_ASSERT(t, language->GetLineComment() == U"#");
    return kTR_Pass;
}

// The shipped cmake.yml, line by line (docs/support-languages.md §8)
DLL_EXPORT int test_datalanguage_cmake_tokens(ITesting *t) {
    auto language = Editor::Instance().GetLanguageForFile("CMakeLists.txt");
    TR_ASSERT(t, language != nullptr);
    //                                   0         1         2         3         4
    //                                   0123456789012345678901234567890123456789012345
    auto tokens = Tokenize(*language, "ADD_EXECUTABLE(app main.cpp)");
    TR_ASSERT(t, ClassAt(tokens, 0) == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, ClassAt(tokens, 14) == kLanguageTokenClass::kOperator);
    TR_ASSERT(t, ClassAt(tokens, 15) == kLanguageTokenClass::kRegular);

    tokens = Tokenize(*language, "target_link_libraries(app PUBLIC fmt)");
    TR_ASSERT(t, ClassAt(tokens, 0) == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, ClassAt(tokens, 26) == kLanguageTokenClass::kKnownType);

    tokens = Tokenize(*language, "if(NOT DEFINED X) # why");
    TR_ASSERT(t, ClassAt(tokens, 0) == kLanguageTokenClass::kKeyword);
    TR_ASSERT(t, ClassAt(tokens, 3) == kLanguageTokenClass::kKnownType);
    TR_ASSERT(t, ClassAt(tokens, 18) == kLanguageTokenClass::kLineComment);
    TR_ASSERT(t, ClassAt(tokens, 20) == kLanguageTokenClass::kCommentedText);

    // A reference in a string is its own class; the string goes on after it
    tokens = Tokenize(*language, "message(\"dir ${CMAKE_SOURCE_DIR} ok\")");
    TR_ASSERT(t, ClassAt(tokens, 8) == kLanguageTokenClass::kString);
    TR_ASSERT(t, ClassAt(tokens, 13) == kLanguageTokenClass::kMacroIdentifier);
    TR_ASSERT(t, ClassAt(tokens, 15) == kLanguageTokenClass::kMacroIdentifier);
    TR_ASSERT(t, ClassAt(tokens, 33) == kLanguageTokenClass::kString);

    // A '\' before a character that is no escape doesn't stop the line: the string goes on, and closes
    //                            0          1          2
    //                            0123456789 012 345678 9012345
    tokens = Tokenize(*language, "message(\"a\\nb\") # c");
    TR_ASSERT(t, ClassAt(tokens, 11) == kLanguageTokenClass::kString);
    TR_ASSERT(t, ClassAt(tokens, 14) == kLanguageTokenClass::kOperator);
    TR_ASSERT(t, ClassAt(tokens, 16) == kLanguageTokenClass::kLineComment);

    // Nested references, a generator expression, numbers and constants
    tokens = Tokenize(*language, "set(V ${${P}_DIR} $<CONFIG:Debug> 3.28 ON)");
    TR_ASSERT(t, ClassAt(tokens, 6) == kLanguageTokenClass::kMacroIdentifier);
    TR_ASSERT(t, ClassAt(tokens, 8) == kLanguageTokenClass::kMacroIdentifier);
    TR_ASSERT(t, ClassAt(tokens, 18) == kLanguageTokenClass::kPreProcessor);
    TR_ASSERT(t, ClassAt(tokens, 20) == kLanguageTokenClass::kPreProcessor);
    TR_ASSERT(t, ClassAt(tokens, 34) == kLanguageTokenClass::kNumber);
    TR_ASSERT(t, ClassAt(tokens, 39) == kLanguageTokenClass::kEnumValue);

    // #[[ ... ]] spans lines; code after it is code again
    std::vector<std::pair<size_t, kLanguageTokenClass>> lineTokens;
    language->Tokenizer().TokenizeText(U"#[[ a\nstill ]]\nset(X 1)", [&lineTokens](size_t idxLine, const LangToken &token) {
        lineTokens.emplace_back(idxLine, token.classification);
    });
    bool isLine1Comment = false;
    bool isLine2Code = false;
    for (auto &[idxLine, tokenClass] : lineTokens) {
        if ((idxLine == 1) && (tokenClass == kLanguageTokenClass::kCommentedText)) {
            isLine1Comment = true;
        }
        if ((idxLine == 2) && (tokenClass == kLanguageTokenClass::kFunction)) {
            isLine2Code = true;
        }
    }
    TR_ASSERT(t, isLine1Comment);
    TR_ASSERT(t, isLine2Code);
    return kTR_Pass;
}

// The prefix rule: '@' takes the name glued to it, dots between names included; '@' with no name is left to the other
// lists. A prefix that pushes, or has 'words' too, is a problem.
DLL_EXPORT int test_datalanguage_prefix(ITesting *t) {
    auto language = MakeLanguage(t, R"yml(
language: { name: pre, extensions: [".pre"] }
tokenizer:
  states:
    main:
      postfix: "( ) @ ."
      tokens:
        - { class: operator, words: "( ) @ ." }
        - { class: macro_identifier, prefix: "@" }
)yml");
    TR_ASSERT(t, language != nullptr);
    //                                   0         1         2
    //                                   0123456789012345678901234
    auto tokens = Tokenize(*language, "@app.route(x) a @ b @c.");
    TR_ASSERT(t, tokens.size() == 9);
    TR_ASSERT(t, ClassAt(tokens, 0) == kLanguageTokenClass::kMacroIdentifier);
    TR_ASSERT(t, ClassAt(tokens, 10) == kLanguageTokenClass::kOperator);
    TR_ASSERT(t, ClassAt(tokens, 16) == kLanguageTokenClass::kOperator);
    TR_ASSERT(t, ClassAt(tokens, 20) == kLanguageTokenClass::kMacroIdentifier);
    // The trailing '.' is not part of the name
    TR_ASSERT(t, ClassAt(tokens, 22) == kLanguageTokenClass::kOperator);

    TR_ASSERT(t, !ParseProblems(R"yml(
language: { name: m, extensions: [".m"] }
tokenizer:
  states:
    main:
      tokens:
        - { class: macro_identifier, prefix: "@", push: main }
)yml").empty());
    TR_ASSERT(t, !ParseProblems(R"yml(
language: { name: m, extensions: [".m"] }
tokenizer:
  states:
    main:
      tokens:
        - { class: macro_identifier, prefix: "@", words: "x" }
)yml").empty());
    return kTR_Pass;
}

DLL_EXPORT int test_datalanguage_python_registered(ITesting *t) {
    auto &editor = Editor::Instance();
    auto language = editor.GetLanguageForFile("game/main.py");
    TR_ASSERT(t, language != nullptr);
    TR_ASSERT(t, language->Identifier() == U"python");
    TR_ASSERT(t, editor.GetLanguageForFile("stubs/x.pyi") == language);
    TR_ASSERT(t, language->GetLineComment() == U"#");
    // Its indent section: ':' at the end indents, a closer dedents, reformat leaves it alone
    auto table = IndentCache::Instance().GetTableForLanguage("python");
    TR_ASSERT(t, table->IsIndentAfter(U':'));
    TR_ASSERT(t, table->IsDedentOn(U')'));
    TR_ASSERT(t, !table->isReindentable);
    return kTR_Pass;
}

// The shipped python.yml, line by line (docs/support-languages.md §8)
DLL_EXPORT int test_datalanguage_python_tokens(ITesting *t) {
    auto language = Editor::Instance().GetLanguageForFile("main.py");
    TR_ASSERT(t, language != nullptr);
    //                                   0         1         2         3         4
    //                                   0123456789012345678901234567890123456789012345
    auto tokens = Tokenize(*language, "def greet(name: str) -> None:  # hi");
    TR_ASSERT(t, ClassAt(tokens, 0) == kLanguageTokenClass::kKeyword);
    TR_ASSERT(t, ClassAt(tokens, 4) == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, ClassAt(tokens, 9) == kLanguageTokenClass::kOperator);
    TR_ASSERT(t, ClassAt(tokens, 10) == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, ClassAt(tokens, 16) == kLanguageTokenClass::kKnownType);
    TR_ASSERT(t, ClassAt(tokens, 21) == kLanguageTokenClass::kOperator);
    TR_ASSERT(t, ClassAt(tokens, 24) == kLanguageTokenClass::kEnumValue);
    TR_ASSERT(t, ClassAt(tokens, 31) == kLanguageTokenClass::kLineComment);
    TR_ASSERT(t, ClassAt(tokens, 33) == kLanguageTokenClass::kCommentedText);

    tokens = Tokenize(*language, "class Player(Sprite):");
    TR_ASSERT(t, ClassAt(tokens, 0) == kLanguageTokenClass::kKeyword);
    TR_ASSERT(t, ClassAt(tokens, 6) == kLanguageTokenClass::kUserType);
    TR_ASSERT(t, ClassAt(tokens, 12) == kLanguageTokenClass::kOperator);
    TR_ASSERT(t, ClassAt(tokens, 13) == kLanguageTokenClass::kRegular);

    tokens = Tokenize(*language, "@dataclass(frozen=True)");
    TR_ASSERT(t, ClassAt(tokens, 0) == kLanguageTokenClass::kMacroIdentifier);
    TR_ASSERT(t, ClassAt(tokens, 10) == kLanguageTokenClass::kOperator);
    TR_ASSERT(t, ClassAt(tokens, 18) == kLanguageTokenClass::kEnumValue);

    //                            0         1         2         3
    //                            0123456789012345678901234567890123456
    tokens = Tokenize(*language, "x = [0x_FF, 1_000.5e-3, 3j, len(a)]");
    TR_ASSERT(t, ClassAt(tokens, 5) == kLanguageTokenClass::kNumber);
    TR_ASSERT(t, ClassAt(tokens, 12) == kLanguageTokenClass::kNumber);
    TR_ASSERT(t, ClassAt(tokens, 24) == kLanguageTokenClass::kNumber);
    TR_ASSERT(t, ClassAt(tokens, 28) == kLanguageTokenClass::kFunction);

    // 'elif' and 'and' are keywords, a name that starts with one is not; 'a @ b' is the operator
    tokens = Tokenize(*language, "elif iffy and a @ b:");
    TR_ASSERT(t, ClassAt(tokens, 0) == kLanguageTokenClass::kKeyword);
    TR_ASSERT(t, ClassAt(tokens, 5) == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, ClassAt(tokens, 10) == kLanguageTokenClass::kKeyword);
    TR_ASSERT(t, ClassAt(tokens, 16) == kLanguageTokenClass::kOperator);
    return kTR_Pass;
}

namespace {
    std::vector<std::pair<size_t, kLanguageTokenClass>> TokenizeLines(LanguageBase &language, const std::u32string &text) {
        std::vector<std::pair<size_t, kLanguageTokenClass>> lineTokens;
        language.Tokenizer().TokenizeText(text, [&lineTokens](size_t idxLine, const LangToken &token) {
            lineTokens.emplace_back(idxLine, token.classification);
        });
        return lineTokens;
    }

    // Every token on the line has the class (and there is one)
    bool IsAllOnLine(const std::vector<std::pair<size_t, kLanguageTokenClass>> &lineTokens, size_t idxLine, kLanguageTokenClass tokenClass) {
        bool isAny = false;
        for (auto &[idx, each] : lineTokens) {
            if (idx == idxLine) {
                isAny = true;
                if (each != tokenClass) {
                    return false;
                }
            }
        }
        return isAny;
    }
}

// Strings: the prefixes, escapes, a missing closer ends with the line, triple quotes span lines and only their own
// closer ends them
DLL_EXPORT int test_datalanguage_python_strings(ITesting *t) {
    auto language = Editor::Instance().GetLanguageForFile("main.py");
    TR_ASSERT(t, language != nullptr);
    //                                   0           1          2            3
    //                                   012345 67890123456 78 9012 345 678901 234
    auto tokens = Tokenize(*language, "print(f\"hi {name}\", r'\\d', 'it\\'s') + x");
    TR_ASSERT(t, ClassAt(tokens, 0) == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, ClassAt(tokens, 6) == kLanguageTokenClass::kString);
    TR_ASSERT(t, ClassAt(tokens, 11) == kLanguageTokenClass::kString);
    TR_ASSERT(t, ClassAt(tokens, 18) == kLanguageTokenClass::kOperator);
    TR_ASSERT(t, ClassAt(tokens, 20) == kLanguageTokenClass::kString);
    TR_ASSERT(t, ClassAt(tokens, 27) == kLanguageTokenClass::kString);
    TR_ASSERT(t, ClassAt(tokens, 34) == kLanguageTokenClass::kOperator);
    TR_ASSERT(t, ClassAt(tokens, 36) == kLanguageTokenClass::kOperator);

    // A '"' inside """ ... """ doesn't end it; after the closer it is code again
    auto lineTokens = TokenizeLines(*language, U"s = \"\"\"one\nsay \"hi\" if\n\"\"\" if\nif x:");
    TR_ASSERT(t, IsAllOnLine(lineTokens, 1, kLanguageTokenClass::kString));
    TR_ASSERT(t, lineTokens.back().first == 3);
    TR_ASSERT(t, lineTokens[lineTokens.size() - 3].second == kLanguageTokenClass::kKeyword);

    // An unclosed "..." ends with its line
    lineTokens = TokenizeLines(*language, U"x = \"open\nif");
    TR_ASSERT(t, IsAllOnLine(lineTokens, 1, kLanguageTokenClass::kKeyword));

    // ''' with a prefix, across lines
    lineTokens = TokenizeLines(*language, U"q = rb'''a\nif\n'''\nif");
    TR_ASSERT(t, IsAllOnLine(lineTokens, 1, kLanguageTokenClass::kString));
    TR_ASSERT(t, IsAllOnLine(lineTokens, 3, kLanguageTokenClass::kKeyword));
    return kTR_Pass;
}
