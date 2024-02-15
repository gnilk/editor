//
// Created by gnilk on 9/15/2021.
//

#include "HexDump.h"
#include <string.h>

void HexDump::Write(std::function<void(const char *str)> printer, const uint8_t *pData, const size_t szData) {
    char hexdump[16*5+2];
    char ascii[16 + 2];
    char* ptrHexdump = hexdump;
    char* ptrAscii = ascii;
    char strFinal[256];

    int32_t addr = 0;
    for (size_t i = 0; i < szData; i++) {
        snprintf(ptrHexdump, 4, "%.2x ", pData[i]);
        snprintf(ptrAscii, 2, "%c", ((pData[i]>31) && (pData[i]<128))?pData[i]:'.');
        ptrAscii++;
        ptrHexdump += 3;
        if ((i & 15) == 15) {
            snprintf(strFinal,sizeof(strFinal), "%.4x %s    %s", addr, hexdump, ascii);
            printer(strFinal);
            addr += 16;

            memset(ascii, ' ', 16+2);
            memset(hexdump, ' ', 16*5+2);

            ptrHexdump = hexdump;
            ptrAscii = ascii;
        } else if ((i & 7) == 7) {
            *ptrHexdump = ' ';
            ptrHexdump++;
        }
    }

    int nPadding = 16 - (szData & 15);
    for (int i = 0; i < nPadding; i++) {
        snprintf(ptrHexdump, 4, "   ");
        ptrHexdump+= 3;

    }
    snprintf(strFinal,sizeof(strFinal), "%.4x %s     %s", addr, hexdump, ascii);
    printer(strFinal);
}

void HexDump::ToLog(gnilk::ILogger *pLogger, const void *pData, const size_t szData) {
    auto writer = [pLogger](const char *s){pLogger->Debug("%s",s);};
    Write(writer, static_cast<const uint8_t *>(pData), szData);
}


void HexDump::ToConsole(const void *pData, const size_t szData) {
    auto writer = [](const char *s){printf("%s\n",s);};
    Write(writer, static_cast<const uint8_t *>(pData), szData);
}
