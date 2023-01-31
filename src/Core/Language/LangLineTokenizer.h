#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <stack>
#include <memory>
#include "Core/StrUtil.h"
#include "Core/Line.h"
#include "Core/Language/LangToken.h"

namespace gnilk {

    // Consider placing this in a namespace instead of using internal classes...
    class LangLineTokenizer {
    public:
        enum class kAction {
            kPushState,
            kPopState,
        };

        struct Action {
            kAction action; // push/pop
            std::string stateName;
        };

        // FIXME: Ability to set user supplied matching function...
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

        // FIXME: Quite a large internal class - consider putting it somewhere else??
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
        explicit LangLineTokenizer();
        virtual ~LangLineTokenizer() = default;

        void ParseLine(std::vector<LangToken> &tokens, const char *input);
        void ParseLines(std::vector<Line *> &lines);

        // State management - this is available
        void SetStartState(const std::string &newStartState);
        State::Ref GetOrAddState(const char *stateName);
        bool HasState(const char *stateName);
        State::Ref GetState(const char *stateName);

    protected:
        kLanguageTokenClass CheckExecuteActionForToken(State::Ref currentState, const char *token, kLanguageTokenClass tokenClass);
        std::pair<bool, kLanguageTokenClass> GetNextToken(char *dst, int nMax, char **input);

        bool ResetStateStack();

        // State stack manipulation - internal!
        bool PushState(const char *stateName);
        void PushState(State::Ref state);
        State::Ref PopState();

    protected:
        std::string startState = "main";
    };
}