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
#include "Core/LangLineTokenizer.h"
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

void DrawLine2(Line &l, std::vector<gnilk::LangLineTokenizer::Token> &attribs) {

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

class LanguageBase {
public:
    LanguageBase() = default;
    virtual ~LanguageBase() = default;

    // Implement this and setup the tokenizer
    virtual bool Initialize() { return false; }; // You should really implement this...

    const gnilk::LangLineTokenizer &Tokenizer() { return  tokenizer; }
protected:
    gnilk::LangLineTokenizer tokenizer;
};

class CPPLanguage : public LanguageBase {
public:
    CPPLanguage() = default;
    virtual ~CPPLanguage() = default;

    bool Initialize() override;
private:

// declare in-string operators


};

// state: main (and probably a few others)
static const std::string cppTypes = "void int char";
static const std::string cppKeywords = "auto typedef class struct static enum for while if return const";
// Note: Multi char operators must be declared first...
static const std::string cppOperators = "== ++ -- << >> += -= *= /= = + - < > ( , * ) { [ ] } < > ; ' \"";
static const std::string cppLineComment = "//";
// state: main & in_block_comment
static const std::string cppBlockCommentStart = "/* */";
static const std::string cppBlockCommentStop = "*/";
// state: in_string
static const std::string inStringOperators = R"_(\" \\ ")_";
static const std::string inStringPostFixOp = "\"";

//
// Configure the tokenizer for C++
//
bool CPPLanguage::Initialize() {
    auto state = tokenizer.GetOrAddState("main");
    state->SetIdentifiers(gnilk::LangLineTokenizer::kOperator, cppOperators.c_str());
    state->SetIdentifiers(gnilk::LangLineTokenizer::kKeyword, cppKeywords.c_str());
    state->SetIdentifiers(gnilk::LangLineTokenizer::kKnownType, cppTypes.c_str());
    state->SetIdentifiers(gnilk::LangLineTokenizer::kLineComment, cppLineComment.c_str());
    state->SetIdentifiers(gnilk::LangLineTokenizer::kBlockComment, cppBlockCommentStart.c_str());
    state->SetPostFixIdentifiers(cppOperators.c_str());

    state->GetOrAddAction("\"",gnilk::LangLineTokenizer::kAction::kPushState, "in_string");
    state->GetOrAddAction("/*", gnilk::LangLineTokenizer::kAction::kPushState, "in_block_comment");

    auto stateStr = tokenizer.GetOrAddState("in_string");
    stateStr->SetIdentifiers(gnilk::LangLineTokenizer::kFunky, inStringOperators.c_str());
    stateStr->SetPostFixIdentifiers(inStringPostFixOp.c_str());
    stateStr->GetOrAddAction("\"", gnilk::LangLineTokenizer::kAction::kPopState);

    auto stateBlkComment = tokenizer.GetOrAddState("in_block_comment");
    stateBlkComment->SetIdentifiers(gnilk::LangLineTokenizer::kFunky, cppBlockCommentStop.c_str());
    stateBlkComment->GetOrAddAction("*/", gnilk::LangLineTokenizer::kAction::kPopState);


    tokenizer.PushState("main");

    return true;
}


static void testTokenizer() {
    std::string strCode = R"_(const char *str="hello \" world"; number++; /* void main func() */ int anothervar;)_";

    //
    // This can be put in a configuration file...
    // Not as advanced as Sublime (by a long-shot) but good enough for a first try...
    //

    // Each buffer will have this
//    gnilk::LangLineTokenizer tokenizer;
    CPPLanguage cppLanguage;
    if (!cppLanguage.Initialize()) {
        printf("ERR: Configuration error when configuring CPP parser\n");
        exit(1);
    }
    auto tokenizer = cppLanguage.Tokenizer();


    // Each line structure should have this!
    // Basically the 'Token' replaces the LineAttrib structure
    // Classification -> will be used to look up the actual attribute
    // idxOrigStr -> same as LineAttrib.cStart
    //
    std::vector<gnilk::LangLineTokenizer::Token> tokens;
    tokenizer.PrepareTokens(tokens, strCode.c_str());


    printf("%s\n", strCode.c_str());
    for(int i=0;i<strCode.size();i++) {
        printf("%d", i % 10);
    }
    printf("\n");

    for(auto &token : tokens) {
        printf("%d:%d:%s\n", token.idxOrigStr, token.classification,token.string.c_str());
    }
}

int main(int argc, char **argv) {
    testTokenizer();
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
        init_pair(gnilk::LangLineTokenizer::kTokenClass::kRegular, 1, 0); // 0 - default color pair
        init_pair(gnilk::LangLineTokenizer::kTokenClass::kOperator, 2, 0); // 0 - default color pair
        init_pair(gnilk::LangLineTokenizer::kTokenClass::kKeyword, 3, 0); // 0 - default color pair
        init_pair(gnilk::LangLineTokenizer::kTokenClass::kKnownType, 4, 0); // 0 - default color pair

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
    std::vector<gnilk::LangLineTokenizer::Token> tokens;
    tokenizer.PrepareTokens(tokens, strCode.c_str());

    DrawLine2(line, tokens);


    int ch;
    while((ch = getch()) != KEY_F(1)) {
        //
    }
    */
}
