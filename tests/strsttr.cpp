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

#include "Core/Language/LangLineTokenizer.h"
#include "Core/Language/CPP/CPPLanguage.h"
#include "Core/Language/LangToken.h"
#include "Core/Config/Config.h"
#include "Core/Buffer.h"


using json = nlohmann::json;

bool Screen_Open() {
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
        attr_t attrib;
        auto itAttrib = attribs.begin();
        //int cNext = attribs[0].cStart;
        for (int i = 0; i < l.Length(); i++) {

            if (i >= itAttrib->cStart) {
                // FIXME: Convert - must be done in driver...
                attrib = COLOR_PAIR(itAttrib->idxColorPair);
                itAttrib++;
                if (itAttrib == attribs.end()) {
                    --itAttrib;
                }
            }
            attrset(attrib);
            addch(l.Buffer().at(i));
        }
    }
    attrset(A_NORMAL);
}

void DrawLine2(Line &l, std::vector<gnilk::LangToken> &attribs) {

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
        attr_t attrib;
        auto itAttrib = attribs.begin();
        //int cNext = attribs[0].cStart;
        for (int i = 0; i < l.Length(); i++) {

            if (i >= itAttrib->idxOrigStr) {
                // FIXME: Convert - must be done in driver...
                attrib = COLOR_PAIR(itAttrib->classification);
                itAttrib++;
                if (itAttrib == attribs.end()) {
                    --itAttrib;
                }
            }
            attrset(attrib);
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

    // Push colors to color configuration
    auto colorConfig = Config::Instance().ColorConfiguration();

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


static void testTokenizer() {
    std::string strCode = R"_(const char *str="hello \" world"; number++; /* void main func()*/ int anothervar;)_";
    std::vector<std::string> codeLines={
            {"i"},
            {"in"},
            {"int"},
            {"int m"},
            {"int ma"},
            {"int mai"},
            {"int main"},
            {"int main("},
    };


    // Each buffer will have this
    CPPLanguage cppLanguage;
    if (!cppLanguage.Initialize()) {
        printf("ERR: Configuration error when configuring CPP parser\n");
        exit(1);
    }
    auto tokenizer = cppLanguage.Tokenizer();


    for(int i=0;i<8;i++) {
        printf("-----[: %s\n", codeLines[i].c_str());

        std::vector<gnilk::LangToken> tokens;
        tokenizer.ParseLine(tokens, codeLines[i].c_str());
        for (auto &t: tokens) {
            printf("%d:%d - %s\n", t.idxOrigStr, static_cast<int>(t.classification), t.string.c_str());
        }
    }
    return;



//    auto colorConfig = Config::Instance().ColorConfiguration();
//    for(auto &token : tokens) {
//        auto strTokenClass = gnilk::LanguageTokenClassToString(token.classification);
//        if (!colorConfig.HasColor(strTokenClass)) {
//            printf("Missing color with name: %s\n", strTokenClass.c_str());
//            exit(1);
//        }
//        printf("%d:%s (%d):%s\n", token.idxOrigStr, strTokenClass.c_str(), token.classification,token.string.c_str());
//    }
}



static int colorCounter = 0;
static void Screen_RegisterColor(int appIndex, const ColorRGBA &foreground, const ColorRGBA &background) {

    int currentColor = colorCounter;

    init_color(colorCounter++, background.R() * 1000, background.G() * 1000, background.B() * 1000);
    init_color(colorCounter++, foreground.R() * 1000, foreground.G() * 1000, foreground.B() * 1000);

    init_pair(appIndex,  currentColor + 1, currentColor);
}


int main(int argc, char **argv) {

    testTokenizer();

    return -1;

    Config::Instance().LoadConfig("tests/config.yml");

    atexit(Close);
    Screen_Open();

    // FIXME: This must be moved into "post-processing" of config loading...
    //        Or part of 'Screen::Open' call
    auto colorConfig = Config::Instance().ColorConfiguration();
    for(int i=0;gnilk::IsLanguageTokenClass(i);i++) {
        auto langClass = gnilk::LanguageTokenClassToString(static_cast<gnilk::kLanguageTokenClass>(i));
        if (!colorConfig.HasColor(langClass)) {
            printf("\nErr, missing color configuration for: %s\n", langClass.c_str());
            return -1;
        }
        Screen_RegisterColor(i, colorConfig.GetColor(langClass), colorConfig.GetColor("background"));
    }


    std::string strCode = R"_(const char *str="hello \" world"; number++; /* void main func()*/ int anothervar;)_";

    Line line;
    Buffer buffer;
    line.Append(strCode);


    CPPLanguage cppLanguage;
    if (!cppLanguage.Initialize()) {
        printf("ERR: Configuration error when configuring CPP parser\n");
        exit(1);
    }
    auto tokenizer = cppLanguage.Tokenizer();

    std::vector<gnilk::LangToken> tokens;
    tokenizer.ParseLine(tokens, strCode.c_str());

    for(auto &t : tokens) {
        printf("%d:%d, %s\n", t.idxOrigStr, t.classification, t.string.c_str());
    }

    DrawLine2(line, tokens);

    int ch;
    while((ch = getch()) != KEY_F(1)) {
        //
    }

    //testTokenizer();
    return -1;
    /*
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

    // Regular
    auto colFG = colorConfig.GetColor("foreground");
    auto colBG = colorConfig.GetColor("background");

    // Operator
    auto colOperatorFG = colorConfig.GetColor("operators");
    auto colKeywordFG = colorConfig.GetColor("keywords");
    auto colKnownTypeFG = colorConfig.GetColor("types");



    atexit(Close);
    Open();

    int nColors = 0;
    if (has_colors()) {
        nColors = COLORS;

        auto pink = ColorRGBA::FromHSL(300, 30/100.0f, 68/100.0f);
        auto blue3 = ColorRGBA::FromHSL(210, 15/100.0f, 22/100.0f);

        init_color(0, colBG.RedAsInt(1000), colBG.GreenAsInt(1000), colBG.BlueAsInt(1000));
        init_color(1, colFG.RedAsInt(1000), colFG.GreenAsInt(1000), colFG.BlueAsInt(1000));
        init_color(2, colOperatorFG.RedAsInt(1000), colOperatorFG.GreenAsInt(1000), colOperatorFG.BlueAsInt(1000) );
        init_color(3, colKeywordFG.RedAsInt(1000), colKeywordFG.GreenAsInt(1000), colKeywordFG.BlueAsInt(1000) );
        init_color(4, colKnownTypeFG.RedAsInt(1000), colKnownTypeFG.GreenAsInt(1000), colKnownTypeFG.BlueAsInt(1000) );


        // init_pair is <pair>,<fg>,<bg>
        init_pair(0, 1, 0); // 0 - default color pair
        // Pairing it up like this allows us to use classification directly...
        init_pair(gnilk::LangLineTokenizer::kLanguageTokenClass::kRegular, 1, 0); // 0 - default color pair
        init_pair(gnilk::LangLineTokenizer::kLanguageTokenClass::kOperator, 2, 0); // 0 - default color pair
        init_pair(gnilk::LangLineTokenizer::kLanguageTokenClass::kKeyword, 3, 0); // 0 - default color pair
        init_pair(gnilk::LangLineTokenizer::kLanguageTokenClass::kKnownType, 4, 0); // 0 - default color pair

    }


//    Line line;
//    char buffer[256];
//    snprintf(buffer, 256, "this is a very color full line of stuff I want to see");
//    line.Append(buffer);
//
//    std::vector<LineAttrib> lineAttribs;
//    lineAttribs.push_back({.cStart = 0,  .idxColorPair = 0});
//    lineAttribs.push_back({.cStart = 5,  .idxColorPair = 1});
//    lineAttribs.push_back({.cStart = 10, .idxColorPair = 0});
//    lineAttribs.push_back({.cStart = 15, .idxColorPair = 1});
//
//    attron(COLOR_PAIR(1));
//    DrawLine(line, lineAttribs);

    // Each buffer will have a reference to the language tokenizer
    //gnilk::LangLineTokenizer tokenizer(cppOperators.c_str(), cppKeywords.c_str(), cppTypes.c_str());

//    std::string strCode = "main func {{{}}} [[]]] static typedef int void char";
    std::string strCode = "void main(int argc, char *argv[]) typedef struct apa {";
    Line line;
    line.Append(strCode);

    // Each line will have this
    std::vector<gnilk::LangLineTokenizer::LangToken> tokens;
    tokenizer.PrepareTokens(tokens, strCode.c_str());

    DrawLine2(line, tokens);


    int ch;
    while((ch = getch()) != KEY_F(1)) {
        //
    }
    */
}
