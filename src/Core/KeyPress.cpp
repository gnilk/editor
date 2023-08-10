//
// Created by gnilk on 10.08.23.
//
#include <logger.h>

#include "KeyPress.h"
#include "Core/KeyMapping.h"

using namespace gedit;

void KeyPress::DumpToLog() {
    auto logger = gnilk::Logger::GetLogger("KeyPress");

    std::string keyName = "";
    if (isSpecialKey) {
        keyName = Keyboard::KeyCodeName(static_cast<Keyboard::kKeyCode>(specialKey));
    }

    logger->Debug("isKeyValid: %s, isHWEventValid: %s, isSpecialKey: %s",
                  isKeyValid?"yes":"no",
                  isHwEventValid?"yes":"no",
                  isSpecialKey?"yes":"no");
    logger->Debug("Modifiers: 0x%.2x (%c%c%c%c), key: %d (0x%.2x), specialKey: %s (%d, 0x%.2x)",
                  modifiers,
                  IsShiftPressed()?'S':'-',
                  IsAltPressed()?'A':'-',
                  IsCtrlPressed()?'C':'-',
                  IsCommandPressed()?'M':'-',
                  key, key, keyName.c_str(), specialKey, specialKey);


}
