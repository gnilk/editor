//
// Created by gnilk on 23.01.23.
//
// testing various ways to draw attributed string
//
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <ncurses.h>
#include <nlohmann/json.hpp>
#include "Core/Line.h"

#include "Core/StrUtil.h"
#include "Tokenizer.h"

using json = nlohmann::json;

bool Open() {
    use_extended_names(TRUE);
    initscr();
    if (has_colors()) {
        // Just test it a bit...
        start_color();
        init_color(COLOR_GREEN, 200,1000,200);
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
        init_pair(2, COLOR_BLACK, COLOR_GREEN);
        attron(COLOR_PAIR(1));
    } else {
        printf("No colors, going with defaults...\n");
    }
    timeout(1); // Make 'getch' non-blocking..
    clear();
    //raw();
    keypad(stdscr, TRUE);
    scrollok(stdscr, TRUE);
    noecho();
    cbreak();
    //nonl();
    // Make this configurable...


    ESCDELAY = 1;
    // TODO: Add 'atexit' call which enforces call of 'endwin'
    return true;
}



void Close(void) {
    endwin();
}

class ColorRGBA {
public:
    //constexpr ColorRGBA() : r(1.0f), g(1.0f), b(1.0f), a(0.0f) { } ;
    ColorRGBA() = default;

    static ColorRGBA FromRGB(uint8_t r, uint8_t g, uint8_t b);
    static ColorRGBA FromHSL(float h, float s, float l);
    static ColorRGBA FromHSLA(float h, float s, float l, float a);
    static ColorRGBA FromHexStr(std::string &str);

    int RedAsInt(int mul = 255) {
        return r * (float)mul;
    }
    int GreenAsInt(int mul = 255) {
        return g * (float)mul;
    }
    int BlueAsInt(int mul = 255) {
        return b * (float)mul;
    }
    int AlphaAsInt(int mul = 255) {
        return a * (float)mul;
    }

    float R() { return r; }
    float G() { return g; }
    float B() { return b; }
    float A() { return a;}

private:
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 0.0f; // no transparency
};

struct LineAttrib {
    int cStart = 0;
    int cEnd = 0;
    int idxColorPair;
};

//static RGB HSLToRGB(HSL hsl) {
//    unsigned char r = 0;
//    unsigned char g = 0;
//    unsigned char b = 0;
//
//    if (hsl.S == 0)
//    {
//        r = g = b = (unsigned char)(hsl.L * 255);
//    }
//    else
//    {
//        float v1, v2;
//        float hue = (float)hsl.H / 360;
//
//        v2 = (hsl.L < 0.5) ? (hsl.L * (1 + hsl.S)) : ((hsl.L + hsl.S) - (hsl.L * hsl.S));
//        v1 = 2 * hsl.L - v2;
//
//        r = (unsigned char)(255 * HueToRGB(v1, v2, hue + (1.0f / 3)));
//        g = (unsigned char)(255 * HueToRGB(v1, v2, hue));
//        b = (unsigned char)(255 * HueToRGB(v1, v2, hue - (1.0f / 3)));
//    }
//
//    return RGB(r, g, b);
//}

// helper....
static float HueToRGB(float v1, float v2, float vH) {
    if (vH < 0)
        vH += 1;

    if (vH > 1)
        vH -= 1;

    if ((6 * vH) < 1)
        return (v1 + (v2 - v1) * 6 * vH);

    if ((2 * vH) < 1)
        return v2;

    if ((3 * vH) < 2)
        return (v1 + (v2 - v1) * ((2.0f / 3) - vH) * 6);

    return v1;
}

//
// h 0..360
// s 0..1 (percentage)
// l 0..1 (percentage)
//
ColorRGBA ColorRGBA::FromHSL(float h, float s, float l) {
    ColorRGBA col;

    // clip s/l
    if (s < 0.0f) s = 0.0f;
    else if ( s > 1.0f) s = 1.0f;
    if (l < 0.0f) s = 0.0f;
    else if (l > 1.0f) l = 1.0f;

    if (s == 0)
    {
        col.r = col.g = col.b = l;
    }
    else
    {
        float v1, v2;
        float hue = (float)h / 360.0f;

        v2 = (l < 0.5) ? (l * (1 + s)) : ((l + s) - (l * s));
        v1 = 2 * l - v2;

        col.r = HueToRGB(v1, v2, hue + (1.0f / 3));
        col.g = HueToRGB(v1, v2, hue);
        col.b = HueToRGB(v1, v2, hue - (1.0f / 3));
    }

    return col;
}

ColorRGBA ColorRGBA::FromHSLA(float h, float s, float l, float a) {
    auto col = FromHSL(h,s,l);
    col.a = a;
    return col;
}

ColorRGBA ColorRGBA::FromHexStr(std::string &str) {
    ColorRGBA rgba;
    return rgba;
}

ColorRGBA ColorRGBA::FromRGB(uint8_t red, uint8_t green, uint8_t blue) {
    ColorRGBA col;
    col.r = red / 255.0f;
    col.g = green / 255.0f;
    col.b = blue / 255.0f;
    return col;
}

//
// TODO: Load sublime theme JSON file with colors...
//

void DrawLine(Line &l) {

    move(0,0);
    clrtoeol();
    move(0,0);
    for(int i=0;i<l.Length();i++) {
        addch(l.Buffer().at(i));
    }
}


class SublimeConfigScript {
public:
    typedef enum {
        kNil = 0,
        kColor = 1,
        kNumber = 2,
        kString = 3,    // ??
        kObject = 4,    // ??
        kUser = 5,
    } kValueType;

    struct ScriptValue {
        kValueType vType = kNil;  // not sure
        std::variant<ColorRGBA, float, std::string, void *> data; //col, number, str, ptrUser;

        const ColorRGBA &Color() const { return std::get<ColorRGBA>(data); }
        float Number() const { return std::get<float>(data); }
        const std::string &String() const { return std::get<std::string>(data); }
        const void *UserPtr() const { return std::get<void *>(data); }
    };
public:
    using FunctionDelegate = std::function<ScriptValue(std::vector<ScriptValue> &args)>;
public:

    void RegisterBuiltIn();

    std::pair<bool, ColorRGBA> ExecuteColorScript(const std::string &str);

    std::pair<bool, SublimeConfigScript::ScriptValue> ExecuteWithTokenizer(gnilk::Tokenizer &tokenizer);
    std::pair<bool, SublimeConfigScript::ScriptValue> ExecuteFunction(gnilk::Tokenizer &tokenizer);
    std::pair<bool, SublimeConfigScript::ScriptValue> ParseNumber(gnilk::Tokenizer &tokenizer);
    std::pair<bool, SublimeConfigScript::ScriptValue> ParseWebColor(gnilk::Tokenizer &tokenizer);


    bool ParseNumberArgs(gnilk::Tokenizer &tokenizer, std::vector<float> &args);

    void AddVariable(const std::string &name, ScriptValue value);

    template<typename T>
    void AddValue(const std::string &name, kValueType valueType, T value);
    const ScriptValue &GetValue(const std::string &name);

    void RegisterFunction(const std::string &name, FunctionDelegate function);
    bool HasFunction(const std::string &name) {
        if (functions.find(name) == functions.end()) {
            return false;
        }
        return true;
    }

private:
    ScriptValue ExecuteHSL(std::vector<ScriptValue> &args);
    ScriptValue ExecuteHSLA(std::vector<ScriptValue> &args);


private:
    std::unordered_map<std::string, FunctionDelegate> functions;
    std::unordered_map<std::string, ScriptValue> variables;
};


static std::string colScriptOp = {"( ) % , #"};

void SublimeConfigScript::RegisterBuiltIn() {
    RegisterFunction("hsl",[this](std::vector<SublimeConfigScript::ScriptValue> &args)->SublimeConfigScript::ScriptValue {
        return this->ExecuteHSL(args);
    });
    RegisterFunction("hsla",[this](std::vector<SublimeConfigScript::ScriptValue> &args)->SublimeConfigScript::ScriptValue {
        return this->ExecuteHSL(args);
    });
}


void SublimeConfigScript::AddVariable(const std::string &name, ScriptValue value) {
    printf("Add Variable: %s\n", name.c_str());
    variables[name] = value;
}

template<typename T>
void SublimeConfigScript::AddValue(const std::string &name, kValueType valueType, T value) {
    ScriptValue scriptValue = {.vType = valueType, .data = value};
    variables[name] = scriptValue;
}

const SublimeConfigScript::ScriptValue &SublimeConfigScript::GetValue(const std::string &name) {
    static ScriptValue value = {.vType = kNil};
    if (variables.find(name) == variables.end()) {
        return value;
    }
    return variables[name];
}

std::pair<bool, SublimeConfigScript::ScriptValue> SublimeConfigScript::ParseNumber(gnilk::Tokenizer &tokenizer) {
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

    return {true, {.vType = SublimeConfigScript::kNumber, .data = v}};
}

SublimeConfigScript::ScriptValue SublimeConfigScript::ExecuteHSL(std::vector<ScriptValue> &args) {
    if (args.size() != 3) {
        printf("Err: Parameter mismatch, HSL expects 3 paramters\n");
        printf("  HSLA(H L S)");
        return {};
    }
    for (int i=0;i<3;i++) {
        if (args[i].vType != SublimeConfigScript::kNumber) {
            printf("Err: Parameter %d is not a number!\n", i);
            return {};
        }
    }
    return {.vType = SublimeConfigScript::kColor, .data = {ColorRGBA::FromHSL(args[0].Number(),args[1].Number(),args[2].Number())}};
}

SublimeConfigScript::ScriptValue SublimeConfigScript::ExecuteHSLA(std::vector<ScriptValue> &args) {

    if (args.size() != 4) {
        printf("Err: Parameter mismatch, HSLA expects 4 paramters\n");
        printf("  HSLA(H L S A)");
        return {};
    }
    for (int i=0;i<4;i++) {
        if (args[i].vType != SublimeConfigScript::kNumber) {
            printf("Err: Parameter %d is not a number!\n", i);
            return {};
        }
    }
    return {.vType = SublimeConfigScript::kColor, .data = {ColorRGBA::FromHSLA(args[0].Number(),args[1].Number(),args[2].Number(),args[3].Number())}};
}

// Not that hard...
void SublimeConfigScript::RegisterFunction(const std::string &name, FunctionDelegate function) {
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
std::pair<bool, ColorRGBA> SublimeConfigScript::ExecuteColorScript(const std::string &script) {
    gnilk::Tokenizer tokenizer(script.c_str(), colScriptOp.c_str());
    auto[ok, value] =  ExecuteWithTokenizer(tokenizer);
    if (!ok) {
        return {false, {}};
    }
    if (value.vType != SublimeConfigScript::kColor) {
        return {false, {}};
    }
    return {ok, value.Color()};
}

std::pair<bool, SublimeConfigScript::ScriptValue> SublimeConfigScript::ExecuteWithTokenizer(gnilk::Tokenizer &tokenizer) {
    auto token = tokenizer.Peek();

    bool result = false;
    ScriptValue value = {};

    if (HasFunction(token)) {
        std::tie(result, value) = ExecuteFunction(tokenizer);
    } else if (token == std::string("#")) {
        std::tie(result, value) = ParseWebColor(tokenizer);
    } else if (strutil::isnumber(token)) {
        std::tie(result, value) = ParseNumber(tokenizer);
    } else {
        printf("ERR: '%s' is not a valid token nor function\n", token);
    }
    return {result, value};
}

std::pair<bool, SublimeConfigScript::ScriptValue> SublimeConfigScript::ExecuteFunction(gnilk::Tokenizer &tokenizer) {
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

std::pair<bool, SublimeConfigScript::ScriptValue> SublimeConfigScript::ParseWebColor(gnilk::Tokenizer &tokenizer) {
    tokenizer.Next();   // eat '#'
    std::string token(tokenizer.Next());

    if (token.length() != 6) {
        printf("ERR: Hex color description to short, format is: #RRGGBB (at least 6 values!!)\n");
        return {false, {}};
    }

    std::string strRed(token,0,2);
    std::string strGreen(token,2,2);
    std::string strBlue(token,4,2);

    auto col = ColorRGBA::FromRGB(strutil::hex2dec(strRed.c_str()),
                       strutil::hex2dec(strGreen.c_str()),
                       strutil::hex2dec(strBlue.c_str()));

    return {true, {.vType = SublimeConfigScript::kColor, .data = col}};
}

static void loadSublimeColorFile(const std::string &filename) {
    std::ifstream f(filename);

    SublimeConfigScript scriptEngine;

    json data = json::parse(f);
    auto variables = data["variables"];
    for (auto &col : variables.items()) {
        if (col.value().is_string()) {
            auto value = col.value().get<std::string>();

            printf("  %s:%s\n", col.key().c_str(), value.c_str());

            auto [ok, color] = scriptEngine.ExecuteColorScript(value);

            scriptEngine.AddValue<ColorRGBA>(col.key(), SublimeConfigScript::kColor, color);
        }
    }
    printf("Testing script engine\n");
    auto colValue = scriptEngine.GetValue("blue3").Color();
    printf("col: %f, %f, %f", colValue.R(), colValue.G(), colValue.B());

}

int main(int argc, char **argv) {
    SublimeConfigScript scriptEngine;
    scriptEngine.RegisterBuiltIn();

    scriptEngine.RegisterFunction("func",[](std::vector<SublimeConfigScript::ScriptValue> &args)->SublimeConfigScript::ScriptValue {
        printf("func exec, args: %d\n", (int)args.size());

        return {.vType = SublimeConfigScript::kNil, .data = nullptr };
    });
    auto [ok, col] = scriptEngine.ExecuteColorScript("hsl(210, 15%, 22%)");
    if (!ok) {
        printf("ERRROROROR\n");
        return -1;
    }
    printf("Col: %d, %d, %d\n", col.RedAsInt(), col.GreenAsInt(), col.BlueAsInt());


    //loadSublimeColorFile("tests/colors.sublime.json");
    return 1;


    atexit(Close);
    Open();

    int nColors = 0;
    if (has_colors()) {
        nColors = COLORS;

        auto black = ColorRGBA::FromRGB(0,0,0);
        auto white = ColorRGBA::FromRGB(255,255,255);


        auto pink = ColorRGBA::FromHSL(300, 30/100.0f, 68/100.0f);
        auto blue3 = ColorRGBA::FromHSL(210, 15/100.0f, 22/100.0f);


        // black
        init_color(0, blue3.RedAsInt(1000), blue3.GreenAsInt(1000), blue3.BlueAsInt(1000));
        // white
        init_color(1, white.RedAsInt(1000), white.GreenAsInt(1000), white.BlueAsInt(1000));
//        init_color();
//        init_pair(1, COLOR_GREEN, COLOR_BLACK);
//        init_pair(2, COLOR_BLACK, COLOR_GREEN);
        init_pair(1, 0, 1);

    }


    Line line;
    char buffer[256];
    snprintf(buffer, 256, "line, colors: %d", nColors);
    line.Append(buffer);

    attron(COLOR_PAIR(1));
    DrawLine(line);
    int ch;
    while((ch = getch()) != KEY_F(1)) {
        //
    }
}
