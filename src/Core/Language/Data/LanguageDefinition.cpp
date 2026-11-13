//
// A language file into plain data - see the header.
//

#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>

#include "LanguageDefinition.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

// The class names a file may use. The tokenizer produces these; the semantic classes (user_type, function, ...)
// come from the code index and are not here.
static const std::unordered_map<std::string, kLanguageTokenClass> tokenClassNames = {
        {"regular", kLanguageTokenClass::kRegular},
        {"operator", kLanguageTokenClass::kOperator},
        {"keyword", kLanguageTokenClass::kKeyword},
        {"separator", kLanguageTokenClass::kSeparator},
        {"known_type", kLanguageTokenClass::kKnownType},
        {"number", kLanguageTokenClass::kNumber},
        {"string", kLanguageTokenClass::kString},
        {"line_comment", kLanguageTokenClass::kLineComment},
        {"block_comment", kLanguageTokenClass::kBlockComment},
        {"commented_text", kLanguageTokenClass::kCommentedText},
        {"code_block_start", kLanguageTokenClass::kCodeBlockStart},
        {"code_block_end", kLanguageTokenClass::kCodeBlockEnd},
        {"array_start", kLanguageTokenClass::kArrayStart},
        {"array_end", kLanguageTokenClass::kArrayEnd},
        {"char", kLanguageTokenClass::kChar},
        {"import", kLanguageTokenClass::kImport},
        {"preprocessor", kLanguageTokenClass::kPreProcessor},
        {"macro_identifier", kLanguageTokenClass::kMacroIdentifier},
        {"heading", kLanguageTokenClass::kHeading},
        {"strong", kLanguageTokenClass::kStrong},
        {"emphasis", kLanguageTokenClass::kEmphasis},
        {"code", kLanguageTokenClass::kCode},
        {"list_marker", kLanguageTokenClass::kListMarker},
        {"block_quote", kLanguageTokenClass::kBlockQuote},
        {"link", kLanguageTokenClass::kLink},
        {"rule", kLanguageTokenClass::kRule},
        {"config_key", kLanguageTokenClass::kConfigKey},
};

static const std::set<std::string> tokenRuleKeys = {"class", "words", "whole_word", "push", "pop", "push_at_eol"};
static const std::set<std::string> stateKeys = {"regular", "postfix", "tokens", "eol", "indent_scoped"};

namespace {
    // Collects problems and keeps the line lookups in one place
    class Reader {
    public:
        explicit Reader(std::vector<LanguageDefinition::Problem> &newProblems) : problems(newProblems) {
        }

        void AddProblem(const YAML::Node &at, const std::string &reason) {
            problems.push_back({LineOf(at), reason});
            hasProblem = true;
        }

        static int LineOf(const YAML::Node &node) {
            if (!node.IsDefined()) {
                return 0;
            }
            auto mark = node.Mark();
            return (mark.line < 0) ? 0 : mark.line + 1;
        }

        std::string Str(const YAML::Node &node, const std::string &defValue = "") {
            if (!node.IsDefined() || node.IsNull()) {
                return defValue;
            }
            if (!node.IsScalar()) {
                AddProblem(node, "expected a single value");
                return defValue;
            }
            return node.Scalar();
        }

        bool Bool(const YAML::Node &node, bool defValue) {
            if (!node.IsDefined() || node.IsNull()) {
                return defValue;
            }
            bool value = defValue;
            if (!node.IsScalar() || !YAML::convert<bool>::decode(node, value)) {
                AddProblem(node, "expected yes or no");
                return defValue;
            }
            return value;
        }

        std::vector<std::string> StrList(const YAML::Node &node) {
            std::vector<std::string> out;
            if (!node.IsDefined() || node.IsNull()) {
                return out;
            }
            if (!node.IsSequence()) {
                AddProblem(node, "expected a list");
                return out;
            }
            for (auto element : node) {
                out.push_back(Str(element));
            }
            return out;
        }

        // 'words: "a b c"' or 'words: [a, b, c]', longest first. A word may not hold a blank - the tokenizer
        // takes its lists space-separated.
        std::vector<std::u32string> Words(const YAML::Node &node) {
            std::vector<std::string> raw;
            if (node.IsScalar()) {
                auto text = node.Scalar();
                size_t pos = 0;
                while (pos < text.size()) {
                    auto start = text.find_first_not_of(" \t\n", pos);
                    if (start == std::string::npos) {
                        break;
                    }
                    auto end = text.find_first_of(" \t\n", start);
                    if (end == std::string::npos) {
                        end = text.size();
                    }
                    raw.push_back(text.substr(start, end - start));
                    pos = end;
                }
            } else if (node.IsSequence()) {
                for (auto element : node) {
                    auto word = Str(element);
                    if (word.find_first_of(" \t\n") != std::string::npos) {
                        AddProblem(element, "a word can't hold a blank: '" + word + "'");
                        continue;
                    }
                    raw.push_back(word);
                }
            } else {
                AddProblem(node, "expected words, as \"a b c\" or [a, b, c]");
            }

            std::vector<std::u32string> words;
            for (auto &word : raw) {
                if (!word.empty()) {
                    words.push_back(UnicodeHelper::utf8to32(word));
                }
            }
            std::stable_sort(words.begin(), words.end(), [](const std::u32string &a, const std::u32string &b) {
                return a.size() > b.size();
            });
            return words;
        }

        std::optional<kLanguageTokenClass> TokenClass(const YAML::Node &node) {
            auto name = Str(node);
            auto tokenClass = LanguageDefinition::TokenClassFromName(name);
            if (!tokenClass.has_value()) {
                AddProblem(node, "'" + name + "' is not a token class");
            }
            return tokenClass;
        }

    public:
        bool hasProblem = false;
    private:
        std::vector<LanguageDefinition::Problem> &problems;
    };
}

static void ReadTokenRule(Reader &reader, const YAML::Node &node, LanguageDefinition::State &state) {
    if (!node.IsMap()) {
        reader.AddProblem(node, "a token is a map: { class: ..., words: ... }");
        return;
    }
    for (auto it : node) {
        if (tokenRuleKeys.find(it.first.Scalar()) == tokenRuleKeys.end()) {
            reader.AddProblem(it.first, "'" + it.first.Scalar() + "' is not a token key");
        }
    }
    if (!node["class"].IsDefined() || !node["words"].IsDefined()) {
        reader.AddProblem(node, "a token needs 'class' and 'words'");
        return;
    }

    LanguageDefinition::TokenRule rule;
    auto tokenClass = reader.TokenClass(node["class"]);
    if (tokenClass.has_value()) {
        rule.tokenClass = *tokenClass;
    }
    rule.isWholeWord = reader.Bool(node["whole_word"], false);
    rule.words = reader.Words(node["words"]);

    int nActions = 0;
    if (node["push"].IsDefined()) {
        rule.action = LanguageDefinition::kStateAction::kPush;
        rule.stateName = reader.Str(node["push"]);
        nActions++;
    }
    if (node["push_at_eol"].IsDefined()) {
        rule.action = LanguageDefinition::kStateAction::kPushAtEOL;
        rule.stateName = reader.Str(node["push_at_eol"]);
        nActions++;
    }
    if (reader.Bool(node["pop"], false)) {
        rule.action = LanguageDefinition::kStateAction::kPop;
        nActions++;
    }
    if (nActions > 1) {
        reader.AddProblem(node, "a token can push or pop, not both");
    }
    state.tokens.push_back(std::move(rule));
}

static void ReadState(Reader &reader, const std::string &name, const YAML::Node &node, LanguageDefinition &definition) {
    LanguageDefinition::State state;
    state.name = name;
    if (node.IsNull()) {
        definition.states.push_back(std::move(state));
        return;
    }
    if (!node.IsMap()) {
        reader.AddProblem(node, "state '" + name + "' is not a map");
        return;
    }
    for (auto it : node) {
        auto key = it.first.Scalar();
        if (key == "numbers") {
            reader.AddProblem(it.first, "'numbers' isn't supported yet (phase 2)");
        } else if (stateKeys.find(key) == stateKeys.end()) {
            reader.AddProblem(it.first, "'" + key + "' is not a state key");
        }
    }

    if (node["regular"].IsDefined()) {
        state.regularClass = reader.TokenClass(node["regular"]);
    }
    if (node["postfix"].IsDefined()) {
        state.postfix = reader.Words(node["postfix"]);
    }
    state.isIndentScoped = reader.Bool(node["indent_scoped"], false);

    auto eol = node["eol"];
    if (eol.IsDefined()) {
        if (eol.IsScalar() && (eol.Scalar() == "pop")) {
            state.eolAction = LanguageDefinition::kStateAction::kPop;
        } else if (eol.IsMap() && eol["push"].IsDefined()) {
            state.eolAction = LanguageDefinition::kStateAction::kPush;
            state.eolStateName = reader.Str(eol["push"]);
        } else {
            reader.AddProblem(eol, "'eol' is 'pop' or { push: <state> }");
        }
    }

    auto tokens = node["tokens"];
    if (tokens.IsDefined() && !tokens.IsNull()) {
        if (!tokens.IsSequence()) {
            reader.AddProblem(tokens, "'tokens' is a list");
        } else {
            for (auto rule : tokens) {
                ReadTokenRule(reader, rule, state);
            }
        }
    }

    // The tokenizer keeps one list per class in a state: the entries of a class merge, so they must agree
    std::map<kLanguageTokenClass, bool> wholeWordByClass;
    for (auto &rule : state.tokens) {
        auto it = wholeWordByClass.find(rule.tokenClass);
        if ((it != wholeWordByClass.end()) && (it->second != rule.isWholeWord)) {
            reader.AddProblem(node, "state '" + name + "': the entries of one class must all be whole_word, or none");
            break;
        }
        wholeWordByClass[rule.tokenClass] = rule.isWholeWord;
    }

    definition.states.push_back(std::move(state));
}

static void CheckStateNames(Reader &reader, const YAML::Node &tokenizer, LanguageDefinition &definition) {
    std::set<std::string> names;
    for (auto &state : definition.states) {
        names.insert(state.name);
    }
    auto check = [&](const std::string &stateName, const std::string &from) {
        if (names.find(stateName) == names.end()) {
            reader.AddProblem(tokenizer["states"][from], "state '" + from + "' goes to '" + stateName + "', which isn't a state");
        }
    };
    if (names.find(definition.startState) == names.end()) {
        reader.AddProblem(tokenizer["start"], "the start state '" + definition.startState + "' isn't a state");
    }
    for (auto &state : definition.states) {
        for (auto &rule : state.tokens) {
            if ((rule.action == LanguageDefinition::kStateAction::kPush) || (rule.action == LanguageDefinition::kStateAction::kPushAtEOL)) {
                check(rule.stateName, state.name);
            }
        }
        if (state.eolAction == LanguageDefinition::kStateAction::kPush) {
            check(state.eolStateName, state.name);
        }
    }
}

std::optional<LanguageDefinition> LanguageDefinition::Parse(const YAML::Node &root, std::vector<Problem> &outProblems) {
    Reader reader(outProblems);
    LanguageDefinition definition;

    if (!root.IsMap() || !root["language"].IsMap()) {
        reader.AddProblem(root, "no 'language:' section");
        return {};
    }
    auto language = root["language"];
    definition.settings.reset(language);
    definition.name = reader.Str(language["name"]);
    if (definition.name.empty()) {
        reader.AddProblem(language, "'language:' has no 'name'");
    }
    definition.display = reader.Str(language["display"], definition.name);
    definition.extensions = reader.StrList(language["extensions"]);
    definition.fileNames = reader.StrList(language["filenames"]);
    definition.author = reader.Str(language["author"]);
    definition.date = reader.Str(language["date"]);
    definition.version = reader.Str(language["version"]);
    definition.lineComment = UnicodeHelper::utf8to32(reader.Str(language["line_comment"]));
    definition.isOverride = reader.Bool(language["override"], false);

    auto engine = reader.Str(language["engine"], "data");
    if (engine == "builtin") {
        definition.engine = kEngine::kBuiltin;
    } else if (engine != "data") {
        reader.AddProblem(language["engine"], "'engine' is 'data' or 'builtin'");
    }

    // Assigning a missing key of a const node throws in yaml-cpp - only take what is there
    if (root["indent"].IsDefined()) {
        definition.indent.reset(root["indent"]);
    }
    if (root["autopairs"].IsDefined()) {
        definition.autoPairs.reset(root["autopairs"]);
    }

    auto tokenizer = root["tokenizer"];
    auto isLanguage = !definition.extensions.empty() || !definition.fileNames.empty();
    if (tokenizer.IsDefined() && !tokenizer.IsNull()) {
        if (!tokenizer.IsMap() || !tokenizer["states"].IsMap()) {
            reader.AddProblem(tokenizer, "'tokenizer:' needs 'states:'");
        } else {
            definition.startState = reader.Str(tokenizer["start"], "main");
            for (auto it : tokenizer["states"]) {
                ReadState(reader, it.first.Scalar(), it.second, definition);
            }
            CheckStateNames(reader, tokenizer, definition);
        }
    } else if (isLanguage && (definition.engine == kEngine::kData)) {
        reader.AddProblem(language, "a data language needs a 'tokenizer:' section");
    }

    if (reader.hasProblem) {
        return {};
    }
    return definition;
}

std::optional<kLanguageTokenClass> LanguageDefinition::TokenClassFromName(const std::string &name) {
    auto it = tokenClassNames.find(name);
    if (it == tokenClassNames.end()) {
        return {};
    }
    return it->second;
}
