//
// Created by gnilk on 15.05.23.
//

#include "Keyboard.h"
#include <unordered_map>

using namespace gedit;

static std::unordered_map<Keyboard::kKeyCode, std::string> keyCodeToStrMap = {
        {Keyboard::kKeyCode_None,          "kKeyCode_None"},
        {Keyboard::kKeyCode_Return,        "kKeyCode_Return"},
        {Keyboard::kKeyCode_Escape,        "kKeyCode_Escape"},
        {Keyboard::kKeyCode_Backspace,     "kKeyCode_Backspace"},
        {Keyboard::kKeyCode_Tab,           "kKeyCode_Tab"},
        {Keyboard::kKeyCode_Space,         "kKeyCode_Space"},
        {Keyboard::kKeyCode_F1,            "kKeyCode_F1"},
        {Keyboard::kKeyCode_F2,            "kKeyCode_F2"},
        {Keyboard::kKeyCode_F3,            "kKeyCode_F3"},
        {Keyboard::kKeyCode_F4,            "kKeyCode_F4"},
        {Keyboard::kKeyCode_F5,            "kKeyCode_F5"},
        {Keyboard::kKeyCode_F6,            "kKeyCode_F6"},
        {Keyboard::kKeyCode_F7,            "kKeyCode_F7"},
        {Keyboard::kKeyCode_F8,            "kKeyCode_F8"},
        {Keyboard::kKeyCode_F9,            "kKeyCode_F9"},
        {Keyboard::kKeyCode_F10,           "kKeyCode_F10"},
        {Keyboard::kKeyCode_F11,           "kKeyCode_F11"},
        {Keyboard::kKeyCode_F12,           "kKeyCode_F12"},
        {Keyboard::kKeyCode_PrintScreen,   "kKeyCode_PrintScreen"},
        {Keyboard::kKeyCode_ScrollLock,    "kKeyCode_ScrollLock"},
        {Keyboard::kKeyCode_Pause,         "kKeyCode_Pause"},
        {Keyboard::kKeyCode_Insert,        "kKeyCode_Insert"},
        {Keyboard::kKeyCode_Home,          "kKeyCode_Home"},
        {Keyboard::kKeyCode_PageUp,        "kKeyCode_PageUp"},
        {Keyboard::kKeyCode_DeleteForward, "kKeyCode_DeleteForward"},
        {Keyboard::kKeyCode_End,           "kKeyCode_End"},
        {Keyboard::kKeyCode_PageDown,      "kKeyCode_PageDown"},
        {Keyboard::kKeyCode_LeftArrow,     "kKeyCode_LeftArrow"},
        {Keyboard::kKeyCode_RightArrow,    "kKeyCode_RightArrow"},
        {Keyboard::kKeyCode_DownArrow,     "kKeyCode_DownArrow"},
        {Keyboard::kKeyCode_UpArrow,       "kKeyCode_UpArrow"},
        {Keyboard::kKeyCode_NumLock,       "kKeyCode_NumLock"},
};

static std::unordered_map<std::string, Keyboard::kKeyCode> strToKeyCodeMap = {
        {"KeyCode_None",          Keyboard::kKeyCode_None},
        {"KeyCode_Return",        Keyboard::kKeyCode_Return},
        {"KeyCode_Escape",        Keyboard::kKeyCode_Escape},
        {"KeyCode_Backspace",     Keyboard::kKeyCode_Backspace},
        {"KeyCode_Tab",           Keyboard::kKeyCode_Tab},
        {"KeyCode_Space",         Keyboard::kKeyCode_Space},
        {"KeyCode_F1",            Keyboard::kKeyCode_F1},
        {"KeyCode_F2",            Keyboard::kKeyCode_F2},
        {"KeyCode_F3",            Keyboard::kKeyCode_F3},
        {"KeyCode_F4",            Keyboard::kKeyCode_F4},
        {"KeyCode_F5",            Keyboard::kKeyCode_F5},
        {"KeyCode_F6",            Keyboard::kKeyCode_F6},
        {"KeyCode_F7",            Keyboard::kKeyCode_F7},
        {"KeyCode_F8",            Keyboard::kKeyCode_F8},
        {"KeyCode_F9",            Keyboard::kKeyCode_F9},
        {"KeyCode_F10",           Keyboard::kKeyCode_F10},
        {"KeyCode_F11",           Keyboard::kKeyCode_F11},
        {"KeyCode_F12",           Keyboard::kKeyCode_F12},
        {"KeyCode_PrintScreen",   Keyboard::kKeyCode_PrintScreen},
        {"KeyCode_ScrollLock",    Keyboard::kKeyCode_ScrollLock},
        {"KeyCode_Pause",         Keyboard::kKeyCode_Pause},
        {"KeyCode_Insert",        Keyboard::kKeyCode_Insert},
        {"KeyCode_Home",          Keyboard::kKeyCode_Home},
        {"KeyCode_PageUp",        Keyboard::kKeyCode_PageUp},
        {"KeyCode_DeleteForward", Keyboard::kKeyCode_DeleteForward},
        {"KeyCode_End",           Keyboard::kKeyCode_End},
        {"KeyCode_PageDown",      Keyboard::kKeyCode_PageDown},
        {"KeyCode_LeftArrow",     Keyboard::kKeyCode_LeftArrow},
        {"KeyCode_RightArrow",    Keyboard::kKeyCode_RightArrow},
        {"KeyCode_DownArrow",     Keyboard::kKeyCode_DownArrow},
        {"KeyCode_UpArrow",       Keyboard::kKeyCode_UpArrow},
        {"KeyCode_NumLock",       Keyboard::kKeyCode_NumLock},
};

static std::unordered_map<std::string, int> strToModifierBitMaskMap = {
        {"Shift",                  Keyboard::kMod_LeftShift | Keyboard::kMod_RightShift},
        {"Ctrl",                   Keyboard::kMod_LeftCtrl | Keyboard::kMod_RightCtrl},
        {"Alt",                    Keyboard::kMod_LeftAlt | Keyboard::kMod_RightAlt},
        {"Cmd",                    Keyboard::kMod_LeftCommand | Keyboard::kMod_RightCommand},
        // Alias
        {"Control",                Keyboard::kMod_LeftCtrl | Keyboard::kMod_RightCtrl},
        {"Alternate",              Keyboard::kMod_LeftAlt | Keyboard::kMod_RightAlt},
        {"Command",                Keyboard::kMod_LeftCommand | Keyboard::kMod_RightCommand},
        // Left/Right distinction
        {"LeftShift",              Keyboard::kMod_LeftShift},
        {"RightShift",             Keyboard::kMod_RightShift},
        {"LeftAlt",                Keyboard::kMod_LeftAlt},
        {"RightAlt",               Keyboard::kMod_RightAlt},
        {"LeftCmd",                Keyboard::kMod_LeftCommand},
        {"RightCmd",               Keyboard::kMod_RightCommand},
        // Alias
        {"LeftAlternate",          Keyboard::kMod_LeftAlt},
        {"RightAlternate",         Keyboard::kMod_RightAlt},
        {"LeftCommand",            Keyboard::kMod_LeftCommand},
        {"RightCommand",           Keyboard::kMod_RightCommand},
        // Using KeyCode prefix
        {"KeyCode_Shift",          Keyboard::kMod_LeftShift | Keyboard::kMod_RightShift},
        {"KeyCode_Ctrl",           Keyboard::kMod_LeftCtrl | Keyboard::kMod_RightCtrl},
        {"KeyCode_Alt",            Keyboard::kMod_LeftAlt | Keyboard::kMod_RightAlt},
        {"KeyCode_Cmd",            Keyboard::kMod_LeftCommand | Keyboard::kMod_RightCommand},
        // Alias
        {"KeyCode_Control",        Keyboard::kMod_LeftCtrl | Keyboard::kMod_RightCtrl},
        {"KeyCode_Alternate",      Keyboard::kMod_LeftAlt | Keyboard::kMod_RightAlt},
        {"KeyCode_Command",        Keyboard::kMod_LeftCommand | Keyboard::kMod_RightCommand},
        // Left/Right distinction
        {"KeyCode_LeftShift",      Keyboard::kMod_LeftShift},
        {"KeyCode_RightShift",     Keyboard::kMod_RightShift},
        {"KeyCode_LeftAlt",        Keyboard::kMod_LeftAlt},
        {"KeyCode_RightAlt",       Keyboard::kMod_RightAlt},
        {"KeyCode_LeftCmd",        Keyboard::kMod_LeftCommand},
        {"KeyCode_RightCmd",       Keyboard::kMod_RightCommand},
        // Alias
        {"KeyCode_LeftAlternate",  Keyboard::kMod_LeftAlt},
        {"KeyCode_RightAlternate", Keyboard::kMod_RightAlt},
        {"KeyCode_LeftCommand",    Keyboard::kMod_LeftCommand},
        {"KeyCode_RightCommand",   Keyboard::kMod_RightCommand},
};



const std::string &Keyboard::KeyCodeName(const Keyboard::kKeyCode keyCode) {
    return keyCodeToStrMap[keyCode];
}

std::optional<Keyboard::kKeyCode> Keyboard::NameToKeyCode(const std::string &name) {
    if (!IsNameKeyCode(name)) {
        return {};
    }
    return {strToKeyCodeMap.at(name)};
}

bool Keyboard::IsNameKeyCode(const std::string &name) {
    if (strToKeyCodeMap.find(name) != strToKeyCodeMap.end()) {
        return true;
    }
    return false;
}

bool Keyboard::IsNameModifierMask(const std::string &modiferName) {
    if (strToModifierBitMaskMap.find(modiferName) != strToModifierBitMaskMap.end()) {
        return true;
    }
    return false;
}

int Keyboard::ModifierMaskFromString(const std::string &strModifiers) {
    if (!IsNameModifierMask(strModifiers)) {
        return 0;
    }
    return strToModifierBitMaskMap.at(strModifiers);
}
