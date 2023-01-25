//
// Created by gnilk on 25.01.23.
//

#ifndef EDITOR_SUBLIMECONFIGCOLORSCRIPT_H
#define EDITOR_SUBLIMECONFIGCOLORSCRIPT_H

#include "SublimeConfigScriptEngine.h"

class SublimeConfigColorScript : public SublimeConfigScriptEngine {
public:
    std::pair<bool, ColorRGBA> ExecuteColorScript(const std::string &str);
    void RegisterBuiltIn() override;
private:
    // These are actually only used in the color script..
    ScriptValue ExecuteHSL(std::vector<ScriptValue> &args);
    ScriptValue ExecuteHSLA(std::vector<ScriptValue> &args);
    ScriptValue ExecuteVAR(std::vector<ScriptValue> &args);
    ScriptValue ExecuteColor(std::vector<ScriptValue> &args);
    ScriptValue ExecuteAlpha(std::vector<ScriptValue> &args);
    ScriptValue ExecuteRGB(std::vector<ScriptValue> &args);
    ScriptValue ExecuteRGBA(std::vector<ScriptValue> &args);

};

#endif //EDITOR_SUBLIMECONFIGCOLORSCRIPT_H
