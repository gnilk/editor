//
// Hex dump utility class
//

#ifndef GNILK_HEXDUMP_H
#define GNILK_HEXDUMP_H

#include "logger.h"
#include <stdint.h>
#include <stdlib.h>
#include <functional>

class HexDump {
public:
    static void Write(std::function<void(const char *str)> printer, const uint8_t *pData, const size_t szData);
    static void ToLog(gnilk::ILogger *pLogger, const void *pData, const size_t szData);
    static void ToConsole(const void *pData, const size_t szData);
};


#endif //GNILK_HEXDUMP_H
