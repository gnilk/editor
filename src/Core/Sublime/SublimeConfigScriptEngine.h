//
// Created by gnilk on 25.01.23.
//

#ifndef EDITOR_SUBLIMECONFIGSCRIPTENGINE_H
#define EDITOR_SUBLIMECONFIGSCRIPTENGINE_H

#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <variant>

#include "Core/ColorRGBA.h"
#include "Core/Tokenizer.h"

class SublimeConfigScriptEngine {
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


        bool IsValid() const { return (vType != kNil); }
        bool IsNumber() const { return (vType == kNumber); }
        bool IsColor() const { return (vType == kColor); }
        bool IsString() const { return (vType == kString); }


    };
    static const ScriptValue invalidScriptValue;

public:
    using FunctionDelegate = std::function<ScriptValue(std::vector<ScriptValue> &args)>;
public:

    virtual void RegisterBuiltIn();
    std::pair<bool, ScriptValue> ExecuteScript(const std::string &str);

    std::pair<bool, SublimeConfigScriptEngine::ScriptValue> ExecuteWithTokenizer(gnilk::Tokenizer &tokenizer);
    std::pair<bool, SublimeConfigScriptEngine::ScriptValue> ExecuteFunction(gnilk::Tokenizer &tokenizer);
    std::pair<bool, SublimeConfigScriptEngine::ScriptValue> ParseInteger(gnilk::Tokenizer &tokenizer);
    std::pair<bool, SublimeConfigScriptEngine::ScriptValue> ParseDouble(gnilk::Tokenizer &tokenizer);
    std::pair<bool, SublimeConfigScriptEngine::ScriptValue> ParseWebColor(gnilk::Tokenizer &tokenizer);


    bool ParseNumberArgs(gnilk::Tokenizer &tokenizer, std::vector<float> &args);

    void AddVariable(const std::string &name, ScriptValue value);
    const ScriptValue GetVariable(const std::string &name) const;
    // Check if a variable is present...
    bool HasVariable(const std::string &name) const {
        if (variables.find(name) == variables.end()) {
            return false;
        }
        return true;
    }

    // This add's a variable from a ScriptValueType - short for AddVariable when you have a valid type...
    template<typename T>
    void AddVarFromValue(const std::string &name, kValueType valueType, T value) {
        ScriptValue scriptValue = {.vType = valueType, .data = value};
        variables[name] = scriptValue;
    }

    void RegisterFunction(const std::string &name, FunctionDelegate function);
    bool HasFunction(const std::string &name) {
        if (functions.find(name) == functions.end()) {
            return false;
        }
        return true;
    }
private:
    ScriptValue ExecuteVAR(std::vector<ScriptValue> &args);


private:
    std::unordered_map<std::string, FunctionDelegate> functions;
    std::unordered_map<std::string, ScriptValue> variables;
};


#endif //EDITOR_SUBLIMECONFIGSCRIPTENGINE_H
