//
// Created by gnilk on 29.03.23.
//
#include <map>
#include <memory>
#include <unordered_map>
#include <string>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>

#include "logger.h"

#include "Core/Keyboard.h"
#include "Core/KeyPress.h"
#include "SDLKeyboardDriver.h"
#include "Core/KeyMapping.h"
#include "Core/RuntimeConfig.h"
#include "Core/Editor.h"
#include "Core/TextBuffer.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;
using namespace gedit::SDL3;

static int createTranslationTable();

KeyboardDriverBase::Ref SDLKeyboardDriver::Create() {
    auto instance = std::make_shared<SDLKeyboardDriver>();
    if (!instance->Initialize()) {
        return nullptr;
    }
    return instance;
}

bool SDLKeyboardDriver::Initialize() {
    createTranslationTable();
    sdlDummyEvent = SDL_RegisterEvents(1);
    // Note: SDL_StartTextInput(window) is called by SDLScreen::Open() once the
    // window is available — SDL3 requires a window pointer.
    HookEditorClipBoard();
    return true;
}

void SDLKeyboardDriver::Close() {
    // SDL_StopTextInput(window) is called by SDLScreen::Close() which owns the window.
}

//
// ProcessEvent is the single entry-point called by SDLScreen::PollEvents() for every SDL event.
// Only SDL_EVENT_KEY_DOWN and SDL_EVENT_TEXT_INPUT produce a KeyPress; everything else returns empty.
//
std::optional<KeyPress> SDLKeyboardDriver::ProcessEvent(const SDL_Event &event) {
    auto logger = gnilk::Logger::GetLogger("SDL3KeyboardDriver");

    if (event.type == SDL_EVENT_KEY_DOWN) {
        auto kp = HandleKeyPressEvent(event);
        if (kp.has_value()) {
            CheckRemoveTextInputEventForKeyPress(*kp);
        }
        return kp;
    }

    if (event.type == SDL_EVENT_TEXT_INPUT) {
        // Suppress text-input when a control/command/alt modifier is held — those combos
        // arrive as SDL_EVENT_KEY_DOWN with modifiers set and are handled there.
        static const auto mask = static_cast<uint8_t>(
            Keyboard::kMod_LeftCtrl  | Keyboard::kMod_RightCtrl  |
            Keyboard::kMod_LeftCommand | Keyboard::kMod_RightCommand |
            Keyboard::kMod_LeftAlt   | Keyboard::kMod_RightAlt);

        auto modifiers = TranslateModifiers(SDL_GetModState());
        if (modifiers & mask) {
            logger->Debug("SDL_EVENT:TEXTINPUT, modifier mask (0x%.2x) invalid to regular input - skipping", modifiers);
            return {};
        }

        KeyPress kp;
        kp.isSpecialKey = false;
        kp.isKeyValid   = true;
        kp.modifiers    = modifiers;
        auto u32str = UnicodeHelper::utf8to32(event.text.text);
        kp.key = u32str[0];
        logger->Debug("SDL_EVENT:TEXTINPUT, modifiers=%x, event.text.text=%s", modifiers, event.text.text);
        return kp;
    }

    return {};
}

void SDLKeyboardDriver::CheckRemoveTextInputEventForKeyPress(const KeyPress &kp) {
    SDL_Event peekEvents[16];
    auto logger = gnilk::Logger::GetLogger("SDL3KeyboardDriver");

    SDL_PumpEvents();
    int nEvents = SDL_PeepEvents(peekEvents, 16, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
    if (nEvents < 0) {
        logger->Error("SDL_PeepEvents, err=%s", SDL_GetError());
        return;
    }

    for (int i = 0; i < nEvents; i++) {
        if (peekEvents[i].type == SDL_EVENT_TEXT_INPUT) {
            if (peekEvents[i].text.text[0] == kp.key) {
                SDL_Event dummy;
                int nGet = SDL_PeepEvents(&dummy, 1, SDL_GETEVENT, SDL_EVENT_TEXT_INPUT, SDL_EVENT_TEXT_INPUT);
                if (nGet < 0) {
                    logger->Error("SDL_PeepEvents, err=%s", SDL_GetError());
                }
                return;
            }
        }
    }
}

std::optional<KeyPress> SDLKeyboardDriver::HandleKeyPressEvent(const SDL_Event &event) {
    auto logger = gnilk::Logger::GetLogger("SDL3KeyboardDriver");

    auto kp = TranslateSDLEvent(event.key);

    // SDL3: key event fields are event.key.key (keycode) and event.key.scancode
    logger->Debug("KeyDown event: %d (0x%.x) - sym: %x (%d), scancode: %x (%d)", event.type, event.type,
                  (int)event.key.key, (int)event.key.key,
                  (int)event.key.scancode, (int)event.key.scancode);

    if (kp.isSpecialKey) {
        auto keyName = Keyboard::KeyCodeName(static_cast<Keyboard::kKeyCode>(kp.specialKey));
        logger->Debug("  special kp, modifiers=%.2x, specialKey=%.2x (%s)", kp.modifiers, kp.specialKey, keyName.c_str());
        return kp;
    } else if (kp.modifiers != 0) {
        static int shiftModifiers = Keyboard::kModifierKeys::kMod_RightShift | Keyboard::kModifierKeys::kMod_LeftShift;
        kp.key = TranslateScanCode(event.key.scancode);
        if ((kp.modifiers & shiftModifiers) && (kp.key != 0)) {
            logger->Debug("Shift+ASCII  (%c) - skipping, this is handled by EVENT_TEXT_INPUT", (int)kp.key);
            return {};
        }
        if (kp.key != 0) {
            kp.isKeyValid = true;
        }
        logger->Debug("  kp, modifiers=%.2x (%d), scancode=%.2x, key=%x (%d), ",
                      kp.modifiers, kp.modifiers,
                      (int)event.key.scancode,
                      (int)kp.key, (int)kp.key);
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

static int createTranslationTable() {
    int scanCode = 0x04;
    for (int i = 'a'; i <= 'z'; i++) {
        scanCodeToAscii[scanCode] = i;
        asciiShiftTranslationMap[scanCode] = std::toupper(i);
        scanCode++;
    }

    static std::string numbers = "1234567890";
    for (size_t i = 0; i < numbers.size(); i++) {
        scanCodeToAscii[scanCode] = numbers[i];
        scanCode++;
    }

    // These are next to the enter key on my keyboard...
    scanCodeToAscii[0x2f] = '[';
    scanCodeToAscii[0x30] = ']';
    scanCodeToAscii[0x31] = '\\';
    scanCodeToAscii[0x32] = '\\';
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
    scanCodeToAscii[SDL_SCANCODE_KP_DIVIDE]   = '/';
    scanCodeToAscii[SDL_SCANCODE_KP_PLUS]     = '+';
    scanCodeToAscii[SDL_SCANCODE_KP_MINUS]    = '-';
    scanCodeToAscii[SDL_SCANCODE_KP_MULTIPLY] = '*';
    scanCodeToAscii[SDL_SCANCODE_KP_COMMA]    = '.';

    return scanCode;
}

KeyPress SDLKeyboardDriver::TranslateSDLEvent(const SDL_KeyboardEvent &kbdEvent) {
    KeyPress keyPress{};
    keyPress.modifiers = TranslateModifiers(SDL_GetModState());
    // SDL3: keycode is kbdEvent.key (not kbdEvent.keysym.sym)
    if (kbdEvent.key) {
        if (sdlToKeyCodes.find(kbdEvent.key) != sdlToKeyCodes.end()) {
            keyPress.isSpecialKey = true;
            keyPress.isKeyValid   = true;
            keyPress.specialKey   = sdlToKeyCodes[kbdEvent.key];
        }
    }
    return keyPress;
}

// SDL3: SDL_Keymod is Uint32 (was Uint16 in SDL2); constants renamed SDL_KMOD_*
uint8_t SDLKeyboardDriver::TranslateModifiers(SDL_Keymod sdlModifiers) {
    uint8_t modifiers = 0;
    if (sdlModifiers & SDL_KMOD_LSHIFT) modifiers |= Keyboard::kMod_LeftShift;
    if (sdlModifiers & SDL_KMOD_RSHIFT) modifiers |= Keyboard::kMod_RightShift;
    if (sdlModifiers & SDL_KMOD_LCTRL)  modifiers |= Keyboard::kMod_LeftCtrl;
    if (sdlModifiers & SDL_KMOD_RCTRL)  modifiers |= Keyboard::kMod_RightCtrl;
    if (sdlModifiers & SDL_KMOD_LALT)   modifiers |= Keyboard::kMod_LeftAlt;
    if (sdlModifiers & SDL_KMOD_RALT)   modifiers |= Keyboard::kMod_RightAlt;
    if (sdlModifiers & SDL_KMOD_LGUI)   modifiers |= Keyboard::kMod_LeftCommand;
    if (sdlModifiers & SDL_KMOD_RGUI)   modifiers |= Keyboard::kMod_RightCommand;
    return modifiers;
}

// We hook the clipboard in the keyboard driver as this is the one processing messages
void SDLKeyboardDriver::HookEditorClipBoard() {
    Editor::Instance().GetClipBoard().SetOnUpdateCallback([](ClipBoard::ClipBoardItem::Ref clipBoardItem) {
        auto dstBuffer = TextBuffer::CreateEmptyBuffer();
        std::u32string flattenedText;

        clipBoardItem->PasteToBuffer(dstBuffer, {0,0});
        dstBuffer->Flatten(flattenedText, 0, clipBoardItem->GetLineCount());

        auto utf8str = UnicodeHelper::utf32to8(flattenedText);
        SDL_SetClipboardText(utf8str.c_str());
    });
}
