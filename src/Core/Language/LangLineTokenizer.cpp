//
// See 'LangLineTokenizer.h' for details...
//
#include <stdlib.h>
#include <vector>
#include <string>
#include <algorithm>
#include <string.h>

#include "LangLineTokenizer.h"
#include <assert.h>
using namespace gedit;

//
// Parse a set of lines
//
void LangLineTokenizer::ParseLines(std::vector<Line::Ref> &lines) {
    if (!ResetStateStack()) {
        return;
    }

    std::vector<LangToken> tokens;

    PushState(startState.c_str());

    int indentCounter = 0;
    int lineCounter = 0;
    for(auto &l : lines) {
        std::vector<LangToken> tokens;

        l->Lock();
        l->SetStateStackDepth((int)stateStack.size());
        ParseLineWithCurrentState(tokens, l->Buffer().data());
        // Indent handling
//        if (std::find(tokens.begin(), tokens.end(), kLanguageTokenClass::kCodeBlockStart) != std::end(tokens)) {
//            indentCounter++;
//        }
        if (std::find_if(tokens.begin(), tokens.end(), [](LangToken &tClass)->bool {
            return ((tClass == kLanguageTokenClass::kCodeBlockStart) | (tClass == kLanguageTokenClass::kArrayStart));
        }) != std::end(tokens)) {
            indentCounter++;
        }
        if (std::find_if(tokens.begin(), tokens.end(), [](LangToken &tClass)->bool {
            return ((tClass == kLanguageTokenClass::kCodeBlockEnd) | (tClass == kLanguageTokenClass::kArrayEnd));
        }) != std::end(tokens)) {
            indentCounter--;
            // NOTE: This can happen during editing when inserting multiple like: { } } }
            if (indentCounter < 0) {
                indentCounter = 0;
            }
            assert(indentCounter >= 0);
        }
        l->SetIndent(indentCounter);

        LangToken::ToLineAttrib(l->Attributes(), tokens);
        l->Release();

        tokens.clear();
        lineCounter++;
    }

    // Let's pop the  'start'
    auto top = stateStack.top();
    PopState();
    if (!stateStack.empty()) {
        // emit warning!
    }
}

// Parse a single line
// Note: Don't use this - this is a bit bogus...
void LangLineTokenizer::ParseLine(std::vector<LangToken> &tokens, const char *input) {
    if (!ResetStateStack()) {
        return;
    }
    PushState(startState.c_str());
    ParseLineWithCurrentState(tokens, input);
    PopState();
}

//
// ParseLine start from a specific state
// This has flaws, considering parsing a single line when entering a block-comment...
//
// Instead we should have a parse function which start at a specific state and parses until that state is reached..
//
void LangLineTokenizer::ParseLineFromStartState(std::string &lineStartState, Line::Ref line) {
    // Reset the state stack, start all over...
    if (!ResetStateStack()) {
        return;
    }
    // Push this first, IF we are in a block (like a block comment) we want to break out to the start state
    // And if we don't push this here - we will simply pop 'null' when (if) the block ends during editing...
    PushState(startState.c_str());

    PushState(lineStartState.c_str());
    std::vector<LangToken> tokens;

    ParseLineWithCurrentState(tokens, line->Buffer().data());
    LangToken::ToLineAttrib(line->Attributes(), tokens);
    PopState();
}

//
// ParseLine start from current state on the stack..
//
// This is the heavy lifting, part 1
// Internal, assumes the current state has been properly set-up
//
void LangLineTokenizer::ParseLineWithCurrentState(std::vector<LangToken> &tokens, const char *input) {
    // Max token length...
    char rawToken[GEDIT_MAX_LANG_TOKEN_LENGTH];
    char *parsepoint = (char *) input;


    while(true) {
        auto currentState = stateStack.top();
        if (currentState == nullptr) {
            return;
        }

        // Get a token and the classification...
        auto [ok, classification] = GetNextToken(rawToken, 256, &parsepoint);
        if (!ok) {
            break;
        }

        // printf("s: %s, tok: %s\n", currentState->name.c_str(), tmp);

        int len = strlen(rawToken);
        if (len == 0) {
            return;
        }
        int pos = (parsepoint - input) - len;
        classification = CheckExecuteActionForToken(currentState, rawToken, classification);
        // If this is regular text - reclassify it depending on the state (this allows for comments/string and other
        // encapsulation statements to override... (#include)
        if (classification == kLanguageTokenClass::kRegular) {
            classification = currentState->regularTokenClass;
        }
        LangToken token { .string = std::string(rawToken), .idxOrigStr = pos, .classification = classification };
        //tokens.push_back(token);
        tokens.emplace_back(token);
    }

    if (!stateStack.empty()) {
        auto currentState = stateStack.top();
        if (currentState->eolAction.action == kAction::kNone) {
            return;
        }
        PopState();
    }

}

//
// Reset the state stack, empty any left-overs (this is not good) and set the proper start state..
//
bool LangLineTokenizer::ResetStateStack() {
    if (!stateStack.empty()) {
        while(!stateStack.empty()) {
            stateStack.pop();
        }
    }

    if (!HasState(startState.c_str())) {
        // This is developer related, we can exit here...
        printf("ERR: Illegal start state!\n");
        exit(1);
    }

    return true;
}


//
// A Pop action will reclassify the token using the popped state as that's where the classification belongs..
// Generally an operator is causing a push and there is another operator causing the pop...
// Example:
//   Comments, start/end - block comment operators
//   assume we have a comment like this:  /* this is a block comment */
//
//  1) We will hit '/*' and push new state 'block_comment' on to the stack
//  2) block comment will ignore everything until it finds '*/' where it will break
//
//  now - '*/' is consumed and parsed by the 'block_comment' state (which is all good)
//  but we really want it classified by the outer/parent state.
//
kLanguageTokenClass LangLineTokenizer::CheckExecuteActionForToken(State::Ref currentState, const char *token, kLanguageTokenClass tokenClass) {
    // Check if we have an action for this token
    if (!currentState->HasActionForToken(token)) {
        return tokenClass;
    }

    auto action = currentState->GetAction(token);
    if (action->action == kAction::kPopState) {

        auto oldState  = PopState();
        currentState = stateStack.top();

        // printf("PopState, %s -> %s (at token: %s)\n", oldState->name.c_str(), stateStack.top()->name.c_str(), token);

        // When we pop, the token causing the pop belongs to the popped-context
        // As it is the tail end of the token causing the push in the first place...   <- read several times...

        // Rewrite classification...
        auto [ok, newTokenClass] = currentState->ClassifyToken(token);
        if (ok) {
            // printf("  Reclassification: %d -> %d\n", tokenClass, newTokenClass);
            tokenClass = newTokenClass;
        }
    } else if (action->action == kAction::kPushState) {
        // printf("PushState, %s -> %s (at token: %s)\n", currentState->name.c_str(), action->stateName.c_str(), token);
        PushState(action->stateName.c_str());
    }

    return tokenClass;
}



//
//
// state handling and also (presumable) state handling between call's into the instance...
//        reasons:
//        - Keep C/CPP strings "\""  - strings have 'escape' operator
//        - /*   - is multi-line
//        - probably a lot more...
//
// - Basically
// - 1) There is an inital state (main - or whatever)
// - 2) Each state have their own set of 'operators', 'keywords', etc...
// - 3) Each operator (or group thereof) can push a new state
// - 4) Each push must have a rule to pop the state back to main..
//
// Example: main (rules should be more complex)
//          - name: block comment start
//            - state: main
//            - match: /*
//            - result: operator.block_comment_start
//            - action: push state.block_comment
//          - name: block comment end
//            - state: block_comment
//            - match: */
//            - result: operator.block_comment_end
//            - action: pop
//          - name: string operator
//            - state: main
//            - match: "
//            - result: operator.string
//            - action: push state.inside_c_string
//          - name: string end operator
//            - state: inside_c_string
//            - match: " but not \"
//            - action: pop
//          - name: raw_string
//            - state: main
//            - match: R"_<prefix>
//            - result: operator.raw_string
//            - action: push state.inside_cpp_rawstring
//          - name: raw_string_end_operator
//            - state: inside_cpp_rawstring
//            - match: <prefix>_"
//            - result: operator.raw_string
//            - action: push state.inside_cpp_rawstring
//
//
// If I adopt this way of dealing with it - I can reuse the language definitions from Sublime..
// Which obviously would save me quite a lot of time...
//
//
//
//
std::pair<bool, kLanguageTokenClass> LangLineTokenizer::GetNextToken(char *dst, int nMax, char **input) {

    auto currentState = stateStack.top();
    assert(currentState != nullptr);

    if (!strutil::skipWhiteSpace(input)) {
        return {false, kLanguageTokenClass::kUnknown};
    }
    int szOperator = 0;

    // Check if we have an identifier in the current state
    for(auto &kvp : currentState->identifiers) {
        if (!kvp.second->IsMatch(*input, szOperator)) {
            continue;
        }

        // we had a match, copy it as the token and return the classification..
        strncpy(dst, *input, szOperator);
        (*input) += szOperator;
        // Not needed - but makes rules easier...
        dst[szOperator] = '\0';
        return {true, kvp.second->classification};
    }

    //
    // No identifier, so just a regular token (something user defined)
    // Such tokens can have a post-fix operator so we need something that 'breaks' parsing...
    // Each state has a set of post-fix operators that are allowed to be place directly in conjunction with the
    // token itself...
    // like: variable++;  where '++' is a postfix token..
    // or:   myInstance;  where ';' is a postfix token..
    //
    // generally the postfix tokens are same as operators - but I've opted to put them separate - not sure if this
    // always holds true...
    //


    // This is just to avoid clutter in the while-loop below
    int idxDst=0;
    auto chkPostFix= [currentState, input, &szOperator](char *dst)->bool {
        // Currentstate can't be null here...
        if (currentState->postfixIdentifiers == nullptr) {
            return false;
        }
        return currentState->postfixIdentifiers->IsMatch(*input, szOperator);
    };

    // Get the token...
    while ((**input != '\0') && !isspace(**input) && !chkPostFix(dst)) {
        dst[idxDst++] = **input;
        // This is a developer problem, ergo - safe to exit..
        if (idxDst >= nMax) {
            fprintf(stderr, "ERR: GetNextToken, token size larger than buffer (>nMax)\n");
            exit(1);
        }
        (*input)++;
    }

    // Make sure we terminate, and then we
    dst[idxDst] = '\0';
    return {true, kLanguageTokenClass::kRegular};
}

//
// State handling follows below
//

void LangLineTokenizer::SetStartState(const std::string &newStartState) {
    startState = newStartState;
}


LangLineTokenizer::State::Ref LangLineTokenizer::GetOrAddState(const char *stateName) {
    if (states.find(stateName) == states.end()) {
        auto state = State::Factory();
        state->name = stateName;
        states[stateName] = state;
    }
    return states[stateName];
}
bool LangLineTokenizer::HasState(const char *stateName) {
    if (states.find(stateName) == states.end()) {
        return false;
    }
    return true;
}
LangLineTokenizer::State::Ref LangLineTokenizer::GetState(const char *stateName) {
    return states[stateName];
}

bool LangLineTokenizer::PushState(const char *stateName) {
    if (!HasState(stateName)) {
        return false;
    }
    PushState(states[stateName]);
    return true;
}
void LangLineTokenizer::PushState(State::Ref state) {
    stateStack.push(state);
}
LangLineTokenizer::State::Ref LangLineTokenizer::PopState() {
    auto top = stateStack.top();
    stateStack.pop();
    return top;
}

LangLineTokenizer::State::Ref LangLineTokenizer::CurrentState() {
    return stateStack.top();
}
