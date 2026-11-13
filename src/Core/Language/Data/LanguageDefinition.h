//
// A language file read into plain data (docs/support-languages.md §2, §3) - the part of a data language that
// is testable without a tokenizer. Parse() checks the file as a whole: every class name known, every push names
// a state, ...; a file with a problem is rejected entirely (never half-loaded), each problem with its line.
//
// Lists of words are sorted longest-first, so the order in the file doesn't matter ('"' before '"""' would
// otherwise never let '"""' match).
//

#ifndef GEDIT_LANGUAGEDEFINITION_H
#define GEDIT_LANGUAGEDEFINITION_H

#include <optional>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "Core/Language/LanguageTokenClass.h"

namespace gedit {
    class LanguageDefinition {
    public:
        enum class kEngine {
            kData,      // the tokenizer comes from the file
            kBuiltin,   // a C++ language gives the tokenizer (phase 4)
        };

        enum class kStateAction {
            kNone,
            kPush,
            kPop,
            kPushAtEOL,     // push at end of line, only if nothing but a comment follows (YAML's 'key: |')
        };

        // One entry in a state's 'tokens:' list
        struct TokenRule {
            kLanguageTokenClass tokenClass = kLanguageTokenClass::kRegular;
            bool isWholeWord = false;
            std::vector<std::u32string> words;
            kStateAction action = kStateAction::kNone;
            std::string stateName;      // for kPush / kPushAtEOL
        };

        struct State {
            std::string name;
            std::optional<kLanguageTokenClass> regularClass;
            std::vector<std::u32string> postfix;
            std::vector<TokenRule> tokens;
            kStateAction eolAction = kStateAction::kNone;
            std::string eolStateName;
            bool isIndentScoped = false;
        };

        struct Problem {
            int line = 0;   // 1-based, 0 = unknown
            std::string reason;
        };
    public:
        // 'root' is a resolved language file (LanguageFileSet::Resolve). nullopt if it has any problem - they're
        // appended to 'outProblems'.
        static std::optional<LanguageDefinition> Parse(const YAML::Node &root, std::vector<Problem> &outProblems);

        // The class name a file uses ('keyword', 'known_type', ...) - nullopt if there's no such class.
        static std::optional<kLanguageTokenClass> TokenClassFromName(const std::string &name);
    public:
        // Metadata
        std::string name;
        std::string display;
        std::vector<std::string> extensions;
        std::vector<std::string> fileNames;
        std::string author;
        std::string date;
        std::string version;
        std::u32string lineComment;
        kEngine engine = kEngine::kData;
        bool isOverride = false;

        // Tokenizer
        std::string startState;
        std::vector<State> states;

        // The rest, as they are in the file - read by their owners
        YAML::Node settings;    // the 'language:' section (tabsize, insert_spaces, ...)
        YAML::Node indent;      // 'indent:' (IndentCache's section format)
        YAML::Node autoPairs;   // 'autopairs:' (AutoPairCache's section format)
    };
}

#endif //GEDIT_LANGUAGEDEFINITION_H
