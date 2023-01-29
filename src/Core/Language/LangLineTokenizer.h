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
    // TODO: Refactor this, it is a bloody mess right now!
    //


    // This is the language tokenize
    class LangLineTokenizer {
    public:
        typedef enum : uint8_t {
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

        //
        // FIXME: Create factory which takes the token list...
        //
        struct IdentiferList {
            kTokenClass classification;
            std::vector<std::string> tokens;

            bool IsMatch(const char *input, int &outSzToken) {
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
            std::unordered_map<kTokenClass, IdentiferList> identifiers;

            // This list is a list of all allowed postfix tokens
            // Used to abort regular value, like variable names and such (i.e. not language components)
            IdentiferList postfixIdentifiers = {};

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
                postfixIdentifiers.classification = kRegular;
                SplitToStringList(postfixIdentifiers.tokens, strTokens);

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
                IdentiferList identiferList = {};
                identiferList.classification = classification;
                SplitToStringList(identiferList.tokens, strTokens);
                identifiers[classification] = identiferList;
            }

            std::pair<bool, kTokenClass> ClassifyToken(const char *token) {

                for (auto &kvp: identifiers) {
                    int dummy;
                    if (kvp.second.IsMatch(token, dummy)) {
                        return {true, kvp.second.classification};
                    }
                }
                return {false, kTokenClass::kUnknown};
            }

            // FIXME: Remove from there...
            void SplitToStringList(std::vector<std::string> &outList, const char *input) {
                char tmp[256];
                char *parsePoint = (char *) input;

                if (input == nullptr) {
                    return;
                }

                while (GetNextTokenNoOperator(tmp, 256, &parsePoint)) {
                    outList.push_back(std::string(tmp));
                }
            }

            // FIXME: Remove from there...
            char *GetNextTokenNoOperator(char *dst, int nMax, char **input) {
                if (!strutil::skipWhiteSpace(input)) {
                    return nullptr;
                }

                int i = 0;
                while (!isspace(**input) && (**input != '\0')) {
                    dst[i++] = **input;

                    // This is a developer problem, ergo - safe to exit..
                    if (i >= nMax) {
                        fprintf(stderr, "ERR: GetNextToken, token size larger than buffer (>nMax)\n");
                        exit(1);
                    }

                    (*input)++;
                }
                dst[i] = '\0';
                return dst;
            }
        };  // State


        std::unordered_map<std::string, State::Ref> states;
        std::stack<State::Ref> stateStack;

        State::Ref GetOrAddState(const char *stateName) {
            if (states.find(stateName) == states.end()) {
                auto state = State::Factory();
                state->name = stateName;
                states[stateName] = state;
            }
            return states[stateName];
        }
        bool HasState(const char *stateName) {
            if (states.find(stateName) == states.end()) {
                return false;
            }
            return true;
        }
        State::Ref GetState(const char *stateName) {
            return states[stateName];
        }


    public:
        struct Token {
            std::string string;     // The token
            int idxOrigStr;         // The position/index in original string
            kTokenClass classification;     // Classification (keyword, user, operator, reserved, comment, etc...)

            const std::string &String() const { return string; }
        };

        bool PushState(const char *stateName) {
            if (!HasState(stateName)) {
                return false;
            }
            PushState(states[stateName]);
            return true;
        }
        void PushState(State::Ref state) {
            stateStack.push(state);
        }
        State::Ref PopState() {
            auto top = stateStack.top();
            stateStack.pop();
            return top;
        }
    public:
        explicit LangLineTokenizer();
        virtual ~LangLineTokenizer() = default;

        void PrepareTokens(std::vector<Token> &tokens, const char *input);
    protected:
        bool InStringList(std::vector<std::string> &strList, const char *input, int &outSz);
        bool SkipWhiteSpace(char **input);
        std::pair<bool, kTokenClass> GetNextToken(char *dst, int nMax, char **input);
        char *GetNextTokenNoOperator(char *dst, int nMax, char **input);

        void SplitToStringList(const char *input, std::vector<std::string> &outList);
    protected:

        // Note: These should be 'global' and not per tokenizer!!!
        std::vector<std::string> operators;
        std::vector<std::string> keywords;
        std::vector<std::string> knowntypes;

        //size_t iTokenIndex;
        kTokenClass currentTokenClass;


    };
}