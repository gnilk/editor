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
#include "Core/Sublime/SublimeConfigColorScript.h"


class ColorConfig {
public:
    ColorConfig();
    void SetDefaults() noexcept;
    void SetColor(const std::string &name, ColorRGBA color);

    bool HasColor(const std::string &name) const {
        return (colors.find(name) != colors.end());
    }
    const ColorRGBA GetColor(const std::string &name) const {
        if (!HasColor(name)) {
            return {};
        }
        auto it = colors.find(name);
        return it->second;
    }
    const ColorRGBA operator[](const std::string &name) const {
        return GetColor(name);
    }

private:
    std::unordered_map<std::string, ColorRGBA> colors;
};
ColorConfig::ColorConfig() {
    SetDefaults();
}

void ColorConfig::SetDefaults() noexcept {

}

void ColorConfig::SetColor(const std::string &name, ColorRGBA color) {
    colors[name] = color;
}

static ColorConfig colorConfig;


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
    int idxColorPair;
};


//
// TODO: Load sublime theme JSON file with colors...
//

void DrawLine(Line &l, std::vector<LineAttrib> &attribs) {

    move(0,0);
    clrtoeol();
    move(0,0);

    int idxColorPair = 0;
    // If no attribs - just dump it out...
    if (attribs.size() == 0) {
        for (int i = 0; i < l.Length(); i++) {
            addch(l.Buffer().at(i));
        }
    } else {
        int idxAttrib = 0;
        int cNext = attribs[0].cStart;
        for (int i = 0; i < l.Length(); i++) {
            if ((i >= cNext) && (cNext >= 0)){
                auto attrib = attribs[idxAttrib];
                attr_t newAttr;
                newAttr = COLOR_PAIR(attrib.idxColorPair);
                attrset(newAttr);

                idxAttrib++;
                if (idxAttrib < attribs.size()) {
                    cNext = attribs[idxAttrib].cStart;
                } else {
                    // Here we can just continue to dump the line data!
                    // This is usefull for comments and stuff...
                    cNext = -1;
                }
            }
            addch(l.Buffer().at(i));
        }
    }
    attrset(A_NORMAL);
}

static void loadSublimeColorFile(const std::string &filename, SublimeConfigColorScript &scriptEngine) {
    std::ifstream f(filename);

//    SublimeConfigScriptEngine scriptEngine;
//    scriptEngine.RegisterBuiltIn();

    json data = json::parse(f);
    auto variables = data["variables"];
    for (auto &col : variables.items()) {
        if (col.value().is_string()) {
            auto value = col.value().get<std::string>();

            printf("  %s:%s\n", col.key().c_str(), value.c_str());

            auto [ok, color] = scriptEngine.ExecuteColorScript(value);

            scriptEngine.AddVarFromValue<ColorRGBA>(col.key(), SublimeConfigScriptEngine::kColor, color);
        }
    }
    auto globals = data["globals"];
    for(auto &col : globals.items()) {
        if (col.value().is_string()) {
            auto value = col.value().get<std::string>();

            auto [ok, scriptValue] = scriptEngine.ExecuteScript(value);
            if (ok && scriptValue.IsColor()) {
                colorConfig.SetColor(col.key(), scriptValue.Color());
            } else {
                printf("  Value for '%s' is not color, constants not supported - skipping\n", col.key().c_str());
            }
        }
    }


    printf("Testing script engine\n");
    auto colValue = scriptEngine.GetVariable("blue3").Color();
    printf("col: %f, %f, %f", colValue.R(), colValue.G(), colValue.B());
}

static int testScriptEngine(SublimeConfigScriptEngine &scriptEngine) {

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

    printf("Execute script\n");
    auto [ok, v] = scriptEngine.ExecuteScript("add(var(myVar) 2)");

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

void testAttribLogic() {
    const std::string str =  {"this is a very color full line of stuff I want to see"};

    std::vector<LineAttrib> attribs;
    attribs.push_back({.cStart = 0, .idxColorPair = 0});
    attribs.push_back({.cStart = 5, .idxColorPair = 1});
    attribs.push_back({.cStart = 10,.idxColorPair = 2});
    attribs.push_back({.cStart = 15,.idxColorPair = 3});

    int idxColor = 0;
    int idxAttrib = 0;
    int cNext = attribs[0].cStart;
    printf("%s\n", str.c_str());
    for (int i = 0; i < str.size(); i++) {
        if ((i >= cNext) && (cNext>=0)){
            auto attrib = attribs[idxAttrib];
            idxColor = attrib.idxColorPair;
            printf("\nNew Attrib: %d:%d\n", idxAttrib, idxColor);
            idxAttrib++;
            if (idxAttrib < attribs.size()) {
                cNext = attribs[idxAttrib].cStart;
            } else {
                cNext = -1;
            }
        }
        printf("%d", idxColor);
    }
    printf("\n");

}


int main(int argc, char **argv) {
//    testAttribLogic();
//    return -1;

    SublimeConfigColorScript scriptEngine;
    scriptEngine.RegisterBuiltIn();

//    printf("Execute and set 'black'\n");
//    auto [ok, color] = scriptEngine.ExecuteScript("hsl(0, 0%, 0%)");
//    if (ok) {
//        scriptEngine.AddVariable("black", color);
//    } else {
//        return -1;
//    }
//    printf("Execute 'shadow'\n");
//
//    auto [ok2, color2] = scriptEngine.ExecuteScript("color(var(black) alpha(0.25))");
//    if (!ok2) {
//        printf("Error in shadow\n");
//        return -1;
//    }
//    return 0;


    loadSublimeColorFile("tests/colors.sublime.json", scriptEngine);

    auto colFG = colorConfig.GetColor("foreground");
    auto colBG = colorConfig.GetColor("background");
    auto colOther = colorConfig.GetColor("accent");

    atexit(Close);
    Open();

    int nColors = 0;
    if (has_colors()) {
        nColors = COLORS;

        auto black = ColorRGBA::FromRGB(0,0,0);
        auto white = ColorRGBA::FromRGB(255,255,255);


        auto pink = ColorRGBA::FromHSL(300, 30/100.0f, 68/100.0f);
        auto blue3 = ColorRGBA::FromHSL(210, 15/100.0f, 22/100.0f);

        init_color(0, colBG.RedAsInt(1000), colBG.GreenAsInt(1000), colBG.BlueAsInt(1000));
        init_color(1, colFG.RedAsInt(1000), colFG.GreenAsInt(1000), colFG.BlueAsInt(1000));

        init_color(2, colOther.RedAsInt(1000), colOther.GreenAsInt(1000), colOther.BlueAsInt(1000));

        // init_pair is <pair>,<fg>,<bg>
        init_pair(0, 1, 0); // 0 - default color pair
        init_pair(1, 2, 0);

    }


    Line line;
    char buffer[256];                   /* 012345678901234567890123456   */
    snprintf(buffer, 256, "this is a very color full line of stuff I want to see");
    line.Append(buffer);

    std::vector<LineAttrib> lineAttribs;
    lineAttribs.push_back({.cStart = 0,  .idxColorPair = 0});
    lineAttribs.push_back({.cStart = 5,  .idxColorPair = 1});
    lineAttribs.push_back({.cStart = 10, .idxColorPair = 0});
    lineAttribs.push_back({.cStart = 15, .idxColorPair = 1});

    attron(COLOR_PAIR(1));
    DrawLine(line, lineAttribs);
    int ch;
    while((ch = getch()) != KEY_F(1)) {
        //
    }
}
