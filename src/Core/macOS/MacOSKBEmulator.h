//
// Created by gnilk on 19.05.2026.
//

#ifndef GOATEDIT_MACOSKBEMULATOR_H
#define GOATEDIT_MACOSKBEMULATOR_H

#include "Core/Graphics/KeyboardDriverBase.h"
#include "Core/DurationTimer.h"
#include <vector>

namespace gedit {
    class MacOSKBEmulator : public KeyboardDriverBase {
    public:
        MacOSKBEmulator() = default;
        virtual ~MacOSKBEmulator() = default;
        bool Initialize();

        KeyPress GetKeyPress() override;
    private:
        void ReadAndConvert(const std::string &filename);
    private:
        DurationTimer timer;
        uint32_t waitMs;
        size_t idxNext = 0;
        std::vector<KeyPress> eventList = {};
    };
}
#endif //GOATEDIT_MACOSKBEMULATOR_H
