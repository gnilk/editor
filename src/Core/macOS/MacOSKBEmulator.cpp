//
// Created by gnilk on 19.05.2026.
//
#include "MacOSKBEmulator.h"
#include "Core/KeyPress.h"
#include "Core/UnicodeHelper.h"
#include <vector>
#include <fstream>
#include <filesystem>

using namespace gedit;
bool MacOSKBEmulator::Initialize() {
    ReadAndConvert("sqlite3.c");
    timer.Reset();
    waitMs = 5000;
    return true;
}

KeyPress MacOSKBEmulator::GetKeyPress() {
    // TODO: Return key-presses like a human...
    // Read a file (like: sqlite3.c)
    //  translate any character to a KeyPress event,
    //  on each call randomize a wait-timer and check if it has passed
    //  if passed, randomize a new value and return next value from the KeyPress array
    if (idxNext < eventList.size()) {
        // Did 'waitMS' pass?
        if (timer.Sample().count() < waitMs) {
            return {};
        }
        waitMs = 10;
        auto result = eventList[idxNext++];
        timer.Reset();
        return result;
    }
    return {};
}


void MacOSKBEmulator::ReadAndConvert(const std::string &filename) {
    auto szFile = std::filesystem::file_size(filename);

    auto ptrData = new char [szFile + 10];
    memset(ptrData, 0, szFile + 10);

    std::ifstream inputStream(filename, std::ios::binary);
    inputStream.read(static_cast<char *>(ptrData), szFile);
    inputStream.close();

    for (auto i=0;i<szFile;i++) {
        KeyPress kp = {};
        auto v = ptrData[i];
        if (v < 31) {
            kp.isKeyValid = true;
            kp.isSpecialKey = true;
            kp.isHwEventValid = false;
            kp.modifiers = 0;
            if (v == '\n') {
                kp.specialKey = Keyboard::kKeyCode_Return;
            } else if (v == ' ') {
                kp.specialKey = Keyboard::kKeyCode_Space;
            } else {
                continue;;
            }
        } else {
            // FIXME: Here we need some handling of 'return' / 'tab'
            std::string dummy;
            dummy.push_back(v);
            auto u32str = UnicodeHelper::utf8to32(dummy);
            kp.isKeyValid =  true;
            kp.isSpecialKey =  false;
            kp.key = u32str[0];
        }
        eventList.push_back(kp);
    }

    delete []ptrData;
}
