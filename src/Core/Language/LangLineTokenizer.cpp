//
// See 'LangLineTokenizer.h' for details...
//
#include <stdlib.h>
#include <vector>
#include <string>
#include <algorithm>
#include <string.h>

#include "LangLineTokenizer.h"
#include "Core/Editor.h"
#include <assert.h>
#include "logger.h"
#include "Core/StrUtil.h"
using namespace gedit;

size_t LangLineTokenizer::ParseRegion(std::vector<Line::Ref> &lines, size_t idxLineStart, size_t idxLineEnd) {
    size_t idxStart = StartParseRegion(lines, idxLineStart);
    size_t idxEnd = EndParseRegion(lines, idxLineEnd);
    auto logger = gnilk::Logger::GetLogger("LangLineRegion");

    if (!ResetStateStack()) {
        return 0;
    }

    size_t nMaxLines = Config::Instance()["languages"].GetInt("regionMaxLines", 1000);

    PushState(startState.c_str());
    int nextIndent = lines[idxStart]->Indent();

    logger->Debug("ParseRegion mapped, idxStart=%zu => %zu, idxEnd=%zu => %zu, stateStack=%zu",
                  idxLineStart, idxStart, idxStart, idxEnd,stateStack.size());

    while(true) {
        if (idxStart >= lines.size()) {
            break;
        }
        auto l = lines.at(idxStart);

        l->SetIndent(nextIndent);
        ParseLine(l, nextIndent);
        idxStart++;
        if ((idxStart > idxEnd) && (stateStack.size() == 1)) break;
        if (idxStart > nMaxLines) break;
    }

    // Let's pop the  'start'
    auto top = stateStack.top();
    PopState();

    if (!stateStack.empty()) {
        // emit warning!
        logger->Debug("ParseRegion, done but state stack not empty!!");
    }
    return stateStack.size();
}

// Try calculate the start of the parse region given a bunch of lines and the idx to the start for the calculation
// This seeks backwards in the list of lines until the stack-depth is 0
size_t LangLineTokenizer::StartParseRegion(std::vector<Line::Ref> &lines, size_t idxRegion) {
    size_t idxStart = 0;
    if ((idxRegion < 5) || (idxRegion > lines.size())) {
        return idxStart;
    }

    // search backwards until the state-stack depth == 0
    idxStart = idxRegion-1;
    while((lines[idxStart]->GetStateStackDepth() > 1) && (idxStart != 0)) {
        idxStart--;
    }
    return idxStart;
}

//
// Try figure out the end of the parse region by seeking forward and then some
// Note: THIS DOES NOT WORK for things like enter block-comment at top-of-file
//
size_t LangLineTokenizer::EndParseRegion(std::vector<Line::Ref> &lines, size_t idxRegion) {
    if ((lines.size() < 5) || (idxRegion > lines.size())) {
        return lines.size();
    }
    if (idxRegion > (lines.size()-5)) {
        return lines.size();
    }

    auto currentModel = Editor::Instance().GetActiveModel();
    if (currentModel->IsSelectionActive()) {
        auto selection = currentModel->GetSelection();
        idxRegion = selection.GetEnd().y;
    }

    idxRegion += 1;
    while(lines[idxRegion]->GetStateStackDepth() > 1) {
        idxRegion++;
        if (idxRegion >= lines.size()) break;
    }
    return idxRegion;
}

//
// Parse a set of lines
//
void LangLineTokenizer::ParseLines(std::vector<Line::Ref> &lines) {
    if (!ResetStateStack()) {
        return;
    }

    PushState(startState.c_str());

    size_t lineCounter = 0;
    int nextIndent = 0;
    for(auto &l : lines) {
        if (l == nullptr) {
            // this can happen - seen it, not sure why...
            // seems to happen if we delete a line and then update, but should really cause npe..
            return;
        }
        l->SetIndent(nextIndent);
        ParseLine(l, nextIndent);

        lineCounter++;
    }

    // Let's pop the  'start'
    auto top = stateStack.top();
    PopState();
    if (!stateStack.empty()) {
        // emit warning!
    }
}

// Parse a single line and compute the next line indentation..
void LangLineTokenizer::ParseLine(const Line::Ref l, int &nextIndent) {
    l->Lock();

    std::vector<LangToken> tokens;
    int currentIndentCounter = nextIndent;

    l->SetStateStackDepth((int)stateStack.size());

    if (l->Buffer().empty()) {
        l->Release();
        return;
    }
    ParseLineWithCurrentState(tokens, l->Buffer());

    // Indent handling
    if (std::find_if(tokens.begin(), tokens.end(), [](LangToken &tClass)->bool {
        return ((tClass == kLanguageTokenClass::kCodeBlockEnd) || (tClass == kLanguageTokenClass::kArrayEnd));
    }) != std::end(tokens)) {
        nextIndent--;
        // NOTE: This can happen during editing when inserting multiple like: { } } }
        if (nextIndent < 0) {
            nextIndent = 0;
        }
        assert(nextIndent >= 0);
    }
    // Did we have a block end, then we should decrease indent for current line
    if (nextIndent < currentIndentCounter) {
        l->SetIndent(nextIndent);
    }

    // If this line also has a block start - increase indent for next line..
    if (std::find_if(tokens.begin(), tokens.end(), [l](LangToken &tClass)->bool {
        return ((tClass == kLanguageTokenClass::kCodeBlockStart) || (tClass == kLanguageTokenClass::kArrayStart));
    }) != std::end(tokens)) {
        nextIndent++;
    }

    // Done, convert to line-attributes..
    LangToken::ToLineAttrib(l->Attributes(), tokens);

    // End shared.
    l->Release();
}

//
// ParseLine start from a specific state
// This should only be used by specific stuff - like 'makefile' output parsing and so forth...
//
// Instead we should have a parse function which start at a specific state and parses until that state is reached..
//
void LangLineTokenizer::ParseLineFromState(const std::string &lineStartState, Line::Ref line) {
    // Reset the state stack, start all over...
    if (!ResetStateStack()) {
        return;
    }
    // Push this first, IF we are in a block (like a block comment) we want to break out to the start state
    // And if we don't push this here - we will simply pop 'null' when (if) the block ends during editing...
    PushState(startState.c_str());

    PushState(lineStartState.c_str());
    std::vector<LangToken> tokens;

    ParseLineWithCurrentState(tokens, line->Buffer());
    LangToken::ToLineAttrib(line->Attributes(), tokens);

    PopState();
}

//
// ParseLine start from current state on the stack..
//
// This is the heavy lifting, part 1
// Internal, assumes the current state has been properly set-up
//
void LangLineTokenizer::ParseLineWithCurrentState(std::vector<LangToken> &tokens, const std::u32string &input) {
    // Max token length...
    char rawToken[GEDIT_MAX_LANG_TOKEN_LENGTH];
    std::u32string nextToken;
    //char *parsepoint = (char *) input;
    auto it = input.begin();

    while(true) {
        auto currentState = stateStack.top();
        if (currentState == nullptr) {
            return;
        }

        nextToken.clear();
        auto [ok, classification] = GetNextToken(nextToken, input, it, input.end());
        if (!ok) {
            break;
        }
//        auto tmp = UnicodeHelper::utf32toascii(nextToken);
//        printf("s: %s, nexttok: '%s'\n", currentState->name.c_str(), tmp.c_str());

        if (nextToken.empty()) {
            return;
        }

        // Get a token and the classification...
        int pos = static_cast<int>(it - input.begin());
        pos -= nextToken.size();

        classification = CheckExecuteActionForToken(currentState, nextToken, classification);
        //printf("  classification: %d\n", classification);
        // If this is regular text - reclassify it depending on the state (this allows for comments/string and other
        // encapsulation statements to override... (#include)
        if (classification == kLanguageTokenClass::kRegular) {
            classification = currentState->regularTokenClass;
        }
        LangToken token { .string = nextToken, .idxOrigStr = pos, .classification = classification };
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
kLanguageTokenClass LangLineTokenizer::CheckExecuteActionForToken(State::Ref currentState, const std::u32string &token, kLanguageTokenClass tokenClass) {
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
std::pair<bool, kLanguageTokenClass> LangLineTokenizer::GetNextToken(std::u32string &dst, const std::u32string &currentLine, std::u32string::const_iterator &itInput, const std::u32string::const_iterator &last) {

    auto currentState = stateStack.top();
    assert(currentState != nullptr);

    if (!strutil::skipWhiteSpace(itInput)) {
        return {false, kLanguageTokenClass::kUnknown};
    }


    // stringview would probably be better/smarter here
    auto strInput = std::u32string_view(itInput, last);


    int szOperator = 0;
    // Check if we have an identifier in the current state

    // Classify identifiers which can be attached, like operators: ++token  <- ++ is an attached
    for(auto &kvp : currentState->identifiers) {
        if (kvp.second->isWholeWord) {
            continue;
        }
        if (!kvp.second->IsMatch(strInput, szOperator)) {
            continue;
        }

        // we had a match, copy it as the token and return the classification..
        dst = strInput.substr(0, szOperator);
        itInput += szOperator;

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

    auto chkPostFix= [currentState,&szOperator,&last](const std::u32string::const_iterator it)->bool {
        // Currentstate can't be null here...
        if (currentState->postfixIdentifiers == nullptr) {
            return false;
        }
        auto str = std::u32string_view(it, last);
        return currentState->postfixIdentifiers->IsMatch(str, szOperator);
    };

    dst.clear();
    while((itInput != last) && !strutil::isspace(*itInput) && !chkPostFix(itInput)) {
        dst.push_back(*itInput);
        itInput++;
    }

    // classify whole word tokens
    for(auto &kvp : currentState->identifiers) {
        if (!kvp.second->isWholeWord) {
            continue;
        }
        if (!kvp.second->IsMatch(strInput, szOperator)) {
            continue;
        }
        return {true, kvp.second->classification};
    }

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
