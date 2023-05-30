//
// Created by gnilk on 29.03.23.
//
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_scancode.h>
#include <map>
#include <unordered_map>
#include <string>
#include <SDL2/SDL.h>
#include <SDL2/SDL_keycode.h>

#include "logger.h"

#include "Core/Keyboard.h"
#include "Core/KeyPress.h"
#include "SDLKeyboardDriver.h"
#include "Core/KeyMapping.h"
#include "Core/RuntimeConfig.h"

using namespace gedit;

static int createTranslationTable();


bool SDLKeyboardDriver::Initialize() {
    createTranslationTable();
    SDL_StartTextInput();
    return true;
}
KeyPress SDLKeyboardDriver::GetKeyPress() {
    SDL_Event event;
    auto logger = gnilk::Logger::GetLogger("SDLKeyboardDriver");

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EventType::SDL_QUIT) {
            SDL_Quit();
            exit(0);
        }  else if (event.type == SDL_EventType::SDL_KEYDOWN) {
            auto kp = HandleKeyPressEvent(event);
            if (kp.has_value()) {
                CheckRemoveTextInputEventForKeyPress(kp.value());
                return *kp;
            }
            continue;
        } else if (event.type == SDL_EventType::SDL_TEXTINPUT) {
            // TO-DO: On Linux we get an SDL_TEXTINPUT for the keydown - causing us to act twice for certain key combos..
            KeyPress kp;
            kp.isSpecialKey = false;
            kp.isKeyValid = true;
            kp.modifiers = TranslateModifiers(SDL_GetModState());
            // This seems to work, but I assume that we can get buffered input here
            // Need to check if there are some flags in SDL to deal with it
            kp.key = event.text.text[0];
            logger->Debug("SDL_EVENT_TEXT_INPUT, event.text.text=%s", event.text.text);
            return kp;
        }  else if ((event.type == SDL_EventType::SDL_WINDOWEVENT) && (event.window.event == SDL_WINDOWEVENT_RESIZED)) {
            logger->Debug("SDL_EVENT_WINDOW_RESIZED");
            RuntimeConfig::Instance().Screen()->OnSizeChanged();
        } else {
            // Note: Enable this to track any other event we might want...
            // logger->Debug("Unhandled event: %d (0x%.x)", event.type, event.type);
        }
    }
    return {};
}

// Check if this is needed on the mac - currently enabled..
void SDLKeyboardDriver::CheckRemoveTextInputEventForKeyPress(const KeyPress &kp) {
    SDL_Event peekEvents[16];   // large enough? - normally just one event - so probably...
    auto logger = gnilk::Logger::GetLogger("SDLKeyboardDriver");

    SDL_PumpEvents();
    int nEvents = SDL_PeepEvents(peekEvents, 16, SDL_PEEKEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
    if (nEvents < 0) {
        logger->Error("SDL_PeepEvents, err=%s", SDL_GetError());
        return;
//        fprintf(stderr, "SDL_PeepEvents, err=%s\n",SDL_GetError());
//        exit(1);
    }

//    logger->Debug("Checking %d events", nEvents);

    for(int i=0;i<nEvents;i++) {
        // Check and remove the next text-input even in the queue matching our key...
        if (peekEvents[i].type == SDL_EventType::SDL_TEXTINPUT) {
            if (peekEvents[i].text.text[0] == kp.key) {
                SDL_Event dummy;
                int nGet = SDL_PeepEvents(&dummy, 1, SDL_GETEVENT, SDL_EventType::SDL_TEXTINPUT, SDL_EventType::SDL_TEXTINPUT);
                if (nGet < 0) {
                    logger->Error("SDL_PeepEvents, err=%s", SDL_GetError());
                    return;

//                    fprintf(stderr, "SDL_PeepEvents, err=%s\n",SDL_GetError());
//                    exit(1);
                }
                // logger->Debug("TextInput Event for KeyPress found - removed!");
                return;
            }
        }
    }
}


std::optional<KeyPress> SDLKeyboardDriver::HandleKeyPressEvent(const SDL_Event &event) {
    auto logger = gnilk::Logger::GetLogger("SDLKeyboardDriver");

    auto kp =  TranslateSDLEvent(event.key);

    logger->Debug("KeyDown event: %d (0x%.x) - sym: %x (%d), scancode: %x (%d)", event.type, event.type,
                  event.key.keysym.sym, event.key.keysym.sym,
                  event.key.keysym.scancode, event.key.keysym.scancode);

    if (kp.isSpecialKey) {
        auto keyName = Keyboard::KeyCodeName(static_cast<Keyboard::kKeyCode>(kp.specialKey));
        logger->Debug("  special kp, modifiers=%.2x, specialKey=%.2x (%s)", kp.modifiers, kp.specialKey, keyName.c_str());
        return kp;
    } else if (kp.modifiers != 0) {
        static int shiftModifiers = Keyboard::kModifierKeys::kMod_RightShift | Keyboard::kModifierKeys::kMod_LeftShift;
        kp.key = TranslateScanCode(event.key.keysym.scancode); //  kp.hwEvent.scanCode);
        if ((kp.modifiers & shiftModifiers) && (kp.key != 0)) {
            logger->Debug("Shift+ASCII  (%c) - skipping, this is handled by EVENT_TEXT_INPUT", kp.key);
            return {};
        }
        if (kp.key != 0) {
            kp.isKeyValid = true;
        }
        logger->Debug("  kp, modifiers=%.2x (%d), scancode=%.2x, key=%.2x (%c), ", kp.modifiers, kp.modifiers, kp.hwEvent.scanCode, kp.key, kp.key);
        return kp;
    }

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

static std::unordered_map<int, char> scanCodeToAscii;
static std::unordered_map<int, char> asciiShiftTranslationMap;

int SDLKeyboardDriver::TranslateScanCode(int scanCode) {
    if (scanCodeToAscii.find(scanCode) == scanCodeToAscii.end()) {
        return 0;
    }
    return scanCodeToAscii[scanCode];
}


//
// This is based on inspection...
// Not exactly sure which 'driver' I used to do this...
//
static int createTranslationTable() {
    int scanCode = 0x04;
    for(int i='a';i<='z';i++) {
        scanCodeToAscii[scanCode] = i;
        asciiShiftTranslationMap[scanCode] = std::toupper(i);
        scanCode++;
    }

    static std::string numbers="1234567890";
    static std::string numbersShift="!@#$%^&*()";
    for(int i=0;i<numbers.size();i++) {
        scanCodeToAscii[scanCode] = numbers[i];
  //      asciiShiftTranslationMap[scanCode] = numbersShift[i];
        scanCode++;
    }

    // These are next to the enter key on my keyboard...
    scanCodeToAscii[0x2f] = '[';
    scanCodeToAscii[0x30] = ']';
    scanCodeToAscii[0x31] = '\\';
    scanCodeToAscii[0x32] = '\\';       // Not sure - can't seem to generate this one now...  might have been a typo..
    scanCodeToAscii[0x33] = ';';
    scanCodeToAscii[0x34] = '\'';
    scanCodeToAscii[0x35] = 0x60; //'`';
    scanCodeToAscii[0x36] = ',';
    scanCodeToAscii[0x37] = '.';
    scanCodeToAscii[0x38] = '/';
    // Numpad
    scanCodeToAscii[0x59] = '1';
    scanCodeToAscii[0x5a] = '2';
    scanCodeToAscii[0x5b] = '3';
    scanCodeToAscii[0x5c] = '4';
    scanCodeToAscii[0x5d] = '5';
    scanCodeToAscii[0x5e] = '6';
    scanCodeToAscii[0x5f] = '7';
    scanCodeToAscii[0x60] = '8';
    scanCodeToAscii[0x61] = '9';
    scanCodeToAscii[0x62] = '0';
    scanCodeToAscii[SDL_SCANCODE_KP_DIVIDE] = '/';
    scanCodeToAscii[SDL_SCANCODE_KP_PLUS] = '+';
    scanCodeToAscii[SDL_SCANCODE_KP_MINUS] = '-';
    scanCodeToAscii[SDL_SCANCODE_KP_MULTIPLY] = '*';
    scanCodeToAscii[SDL_SCANCODE_KP_COMMA] = '.';
/*
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

*/

    return scanCode;
}



KeyPress SDLKeyboardDriver::TranslateSDLEvent(const SDL_KeyboardEvent &kbdEvent) {
    KeyPress keyPress{};
    keyPress.modifiers = TranslateModifiers(SDL_GetModState());
    if (kbdEvent.keysym.sym) {
//        keyPress.isHwEventValid = true;
//        keyPress.hwEvent.scanCode = kbdEvent.keysym.scancode;
        if (sdlToKeyCodes.find(kbdEvent.keysym.sym) != sdlToKeyCodes.end()) {
            keyPress.isSpecialKey = true;
            keyPress.isKeyValid = true;
            keyPress.specialKey = sdlToKeyCodes[kbdEvent.keysym.sym];
        }
        else {

//            // Note: This is very wrong for any other locale than mine...
//            // This should NOT use scan-codes!!!
//            if (kbdEvent.keysym.mod & (SDL_KMOD_SHIFT | SDL_KMOD_CAPS)) {
//                keyPress.key = asciiShiftTranslationMap[kbdEvent.keysym.scancode];
//            } else {
//                keyPress.key = asciiTranslationMap[kbdEvent.keysym.scancode];
//            }
//            keyPress.isSpecialKey = false;
//            keyPress.isKeyValid = true;

        }
    }
    return keyPress;
}

uint8_t SDLKeyboardDriver::TranslateModifiers(const uint16_t sdlModifiers) {
    uint8_t modifiers = 0;
    if (sdlModifiers & KMOD_LSHIFT) {
        modifiers |= Keyboard::kMod_LeftShift;
    }
    if (sdlModifiers & KMOD_RSHIFT) {
        modifiers |= Keyboard::kMod_RightShift;
    }
    if (sdlModifiers & KMOD_LCTRL) {
        modifiers |= Keyboard::kMod_LeftCtrl;
    }
    if (sdlModifiers & KMOD_RCTRL) {
        modifiers |= Keyboard::kMod_RightCtrl;
    }
    if (sdlModifiers & KMOD_LALT) {
        modifiers |= Keyboard::kMod_LeftAlt;
    }
    if (sdlModifiers & KMOD_RALT) {
        modifiers |= Keyboard::kMod_RightAlt;
    }
    if (sdlModifiers & KMOD_LGUI) {
        modifiers |= Keyboard::kMod_LeftCommand;
    }
    if (sdlModifiers & KMOD_RGUI) {
        modifiers |= Keyboard::kMod_RightCommand;
    }
    return modifiers;
}



