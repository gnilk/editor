//
// Created by gnilk on 25.01.23.
//
// This implements a small script engine based on the Sublime Theme/Color Configuration rules
// Could be generalized to something else if need to...
//
// Note: Just one big global scope...
//

#include <string>
#include <functional>

#include "Core/ColorRGBA.h"
#include "Core/StrUtil.h"
#include "Core/Tokenizer.h"

#include "SublimeConfigScriptEngine.h"



// FIXME: Move to class member
static std::string colScriptOp = {"( ) % , #"};

// Defined the 'invalidValue'
const SublimeConfigScriptEngine::ScriptValue SublimeConfigScriptEngine::invalidScriptValue = {.vType = kNil, .data = nullptr };

// Register built-in functionality
// FIXME: IF this gets used for different purposes than colors - we should move this to a specialization class (SublimeColorScript)
void SublimeConfigScriptEngine::RegisterBuiltIn() {
    RegisterFunction("var",[this](std::vector<SublimeConfigScriptEngine::ScriptValue> &args)->SublimeConfigScriptEngine::ScriptValue {
        return this->ExecuteVAR(args);
    });

}

// Register a global scope function (just add to the function map)
void SublimeConfigScriptEngine::RegisterFunction(const std::string &name, FunctionDelegate function) {
    functions[name] = function;
}


// Add a variable to the global scope
void SublimeConfigScriptEngine::AddVariable(const std::string &name, ScriptValue value) {
    printf("Add Variable: %s\n", name.c_str());
    variables[name] = value;
}

// Return a variable from the global scope
const SublimeConfigScriptEngine::ScriptValue SublimeConfigScriptEngine::GetVariable(const std::string &name) const {
    if (!HasVariable(name)) {
        return invalidScriptValue;
    }
    auto it = variables.find(name);
    return it->second;
//    return variables[name];
}


//
// see: https://www.sublimetext.com/docs/color_schemes.html
//
//
// This executes a specific set of functions...
// Let's fix this so we can execute:
// color(var(blue3) alpha(0.5))
// 1) get token
// 2) peek next - if '(' we have a function call
//    the result of a function call is _ALWAYS_ a 'ScriptVariable'
//    thus we can nicely build this recursively
//
// problem: color(var(blue3) alpha(0.5))
//
// var -> will return ColorRGBA
// alpha -> will return a number -> this is a special adjuster...   thus: color(col adjuster)
//
// which means that 'color' as a function must take two arguments and have the logic to replace the alpha in color..
//
//
std::pair<bool, SublimeConfigScriptEngine::ScriptValue> SublimeConfigScriptEngine::ExecuteScript(const std::string &script) {
    gnilk::Tokenizer tokenizer(script.c_str(), colScriptOp.c_str());
    return  ExecuteWithTokenizer(tokenizer);
}


// Execute something which has been pre-processed already
std::pair<bool, SublimeConfigScriptEngine::ScriptValue> SublimeConfigScriptEngine::ExecuteWithTokenizer(gnilk::Tokenizer &tokenizer) {
    auto token = tokenizer.Peek();

    bool result = false;
    ScriptValue value = {};

    if (HasFunction(token)) {
        std::tie(result, value) = ExecuteFunction(tokenizer);
    } else if (token == std::string("#")) {
        std::tie(result, value) = ParseWebColor(tokenizer);
    } else if (strutil::isinteger(token)) {
        std::tie(result, value) = ParseInteger(tokenizer);
    } else if (strutil::isdouble(token)) {
        std::tie(result, value) = ParseDouble(tokenizer);
    } else {
        // We assume these tokens are strings...  great!   =)
        tokenizer.Next();   // Consume string
        result = true;
        value = {.vType = kString, .data = std::string(token)};
    }
    return {result, value};
}

//
// Parse and Execute a function
//
std::pair<bool, SublimeConfigScriptEngine::ScriptValue> SublimeConfigScriptEngine::ExecuteFunction(gnilk::Tokenizer &tokenizer) {
    auto funcName = tokenizer.Next();

    if (tokenizer.Peek() != std::string("(")) {
        printf("Syntax error: expected '(' after function name but got '%s'\n", tokenizer.Peek());
        return {false, {}};
    }

    // FIXME: The parsing is flaky...
    std::vector<ScriptValue> args;
    tokenizer.Next();
    while(tokenizer.HasMore()) {
        if (tokenizer.Peek() == std::string(")")) {
            // Execute and return..
            tokenizer.Next(); // consume ')'
            //printf("EXECUTE: %s, with: %d args\n", funcName, (int)args.size());
            auto value = functions[funcName](args);
            return {true, {value}};
        }
        auto [ok, value] = ExecuteWithTokenizer(tokenizer);
        args.push_back(value);

        // Check for next argument..
        if (tokenizer.Peek() == std::string(",")) {
            tokenizer.Next();
        }
    }

    return {false, {}};
}

//
// Parse web color definintion
//  Color is either #<RR><GG><BB> or #<R><G><B>
//  FIXME: support with alpha! #<RR><GG><BB><AA>
//
std::pair<bool, SublimeConfigScriptEngine::ScriptValue> SublimeConfigScriptEngine::ParseWebColor(gnilk::Tokenizer &tokenizer) {
    tokenizer.Next();   // eat '#'
    std::string token(tokenizer.Next());

    if ((token.length() != 6) && (token.length() != 3)) {
        printf("ERR: Hex color description to short, format is: #RRGGBB (at least 6 values!!)\n");
        return {false, {}};
    }

    std::string strRed, strGreen, strBlue;
    if (token.length() == 6) {
        strRed = std::string(token,0,2);
        strGreen = std::string(token,2,2);
        strBlue = std::string(token,4,2);
    } else {
        // Short form
        strRed = token[0];
        strGreen = token[1];
        strBlue = token[2];
    }


    auto col = ColorRGBA::FromRGB(strutil::hex2dec(strRed.c_str()),
                                  strutil::hex2dec(strGreen.c_str()),
                                  strutil::hex2dec(strBlue.c_str()));

    return {true, {.vType = SublimeConfigScriptEngine::kColor, .data = col}};
}

//
// Parse an integer. Integers are defined as numbers without the '.'
// Note: The script engine can detect double and integers but treat everything as numbers
//
std::pair<bool, SublimeConfigScriptEngine::ScriptValue> SublimeConfigScriptEngine::ParseInteger(gnilk::Tokenizer &tokenizer) {
    auto vStr = tokenizer.Next();
    float v = (float)atoi(vStr);

    if (tokenizer.HasMore()) {
        switch(tokenizer.Case(tokenizer.Peek(), "%")) {
            case 0 :
                tokenizer.Next();   // swallow '%'
                v = v / 100.0f;
                break;
        }
    }

    return {true, {.vType = SublimeConfigScriptEngine::kNumber, .data = v}};
}

//
// Parse a double. Double are defined as <xx.yy> (i.e. numbers with a decimal point)
// Note: The script engine can detect double and integers but treat everything as numbers
//
std::pair<bool, SublimeConfigScriptEngine::ScriptValue> SublimeConfigScriptEngine::ParseDouble(gnilk::Tokenizer &tokenizer) {
    auto vStr = tokenizer.Next();
    float v = atof(vStr);

    if (tokenizer.HasMore()) {
        switch(tokenizer.Case(tokenizer.Peek(), "%")) {
            case 0 :
                tokenizer.Next();   // swallow '%'
                v = v / 100.0f;
                break;
        }
    }

    return {true, {.vType = SublimeConfigScriptEngine::kNumber, .data = v}};
}

//
// built in functionality
//

SublimeConfigScriptEngine::ScriptValue SublimeConfigScriptEngine::ExecuteVAR(std::vector<ScriptValue> &args) {
    if (args.size() != 1) {
        printf("Err: Parameter mismatch, 'var' expects a single parameter - the variable name\n");
        printf("  var(<name of variable>)\n");
        return invalidScriptValue;
    }
    if (!args[0].IsString()) {
        printf("Err: Type mismatch, parameter type is not string\n");
        return invalidScriptValue;
    }
    if (!HasVariable(args[0].String())) {
        printf("Err: no variable named '%s'\n", args[0].String().c_str());
        return invalidScriptValue;
    }
    return GetVariable(args[0].String());
}

