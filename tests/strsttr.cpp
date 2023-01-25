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
#include "Core/Tokenizer.h"
#include "Core/ColorRGBA.h"
#include "Core/Sublime/SublimeConfigScriptEngine.h"


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






static void loadSublimeColorFile(const std::string &filename) {
    std::ifstream f(filename);

    SublimeConfigScriptEngine scriptEngine;
    scriptEngine.RegisterBuiltIn();

    json data = json::parse(f);
    auto variables = data["variables"];
    for (auto &col : variables.items()) {
        if (col.value().is_string()) {
            auto value = col.value().get<std::string>();

            printf("  %s:%s\n", col.key().c_str(), value.c_str());

            auto [ok, color] = scriptEngine.ExecuteColorScript(value);

            scriptEngine.AddValue<ColorRGBA>(col.key(), SublimeConfigScriptEngine::kColor, color);
        }
    }
    printf("Testing script engine\n");
    auto colValue = scriptEngine.GetValue("blue3").Color();
    printf("col: %f, %f, %f", colValue.R(), colValue.G(), colValue.B());
}

static int testScriptEngine() {
    SublimeConfigScriptEngine scriptEngine;
    scriptEngine.RegisterBuiltIn();

    scriptEngine.RegisterFunction("func",[](std::vector<SublimeConfigScriptEngine::ScriptValue> &args)->SublimeConfigScriptEngine::ScriptValue {
        printf("func exec, args: %d\n", (int)args.size());
        return {.vType = SublimeConfigScriptEngine::kNil, .data = nullptr };
    });

    scriptEngine.RegisterFunction("add",[](std::vector<SublimeConfigScriptEngine::ScriptValue> &args)->SublimeConfigScriptEngine::ScriptValue {
        printf("add exec, args: %d\n", (int)args.size());
        float v = args[0].Number() + args[1].Number();

        return {.vType = SublimeConfigScriptEngine::kNumber, .data = v };
    });
    scriptEngine.RegisterFunction("sq",[](std::vector<SublimeConfigScriptEngine::ScriptValue> &args)->SublimeConfigScriptEngine::ScriptValue {
        printf("sq exec, args: %d\n", (int)args.size());
        float v = args[0].Number() * args[0].Number();

        return {.vType = SublimeConfigScriptEngine::kNumber, .data = v };
    });

    scriptEngine.AddVariable("myVar", {.vType = SublimeConfigScriptEngine::kNumber, .data = 2.0f});

    auto [ok, v] = scriptEngine.ExecuteScript("hsla(210, 13%, 40%, 0.7)");
//    auto [ok, v] = scriptEngine.ExecuteScript("add(2, sq(var(myVar)))");

    if (!ok) {
        printf("ERRROROROR\n");
        return -1;
    }
    printf("v type: %d\n", v.vType);
    if (v.IsNumber()) {
        printf("Number: %f\n", v.Number());
        return -1;
    } else if (v.IsColor()) {
        printf("col: %f, %f, %f", v.Color().R(), v.Color().G(), v.Color().B());
    }
    return 1;
}


int main(int argc, char **argv) {

    // return testScriptEngine();

    // This can now load the default sublime color configuration...
    loadSublimeColorFile("tests/colors.sublime.json");
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
