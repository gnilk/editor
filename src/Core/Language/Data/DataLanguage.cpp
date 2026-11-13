//
// A language from a file - see the header.
//

#include <algorithm>
#include <map>

#include "DataLanguage.h"
#include "Core/Config/Config.h"
#include "Core/Language/Data/LanguageFileSet.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

static LangLineTokenizer::kAction ToTokenizerAction(LanguageDefinition::kStateAction action) {
    switch (action) {
        case LanguageDefinition::kStateAction::kPush:
            return LangLineTokenizer::kAction::kPushState;
        case LanguageDefinition::kStateAction::kPop:
            return LangLineTokenizer::kAction::kPopState;
        case LanguageDefinition::kStateAction::kPushAtEOL:
            return LangLineTokenizer::kAction::kPushStateAtEOL;
        case LanguageDefinition::kStateAction::kNone:
            break;
    }
    return LangLineTokenizer::kAction::kNone;
}

// The tokenizer takes its lists space-separated, and matches them in order - so longest first
static std::u32string JoinLongestFirst(std::vector<std::u32string> words) {
    std::stable_sort(words.begin(), words.end(), [](const std::u32string &a, const std::u32string &b) {
        return a.size() > b.size();
    });
    std::u32string joined;
    for (auto &word : words) {
        if (!joined.empty()) {
            joined += U' ';
        }
        joined += word;
    }
    return joined;
}

DataLanguage::DataLanguage(LanguageDefinition newDefinition) : definition(std::move(newDefinition)) {
    identifier = UnicodeHelper::utf8to32(definition.name);
}

bool DataLanguage::Initialize() {
    for (auto &stateDefinition : definition.states) {
        BuildState(stateDefinition);
    }
    if (!definition.startState.empty()) {
        tokenizer.SetStartState(definition.startState);
    }
    ApplySettings();
    return true;
}

void DataLanguage::BuildState(const LanguageDefinition::State &stateDefinition) {
    auto state = tokenizer.GetOrAddState(stateDefinition.name.c_str());
    if (stateDefinition.regularClass.has_value()) {
        state->SetRegularTokenClass(*stateDefinition.regularClass);
    }
    if (!stateDefinition.postfix.empty()) {
        state->SetPostFixIdentifiers(JoinLongestFirst(stateDefinition.postfix));
    }
    state->SetIndentScoped(stateDefinition.isIndentScoped);

    // One list per class in a state: the entries of a class are merged (LanguageDefinition checked they agree on
    // whole_word)
    std::map<kLanguageTokenClass, std::vector<std::u32string>> wordsByClass;
    std::map<kLanguageTokenClass, bool> wholeWordByClass;
    for (auto &rule : stateDefinition.tokens) {
        auto &words = wordsByClass[rule.tokenClass];
        words.insert(words.end(), rule.words.begin(), rule.words.end());
        wholeWordByClass[rule.tokenClass] = rule.isWholeWord;
    }
    for (auto &[tokenClass, words] : wordsByClass) {
        state->SetIdentifiers(tokenClass, wholeWordByClass[tokenClass], JoinLongestFirst(words));
    }

    for (auto &rule : stateDefinition.tokens) {
        if (rule.action == LanguageDefinition::kStateAction::kNone) {
            continue;
        }
        auto action = ToTokenizerAction(rule.action);
        auto nextState = rule.stateName.empty() ? nullptr : rule.stateName.c_str();
        for (auto &word : rule.words) {
            state->GetOrAddAction(word, action, nextState);
        }
    }

    if (stateDefinition.eolAction != LanguageDefinition::kStateAction::kNone) {
        auto eolState = stateDefinition.eolStateName.empty() ? nullptr : stateDefinition.eolStateName.c_str();
        state->SetEOLAction(ToTokenizerAction(stateDefinition.eolAction), eolState);
    }
}

// The file's settings with config.yml 'languages.<name>' over them - a fresh tree, so neither the file's nodes
// nor the config's are written into. reset() rebinds; operator= on a YAML::Node writes through to the node it shares.
void DataLanguage::ApplySettings() {
    configNodeName = definition.name;
    YAML::Node userSettings;
    if (Config::Instance().HasKey("languages")) {
        auto languages = Config::Instance().GetNode("languages");
        if (languages.HasKey(definition.name)) {
            userSettings.reset(languages.GetNode(definition.name).GetDataNode());
        }
    }
    dataNode.reset(LanguageFileSet::Merge(definition.settings, userSettings));
}
