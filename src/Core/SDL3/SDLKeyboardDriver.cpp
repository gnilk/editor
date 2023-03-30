//
// Created by gnilk on 29.03.23.
//
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_scancode.h>
#include <map>
#include <unordered_map>
#include <string>
#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>

#include "Core/KeyCodes.h"
#include "Core/KeyPress.h"
#include "SDLKeyboardDriver.h"


using namespace gedit;

static int createTranslationTable();


bool SDLKeyboardDriver::Initialize() {
    createTranslationTable();
    return true;
}
KeyPress SDLKeyboardDriver::GetKeyPress() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EventType::SDL_EVENT_QUIT) {
            SDL_Quit();
            exit(0);
        }
        if (event.type == SDL_EventType::SDL_EVENT_KEY_DOWN) {
            return TranslateSDLEvent(event.key);
        }
    }
    SDL_Delay(1000/60);

    return {};
}

static std::map<SDL_Keycode, Keyboard::kKeyCode> sdlToKeyCodes {
        {SDLK_SPACE, Keyboard::kKeyCode_Space},
        {SDLK_RETURN, Keyboard::kKeyCode_Return},
        {SDLK_ESCAPE, Keyboard::kKeyCode_Escape},
        {SDLK_UP, Keyboard::kKeyCode_UpArrow},
        {SDLK_DOWN, Keyboard::kKeyCode_DownArrow},
        {SDLK_LEFT, Keyboard::kKeyCode_LeftArrow},
        {SDLK_RIGHT, Keyboard::kKeyCode_RightArrow},
        {SDLK_HOME, Keyboard::kKeyCode_Home},
        {SDLK_END, Keyboard::kKeyCode_End},
        {SDLK_PAGEUP, Keyboard::kKeyCode_PageUp},
        {SDLK_PAGEDOWN, Keyboard::kKeyCode_PageDown},
        {SDLK_INSERT, Keyboard::kKeyCode_Insert},
        {SDLK_DELETE, Keyboard::kKeyCode_DeleteForward},
        {SDLK_BACKSPACE, Keyboard::kKeyCode_Backspace},
        {SDLK_F1, Keyboard::kKeyCode_F1},
        {SDLK_F2, Keyboard::kKeyCode_F2},
        {SDLK_F3, Keyboard::kKeyCode_F3},
        {SDLK_F4, Keyboard::kKeyCode_F4},
        {SDLK_F5, Keyboard::kKeyCode_F5},
        {SDLK_F6, Keyboard::kKeyCode_F6},
        {SDLK_F7, Keyboard::kKeyCode_F7},
        {SDLK_F8, Keyboard::kKeyCode_F8},
        {SDLK_F9, Keyboard::kKeyCode_F9},
        {SDLK_F10, Keyboard::kKeyCode_F10},
        {SDLK_F11, Keyboard::kKeyCode_F11},
        {SDLK_F12, Keyboard::kKeyCode_F12},
        {SDLK_PRINTSCREEN, Keyboard::kKeyCode_PrintScreen},
        {SDLK_SCROLLLOCK, Keyboard::kKeyCode_ScrollLock},
        {SDLK_PAUSE, Keyboard::kKeyCode_Pause},
        {SDLK_TAB, Keyboard::kKeyCode_Tab},
        {SDLK_KP_ENTER, Keyboard::kKeyCode_Return},
};

static std::unordered_map<int, char> asciiTranslationMap;
static std::unordered_map<int, char> asciiShiftTranslationMap;

//
// This is based on inspection...
// Not exactly sure which 'driver' I used to do this...
//
static int createTranslationTable() {
    int scanCode = 0x04;
    for(int i='a';i<='z';i++) {
        asciiTranslationMap[scanCode] = i;
        asciiShiftTranslationMap[scanCode] = std::toupper(i);
        scanCode++;
    }

    static std::string numbers="1234567890";
    static std::string numbersShift="!@#$%^&*()";
    for(int i=0;i<numbers.size();i++) {
        asciiTranslationMap[scanCode] = numbers[i];
        asciiShiftTranslationMap[scanCode] = numbersShift[i];
        scanCode++;
    }

    // These are next to the enter key on my keyboard...
    asciiTranslationMap[0x2f] = '[';
    asciiTranslationMap[0x30] = ']';
    asciiTranslationMap[0x32] = '\\';
    asciiTranslationMap[0x33] = ';';
    asciiTranslationMap[0x34] = '\'';
    asciiTranslationMap[0x35] = 0x60; //'`';
    asciiTranslationMap[0x36] = ',';
    asciiTranslationMap[0x37] = '.';
    asciiTranslationMap[0x38] = '/';
    // Numpad
    asciiTranslationMap[0x59] = '1';
    asciiTranslationMap[0x5a] = '2';
    asciiTranslationMap[0x5b] = '3';
    asciiTranslationMap[0x5c] = '4';
    asciiTranslationMap[0x5d] = '5';
    asciiTranslationMap[0x5e] = '6';
    asciiTranslationMap[0x5f] = '7';
    asciiTranslationMap[0x60] = '8';
    asciiTranslationMap[0x61] = '9';
    asciiTranslationMap[0x62] = '0';
    asciiTranslationMap[SDL_SCANCODE_KP_DIVIDE] = '/';
    asciiTranslationMap[SDL_SCANCODE_KP_PLUS] = '+';
    asciiTranslationMap[SDL_SCANCODE_KP_MINUS] = '-';
    asciiTranslationMap[SDL_SCANCODE_KP_MULTIPLY] = '*';
    asciiTranslationMap[SDL_SCANCODE_KP_COMMA] = '.';

    // SHIFT
    asciiShiftTranslationMap[0x2f] = '{';
    asciiShiftTranslationMap[0x30] = '}';
    asciiShiftTranslationMap[0x32] = '|';
    asciiShiftTranslationMap[0x33] = ':';
    asciiShiftTranslationMap[0x34] = '"';
    asciiShiftTranslationMap[0x35] = '~';
    asciiShiftTranslationMap[0x36] = '<';
    asciiShiftTranslationMap[0x37] = '>';
    asciiShiftTranslationMap[0x38] = '?';
    // Numpad
    asciiShiftTranslationMap[0x59] = '1';
    asciiShiftTranslationMap[0x5a] = '2';
    asciiShiftTranslationMap[0x5b] = '3';
    asciiShiftTranslationMap[0x5c] = '4';
    asciiShiftTranslationMap[0x5d] = '5';
    asciiShiftTranslationMap[0x5e] = '6';
    asciiShiftTranslationMap[0x5f] = '7';
    asciiShiftTranslationMap[0x60] = '8';
    asciiShiftTranslationMap[0x61] = '9';
    asciiShiftTranslationMap[0x62] = '0';
    asciiTranslationMap[SDL_SCANCODE_KP_DIVIDE] = '/';
    asciiTranslationMap[SDL_SCANCODE_KP_PLUS] = '+';
    asciiTranslationMap[SDL_SCANCODE_KP_MINUS] = '-';
    asciiTranslationMap[SDL_SCANCODE_KP_MULTIPLY] = '*';
    asciiTranslationMap[SDL_SCANCODE_KP_COMMA] = '.';       // This should probably be localized...



    return scanCode;
}



KeyPress SDLKeyboardDriver::TranslateSDLEvent(const SDL_KeyboardEvent &kbdEvent) {
    KeyPress keyPress;
    keyPress.modifiers = TranslateModifiers(SDL_GetModState());
    if (kbdEvent.keysym.sym) {
        keyPress.isHwEventValid = true;
        keyPress.hwEvent.scanCode = kbdEvent.keysym.scancode;
        if (sdlToKeyCodes.find(kbdEvent.keysym.sym) != sdlToKeyCodes.end()) {
            keyPress.isSpecialKey = true;
            keyPress.isKeyValid = true;
            keyPress.specialKey = sdlToKeyCodes[kbdEvent.keysym.sym];
        } else {

            // Assume this..
//            keyPress.key = SDL_GetKeyFromScancode(kbdEvent.keysym.scancode);
//            if (kbdEvent.keysym.mod & (SDL_KMOD_SHIFT | SDL_KMOD_CAPS)) {
//                keyPress.key &= ~0x20;
//            }


            // Note: This is very wrong for any other locale than mine...
            // This should NOT use scan-codes!!!
            if (kbdEvent.keysym.mod & (SDL_KMOD_SHIFT | SDL_KMOD_CAPS)) {
                keyPress.key = asciiShiftTranslationMap[kbdEvent.keysym.scancode];
            } else {
                keyPress.key = asciiTranslationMap[kbdEvent.keysym.scancode];
            }
            keyPress.isSpecialKey = false;
            keyPress.isKeyValid = true;

        }
    }
    return keyPress;
}

uint8_t SDLKeyboardDriver::TranslateModifiers(const uint16_t sdlModifiers) {
    uint8_t modifiers = 0;
    if (sdlModifiers & SDL_KMOD_LSHIFT) {
        modifiers |= Keyboard::kMod_LeftShift;
    }
    if (sdlModifiers & SDL_KMOD_RSHIFT) {
        modifiers |= Keyboard::kMod_RightShift;
    }
    if (sdlModifiers & SDL_KMOD_LCTRL) {
        modifiers |= Keyboard::kMod_LeftCtrl;
    }
    if (sdlModifiers & SDL_KMOD_RCTRL) {
        modifiers |= Keyboard::kMod_RightCtrl;
    }
    if (sdlModifiers & SDL_KMOD_LALT) {
        modifiers |= Keyboard::kMod_LeftAlt;
    }
    if (sdlModifiers & SDL_KMOD_RALT) {
        modifiers |= Keyboard::kMod_RightAlt;
    }
    if (sdlModifiers & SDL_KMOD_LGUI) {
        modifiers |= Keyboard::kMod_LeftCommand;
    }
    if (sdlModifiers & SDL_KMOD_RGUI) {
        modifiers |= Keyboard::kMod_RightCommand;
    }
    return modifiers;
}



