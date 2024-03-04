//
// Created by gnilk on 04.03.24.
//

#include <testinterface.h>
#include "Core/UnicodeHelper.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_unicode(ITesting *t);
    DLL_EXPORT int test_unicode_convchar(ITesting *t);
}
DLL_EXPORT int test_unicode(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_unicode_convchar(ITesting *t) {
    static uint8_t utf8chars[]={0x32,0x41,0x00};
    printf("%s\n", utf8chars);
    return kTR_Pass;
}
