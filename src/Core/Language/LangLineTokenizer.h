#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <stack>
#include <memory>
#include "Core/StrUtil.h"
#include "Core/Line.h"
#include "Core/Language/LangToken.h"
#include <string.h>
//
// General purpose stateful stack based language parser/tokenizer.
// The aim was to be fast on regular files (less than 1000 loc per file/unit).
// Can probably be driven straight from JSON/YAML or something..
//
namespace gedit {

#ifndef GEDIT_MAX_LANG_TOKEN_LENGTH
// define the maximum length of a single token
#define GEDIT_MAX_LANG_TOKEN_LENGTH 256
#endif

    // Consider placing this in a namespace instead of using internal classes...
    class LangLineTokenizer {
    public:
        static const int RootStateDepth = 1;

    public:
        enum class kAction {
            kNone,          // No action - this is used by EOL states..
            kPushState,
            kPopState,
        };

        struct Action {
            kAction action; // push/pop
            std::string stateName;
        };

        // Consider: Ability to set user supplied matching function...
        struct IdentifierList {
            using Ref = std::shared_ptr<IdentifierList>;
            static IdentifierList::Ref Factory(kLanguageTokenClass tokenClass, const char *strTokens) {
                auto instance = std::make_shared<IdentifierList>();

                instance->classification = tokenClass;
                strutil::splitToStringList(instance->tokens, strTokens);

                return instance;
            }

            kLanguageTokenClass classification;
            std::vector<std::string> tokens;

            __inline bool IsMatch(const char *input, int &outSzToken) {
                for (auto s: tokens) {
                    if (!strncmp(s.c_str(), input, s.size())) {
                        outSzToken = s.size();
                        return true;
                    }
                }
                return false;
            }
        };

        // TO-DO: Quite a large internal class - consider putting it somewhere else??
        // Some notes about when and where to use certain functions.
        //
        // use the Tokenizer class to create a state by calling 'GetOrAddState'
        //
        // Define state behaviour through these functions.
        //   'SetIdentifiers' - Set a list of space-delimited token that should be classified accordingly
        //   'SetAction' - Define an action (Push/Pop of state) when a certain token is matched
        //   'SetEOLAction' - Set special action for when EOL is found
        //   'SetRegularTokenClass' - Define what classification a regular token should have, used when inside a comment or string - we don't want to recognize keywords - instead classify with this
        //   'SetPostFixIdentifiers' - Define a set of tokens that can occur at the end, like myVariable++ or just 'myVariable;' Generally they are the same as operators - but I've opted to keep them separate..
        //
        // A state can have multiple lists of 'Identifiers' and 'Actions' but only one of 'EOLAction', 'RegularTokenClass' and 'PostFixIdentifiers'.
        //
        struct State {
            // Reference
            using Ref = std::shared_ptr<State>;

            static State::Ref Factory() {
                return std::make_shared<State>();
            }

            // LangToken class if we don't find any kind of mapping...
            // i.e. normal text
            kLanguageTokenClass regularTokenClass = kLanguageTokenClass::kRegular;

            std::string name;
            std::unordered_map<kLanguageTokenClass, IdentifierList::Ref> identifiers;

            // This list is a list of all allowed postfix tokens
            // Used to abort regular value, like variable names and such (i.e. not language components)
            IdentifierList::Ref postfixIdentifiers = nullptr;

            // Actions that should happen on specific tokens in this state
            std::unordered_map<std::string, Action> actions;
            Action eolAction = {.action = kAction::kNone, .stateName = ""};

            void SetRegularTokenClass(kLanguageTokenClass newRegularClass) {
                regularTokenClass = newRegularClass;
            }

            //
            // Actions are stack related...  currently only push/pop implemented...
            //
            const Action &GetOrAddAction(const char *token, kAction action, const char *nextState = nullptr) {
                if (actions.find(token) == actions.end()) {
                    if ((action == kAction::kPushState) && (nextState == nullptr)) {
                        fprintf(stderr, "ERR: PushState can't push nullptr as state name\n");
                        exit(1);
                    }
                    actions[token] = {.action = action, .stateName = (nextState == nullptr) ? "" : nextState};
                }
                return actions[token];
            }

            const Action *GetAction(const char *token) {
                if (actions.find(token) == actions.end()) {
                    return nullptr;
                }
                return &actions[token];
            }

            bool HasActionForToken(const char *token) {
                if (actions.find(token) == actions.end()) {
                    return false;
                }
                return true;
            }

            // Set action for what happens at end-of-line in this state
            // like, when you have line-comments the state should be popped at the end of line...
            void SetEOLAction(kAction action, const char *nextState = nullptr) {
                eolAction.action = action;
                if (nextState != nullptr) {
                    eolAction.stateName = nextState;
                }
            }

            //
            // Post fix identifiers are identifiers that should break regular text parsing.
            // Normally this is your regular 'operators' but in case of comments you might want to
            // set something else...
            // Like for CPP you want '*/' as postfix-operator in the block_comment state...
            //
            void SetPostFixIdentifiers(const char *strTokens) {
                postfixIdentifiers = IdentifierList::Factory(kLanguageTokenClass::kRegular, strTokens);
            }

            //
            // Identifiers should be declared in length order
            // like:
            // "++= <<= >>= ++ << >> + < >"
            // Other wise you will have false positives
            //
            // Each identifier list belongs to a classification
            //
            void SetIdentifiers(kLanguageTokenClass classification, const char *strTokens) {
                auto identifierList = IdentifierList::Factory(classification, strTokens);
                identifiers[classification] = identifierList;
            }

            std::pair<bool, kLanguageTokenClass> ClassifyToken(const char *token) {
                for (auto &kvp: identifiers) {
                    int dummy;
                    if (kvp.second->IsMatch(token, dummy)) {
                        return {true, kvp.second->classification};
                    }
                }
                return {false, kLanguageTokenClass::kUnknown};
            }
        };  // State


        std::unordered_map<std::string, State::Ref> states;
        std::stack<State::Ref> stateStack;

    public:

    public:
        LangLineTokenizer() = default;
        virtual ~LangLineTokenizer() = default;

        void ParseLines(std::vector<Line::Ref> &lines);
        void ParseRegion(std::vector<Line::Ref> &lines, size_t idxLineStart, size_t idxLineEnd);
        void ParseLine(const Line::Ref l, int &indentCounter);
        void ParseLineFromStartState(std::string &listStartState, Line::Ref line);

        // State management - this is available
        void SetStartState(const std::string &newStartState);
        State::Ref GetOrAddState(const char *stateName);
        bool HasState(const char *stateName);
        State::Ref GetState(const char *stateName);

    protected:
        void ParseLineWithCurrentState(std::vector<LangToken> &tokens, const char *input);

        kLanguageTokenClass CheckExecuteActionForToken(State::Ref currentState, const char *token, kLanguageTokenClass tokenClass);
        std::pair<bool, kLanguageTokenClass> GetNextToken(char *dst, int nMax, char **input);

        bool ResetStateStack();

        size_t StartParseRegion(std::vector<Line::Ref> &lines, size_t idxRegion);
        size_t EndParseRegion(std::vector<Line::Ref> &lines, size_t idxRegion);

        // State stack manipulation - internal!
        bool PushState(const char *stateName);
        void PushState(State::Ref state);
        State::Ref PopState();
        State::Ref CurrentState();

    protected:
        std::string startState = "main";
    };
}