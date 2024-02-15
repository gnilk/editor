//
// Created by gnilk on 15.02.24.
//

#ifndef SHELL_ANSIPARSER_H
#define SHELL_ANSIPARSER_H

#include <stdint.h>
#include <string>

namespace gedit {
    class AnsiParser {
    public:
        AnsiParser() = default;
        virtual ~AnsiParser() = default;
        std::string Strip(const uint8_t *ptrBuffer, const size_t size);
    protected:
        std::string StripInternal();
        void StripCSI();
        void StripOSC();
        bool InRange(const std::pair<int,int> &range);

        std::string OSC_ParseStringToBel();

        bool Next();
        uint8_t At();
    private:
        size_t idx = 0;
        size_t max = 0;
        const uint8_t *buffer = nullptr;
    };
}


#endif //SHELL_ANSIPARSER_H
