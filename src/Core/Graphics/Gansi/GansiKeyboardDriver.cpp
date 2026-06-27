//
// gedit::Gansi keyboard driver implementation.
//
#include "Core/Graphics/Gansi/GansiKeyboardDriver.h"
#include "Core/Keyboard.h"

using namespace gedit;
using namespace gedit::Gansi;

namespace {
    // gansi named key -> editor key code.
    int ToKeyCode(gnilk::ansi::Key key) {
        switch (key) {
            case gnilk::ansi::Key::Up:        return Keyboard::kKeyCode_UpArrow;
            case gnilk::ansi::Key::Down:      return Keyboard::kKeyCode_DownArrow;
            case gnilk::ansi::Key::Left:      return Keyboard::kKeyCode_LeftArrow;
            case gnilk::ansi::Key::Right:     return Keyboard::kKeyCode_RightArrow;
            case gnilk::ansi::Key::Home:      return Keyboard::kKeyCode_Home;
            case gnilk::ansi::Key::End:       return Keyboard::kKeyCode_End;
            case gnilk::ansi::Key::PageUp:    return Keyboard::kKeyCode_PageUp;
            case gnilk::ansi::Key::PageDown:  return Keyboard::kKeyCode_PageDown;
            case gnilk::ansi::Key::Insert:    return Keyboard::kKeyCode_Insert;
            case gnilk::ansi::Key::Delete:    return Keyboard::kKeyCode_DeleteForward;
            case gnilk::ansi::Key::Enter:     return Keyboard::kKeyCode_Return;
            case gnilk::ansi::Key::Tab:       return Keyboard::kKeyCode_Tab;
            case gnilk::ansi::Key::Backspace: return Keyboard::kKeyCode_Backspace;
            case gnilk::ansi::Key::Escape:    return Keyboard::kKeyCode_Escape;
            case gnilk::ansi::Key::F1:        return Keyboard::kKeyCode_F1;
            case gnilk::ansi::Key::F2:        return Keyboard::kKeyCode_F2;
            case gnilk::ansi::Key::F3:        return Keyboard::kKeyCode_F3;
            case gnilk::ansi::Key::F4:        return Keyboard::kKeyCode_F4;
            case gnilk::ansi::Key::F5:        return Keyboard::kKeyCode_F5;
            case gnilk::ansi::Key::F6:        return Keyboard::kKeyCode_F6;
            case gnilk::ansi::Key::F7:        return Keyboard::kKeyCode_F7;
            case gnilk::ansi::Key::F8:        return Keyboard::kKeyCode_F8;
            case gnilk::ansi::Key::F9:        return Keyboard::kKeyCode_F9;
            case gnilk::ansi::Key::F10:       return Keyboard::kKeyCode_F10;
            case gnilk::ansi::Key::F11:       return Keyboard::kKeyCode_F11;
            case gnilk::ansi::Key::F12:       return Keyboard::kKeyCode_F12;
            default:                          return Keyboard::kKeyCode_None;
        }
    }
}

KeyboardDriverBase::Ref GansiKeyboardDriver::Create() {
    return std::make_shared<GansiKeyboardDriver>();
}

uint8_t GansiKeyboardDriver::TranslateModifiers(gnilk::ansi::Mod mods) const {
    uint8_t mask = Keyboard::kMod_None;
    if (gnilk::ansi::HasMod(mods, gnilk::ansi::Mod::Shift)) {
        mask |= Keyboard::kMod_LeftShift;
    }
    if (gnilk::ansi::HasMod(mods, gnilk::ansi::Mod::Ctrl)) {
        mask |= Keyboard::kMod_LeftCtrl;
    }
    if (gnilk::ansi::HasMod(mods, gnilk::ansi::Mod::Alt)) {
        mask |= Keyboard::kMod_LeftAlt;
    }
    if (gnilk::ansi::HasMod(mods, gnilk::ansi::Mod::Super)) {
        mask |= Keyboard::kMod_LeftCommand;
    }
    return mask;
}

KeyPress GansiKeyboardDriver::TranslateKeyEvent(const gnilk::ansi::KeyEvent &ev) const {
    KeyPress kp;
    kp.modifiers = TranslateModifiers(ev.mods);
    if (ev.isChar) {
        kp.isKeyValid = true;
        kp.key = ev.ch;
    } else {
        kp.isSpecialKey = true;
        kp.specialKey = ToKeyCode(ev.key);
    }
    return kp;
}
