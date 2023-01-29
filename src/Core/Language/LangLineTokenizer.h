#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <stack>
#include <memory>
#include "Core/StrUtil.h"
#include "Core/Line.h"

namespace gnilk {
    //
    // Language tokenizer and classifier
    //
    // Left:
    // - Line comments
    // - Block comments
    //
    // Perhaps we need to have something to make this 'state' aware - i.e. to track state between call's..
    // like if line parsing ended in a block-comment, the next line should be IN a block-comment (i.e. first token of that line should be a comment line)
    // Alternatively we add something around this which is buffer aware and this is used only per-line...
    //
    class LangBufferTokenizer {
    public:

    public:
        std::vector<Line> &lines;
    };

    //
    // WARNING: WIP Refactoring in progress...
    //



    // Consider placing this in a namespace instead of using internal classes...
    class LangLineTokenizer {
    public:
        // Extend this as we go along...
        typedef enum  : uint8_t {
            kUnknown = 0,
            kRegular = 1,
            kOperator = 2,
            kKeyword = 3,
            kKnownType = 4,
            // FIXME: Implement this...
            kNumber = 5,
            kString = 6,
            kLineComment = 7,
            kBlockComment = 8,
            kFunky = 196,       // USED for debugging..
        } kTokenClass;

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
            static IdentifierList::Ref Factory(kTokenClass tokenClass, const char *strTokens) {
                auto instance = std::make_shared<IdentifierList>();

                instance->classification = tokenClass;
                strutil::splitToStringList(instance->tokens, strTokens);

                return instance;
            }

            kTokenClass classification;
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

        struct State {
            // Reference
            using Ref = std::shared_ptr<State>;

            static State::Ref Factory() {
                return std::make_shared<State>();
            }

            // Token class if we don't find any kind of mapping...
            // i.e. normal text
            kTokenClass regularTokenClass = kTokenClass::kRegular;

            std::string name;
            std::unordered_map<kTokenClass, IdentifierList::Ref> identifiers;

            // This list is a list of all allowed postfix tokens
            // Used to abort regular value, like variable names and such (i.e. not language components)
            IdentifierList::Ref postfixIdentifiers = nullptr;

            // Actions that should happen on specific tokens in this state
            std::unordered_map<std::string, Action> actions;

            void SetRegularTokenClass(kTokenClass newRegularClass) {
                regularTokenClass = newRegularClass;
            }


            //
            // Actions are stack related...  currently only push/pop implemented...
            // FIXME: Move these to Tokenizer - should not be in the state...
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
                postfixIdentifiers = IdentifierList::Factory(kRegular, strTokens);
            }

            //
            // Identifiers should be declared in length order
            // like:
            // "++= <<= >>= ++ << >> + < >"
            // Other wise you will have false positives
            //
            // Each identifier list belongs to a classification
            //
            void SetIdentifiers(kTokenClass classification, const char *strTokens) {
                auto identifierList = IdentifierList::Factory(classification, strTokens);
                identifiers[classification] = identifierList;
            }

            std::pair<bool, kTokenClass> ClassifyToken(const char *token) {
                for (auto &kvp: identifiers) {
                    int dummy;
                    if (kvp.second->IsMatch(token, dummy)) {
                        return {true, kvp.second->classification};
                    }
                }
                return {false, kTokenClass::kUnknown};
            }
        };  // State


        std::unordered_map<std::string, State::Ref> states;
        std::stack<State::Ref> stateStack;

    public:
        struct Token {
            std::string string;     // The token
            int idxOrigStr;         // The position/index in original string
            kTokenClass classification;     // Classification (keyword, user, operator, reserved, comment, etc...)

            const std::string &String() const { return string; }
        };

    public:
        explicit LangLineTokenizer();
        virtual ~LangLineTokenizer() = default;

        void PrepareTokens(std::vector<Token> &tokens, const char *input);

        // State handling
        State::Ref GetOrAddState(const char *stateName);
        bool HasState(const char *stateName);
        State::Ref GetState(const char *stateName);

        bool PushState(const char *stateName);
        void PushState(State::Ref state);
        State::Ref PopState();

    protected:
        kTokenClass CheckExecuteActionForToken(State::Ref currentState, const char *token, kTokenClass tokenClass);
        std::pair<bool, kTokenClass> GetNextToken(char *dst, int nMax, char **input);
    };
}