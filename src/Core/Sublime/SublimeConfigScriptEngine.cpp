//
// Created by gnilk on 25.01.23.
//
// FIXME: Move to class member

#include <string>
#include <functional>

#include "Core/ColorRGBA.h"
#include "Core/StrUtil.h"
#include "Core/Tokenizer.h"

#include "SublimeConfigScriptEngine.h"



static std::string colScriptOp = {"( ) % , #"};

const SublimeConfigScriptEngine::ScriptValue SublimeConfigScriptEngine::invalidScriptValue = {.vType = kNil, .data = nullptr };


void SublimeConfigScriptEngine::RegisterBuiltIn() {
    RegisterFunction("hsl",[this](std::vector<SublimeConfigScriptEngine::ScriptValue> &args)->SublimeConfigScriptEngine::ScriptValue {
        return this->ExecuteHSL(args);
    });
    RegisterFunction("hsla",[this](std::vector<SublimeConfigScriptEngine::ScriptValue> &args)->SublimeConfigScriptEngine::ScriptValue {
        return this->ExecuteHSLA(args);
    });
    RegisterFunction("var",[this](std::vector<SublimeConfigScriptEngine::ScriptValue> &args)->SublimeConfigScriptEngine::ScriptValue {
        return this->ExecuteVAR(args);
    });
    RegisterFunction("color",[this](std::vector<SublimeConfigScriptEngine::ScriptValue> &args)->SublimeConfigScriptEngine::ScriptValue {
        return this->ExecuteColor(args);
    });
    RegisterFunction("alpha",[this](std::vector<SublimeConfigScriptEngine::ScriptValue> &args)->SublimeConfigScriptEngine::ScriptValue {
        return this->ExecuteAlpha(args);
    });
    RegisterFunction("rgb",[this](std::vector<SublimeConfigScriptEngine::ScriptValue> &args)->SublimeConfigScriptEngine::ScriptValue {
        return this->ExecuteRGB(args);
    });
    RegisterFunction("rgba",[this](std::vector<SublimeConfigScriptEngine::ScriptValue> &args)->SublimeConfigScriptEngine::ScriptValue {
        return this->ExecuteRGBA(args);
    });
}


void SublimeConfigScriptEngine::AddVariable(const std::string &name, ScriptValue value) {
    printf("Add Variable: %s\n", name.c_str());
    variables[name] = value;
}

SublimeConfigScriptEngine::ScriptValue SublimeConfigScriptEngine::GetVariable(const std::string &name) {
    if (!HasVariable(name)) {
        return invalidScriptValue;
    }
    return variables[name];
}


const SublimeConfigScriptEngine::ScriptValue &SublimeConfigScriptEngine::GetValue(const std::string &name) {
    static ScriptValue value = {.vType = kNil};
    if (variables.find(name) == variables.end()) {
        return value;
    }
    return variables[name];
}

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


SublimeConfigScriptEngine::ScriptValue SublimeConfigScriptEngine::ExecuteHSL(std::vector<ScriptValue> &args) {
    if (args.size() != 3) {
        printf("Err: Parameter mismatch, HSL expects 3 paramters\n");
        printf("  HSLA(H L S)");
        return {};
    }
    for (int i=0;i<3;i++) {
        if (args[i].vType != SublimeConfigScriptEngine::kNumber) {
            printf("Err: Parameter %d is not a number!\n", i);
            return {};
        }
    }
    return {.vType = SublimeConfigScriptEngine::kColor, .data = {ColorRGBA::FromHSL(args[0].Number(), args[1].Number(), args[2].Number())}};
}

SublimeConfigScriptEngine::ScriptValue SublimeConfigScriptEngine::ExecuteHSLA(std::vector<ScriptValue> &args) {

    if (args.size() != 4) {
        printf("Err: Parameter mismatch, HSLA expects 4 paramters\n");
        printf("  HSLA(H, L, S, A)");
        return invalidScriptValue;
    }
    for (int i=0;i<4;i++) {
        if (args[i].vType != SublimeConfigScriptEngine::kNumber) {
            printf("Err: Parameter %d is not a number!\n", i);
            return invalidScriptValue;
        }
    }
    return {.vType = SublimeConfigScriptEngine::kColor, .data = {ColorRGBA::FromHSLA(args[0].Number(), args[1].Number(), args[2].Number(), args[3].Number())}};
}

SublimeConfigScriptEngine::ScriptValue SublimeConfigScriptEngine::ExecuteVAR(std::vector<ScriptValue> &args) {
    if (args.size() != 1) {
        printf("Err: Parameter mismatch, VAR expects 1 argument");
        printf("   var(name_of_variable)\n");
        return invalidScriptValue;
    }
    auto param = args[0];
    if (param.vType != SublimeConfigScriptEngine::kString) {
        printf("Err: Argument type mismatch, only strings allowed\n");
        return invalidScriptValue;
    }

    return GetVariable(param.String());
}

SublimeConfigScriptEngine::ScriptValue SublimeConfigScriptEngine::ExecuteColor(std::vector<ScriptValue> &args) {
    if ((args.size() < 1) || (args.size() > 2)) {
        printf("Err: Parameter mismatch, COLOR expects 1 or 2 arguments");
        printf("   color(<color> OPTIONAL:<adjuster>)\n");
        return invalidScriptValue;
    }
    auto color = args[0];
    if (!color.IsColor()) {
        printf("Err: Argument type mismatch, color (first) argument must be a color!\n");
        return invalidScriptValue;
    }

    // Adjust With - can be a color...
    if (args.size() > 1) {
        if (args[1].IsNumber()) {
            return {.vType = kColor, .data = color.Color() * args[1].Number() };
        } else if (args[1].IsColor()) {
            return {.vType = kColor, .data = color.Color() * args[1].Color() };
        } else {
            printf("Err: Argument type mismatch, adjuster (second) argument must be number or color!\n");
        }
    }
    return invalidScriptValue;
}

SublimeConfigScriptEngine::ScriptValue SublimeConfigScriptEngine::ExecuteAlpha(std::vector<ScriptValue> &args) {
    if (args.size() != 1) {
        printf("Err: Parameter mismatch, alpha expects 1 argument\n");
        printf("  alpha(number)\n");
        return invalidScriptValue;
    }
    if (!args[0].IsNumber()) {
        printf("Err: Argument type mismatch, not a number\n");
        return invalidScriptValue;
    }
    // Construct a color with {1,1,1 <alpha>}
    ColorRGBA col;
    col.SetAlpha(args[0].Number());
    return {.vType = kColor, .data = col};
}

SublimeConfigScriptEngine::ScriptValue SublimeConfigScriptEngine::ExecuteRGB(std::vector<ScriptValue> &args) {
    if (args.size() != 3) {
        printf("Err: Parameter mismatch, rgb expects 3 arguments\n");
        printf("  rgb(r, g, b) - all numbers\n");
        return invalidScriptValue;
    }
    if (!args[0].IsNumber() || !args[1].IsNumber() || !args[2].IsNumber()) {
        printf("Err: not a number\n");
        return invalidScriptValue;
    }
    // Specification says that rgb accepts three integers in range 0..255
    // see: https://www.sublimetext.com/docs/color_schemes.html under 'Colors'
    return {.vType = kColor, .data = ColorRGBA::FromRGB(args[0].Number()/255.0f, args[1].Number()/255.0f, args[2].Number()/255.0f)};
}
SublimeConfigScriptEngine::ScriptValue SublimeConfigScriptEngine::ExecuteRGBA(std::vector<ScriptValue> &args) {
    if (args.size() != 4) {
        printf("Err: Parameter mismatch, rgba expects 4 arguments\n");
        printf("  rgb(r, g, b, a) - all numbers\n");
        return invalidScriptValue;
    }
    if (!args[0].IsNumber() || !args[1].IsNumber() || !args[2].IsNumber() || !(args[3].IsNumber())) {
        printf("Err: not a number\n");
        return invalidScriptValue;
    }
    // Specification says that rgba accepts three integers in range 0..255 and alpha in range 0..1 (float)
    // see: https://www.sublimetext.com/docs/color_schemes.html under 'Colors'
    return {.vType = kColor, .data = ColorRGBA::FromRGBA(args[0].Number()/255.0f, args[1].Number()/255.0f, args[2].Number()/255.0f, args[3].Number())};
}


// Not that hard...
void SublimeConfigScriptEngine::RegisterFunction(const std::string &name, FunctionDelegate function) {
    functions[name] = function;
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

// Specialization
std::pair<bool, ColorRGBA> SublimeConfigScriptEngine::ExecuteColorScript(const std::string &script) {

    auto [ok, value] = ExecuteScript(script);
    if (!ok) {
        return {false, {}};
    }
    if (value.vType != SublimeConfigScriptEngine::kColor) {
        printf("ERR: Script does not evaluate to a color!\n");
        return {false, {}};
    }
    // Ok, we have a color
    return {true, value.Color()};
}

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
            printf("EXECUTE: %s, with: %d args\n", funcName, (int)args.size());
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


std::pair<bool, SublimeConfigScriptEngine::ScriptValue> SublimeConfigScriptEngine::ParseWebColor(gnilk::Tokenizer &tokenizer) {
    tokenizer.Next();   // eat '#'
    std::string token(tokenizer.Next());

    //
    // Color is either #<RR><GG><BB> or #<R><G><B>
    //
    // TODO: support with alpha! #<RR><GG><BB><AA>
    //
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
