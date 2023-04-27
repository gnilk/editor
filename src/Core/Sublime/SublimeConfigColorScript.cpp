//
// Created by gnilk on 25.01.23.
//

#include "SublimeConfigColorScript.h"
#include "Core/StrUtil.h"
#include "Core/Tokenizer.h"

// Execute a color script (i.e. returns a color and not script value) - Specialization
std::pair<bool, ColorRGBA> SublimeConfigColorScript::ExecuteColorScript(const std::string &script) {

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

// Register built-in functionality
// FIXME: IF this gets used for different purposes than colors - we should move this to a specialization class (SublimeColorScript)
void SublimeConfigColorScript::RegisterBuiltIn() {
    // Make sure we register whatever is in the base class...
    SublimeConfigScriptEngine::RegisterBuiltIn();

    RegisterFunction("hsl",[this](std::vector<SublimeConfigScriptEngine::ScriptValue> &args)->SublimeConfigScriptEngine::ScriptValue {
        return this->ExecuteHSL(args);
    });
    RegisterFunction("hsla",[this](std::vector<SublimeConfigScriptEngine::ScriptValue> &args)->SublimeConfigScriptEngine::ScriptValue {
        return this->ExecuteHSLA(args);
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

const ColorRGBA SublimeConfigColorScript::GetColor(const std::string &name) const {
    if (!HasVariable(name)) {
        return {};
    }
    auto var = GetVariable(name);
    if (!var.IsColor()) {
        return {};
    }
    return var.Color();
};

//
// Built-in functionality
//
SublimeConfigScriptEngine::ScriptValue SublimeConfigColorScript::ExecuteHSL(std::vector<ScriptValue> &args) {
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

SublimeConfigScriptEngine::ScriptValue SublimeConfigColorScript::ExecuteHSLA(std::vector<ScriptValue> &args) {

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

SublimeConfigScriptEngine::ScriptValue SublimeConfigColorScript::ExecuteColor(std::vector<ScriptValue> &args) {
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
    } else {
        return { .vType = kColor, .data = color.Color() };
    }
    return invalidScriptValue;
}

SublimeConfigScriptEngine::ScriptValue SublimeConfigColorScript::ExecuteAlpha(std::vector<ScriptValue> &args) {
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

SublimeConfigScriptEngine::ScriptValue SublimeConfigColorScript::ExecuteRGB(std::vector<ScriptValue> &args) {
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
SublimeConfigScriptEngine::ScriptValue SublimeConfigColorScript::ExecuteRGBA(std::vector<ScriptValue> &args) {
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
