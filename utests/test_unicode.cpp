//
// Created by gnilk on 04.03.24.
//

#include <testinterface.h>
#include "Core/UnicodeHelper.h"
#include "Core/HexDump.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_unicode(ITesting *t);
    DLL_EXPORT int test_unicode_convchar(ITesting *t);
}
DLL_EXPORT int test_unicode(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_unicode_convchar(ITesting *t) {
    static uint8_t utf8chars[]={0x4d, 0xc3, 0xa4, 0x72, 0x00};

    HexDump::ToConsole(utf8chars, sizeof(utf8chars));

    std::u32string u32String;
    int pos = 0;
    while(utf8chars[pos] != 0) {
        int numConsumed = UnicodeHelper::ConvertUTF8ToUTF32Char(u32String, &utf8chars[pos], sizeof(utf8chars) - pos);
        TR_ASSERT(t, numConsumed >= 0);
        printf("Consumed: %d\n", numConsumed);
        pos += numConsumed;
        if (pos >= sizeof(utf8chars)) {
            break;
        }
    }
    std::string utf8String;
    TR_ASSERT(t, UnicodeHelper::ConvertUTF32ToUTF8String(utf8String, u32String));
    printf("Converted back: %s\n", utf8String.c_str());
    return kTR_Pass;
}
