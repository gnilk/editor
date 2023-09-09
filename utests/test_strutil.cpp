//
// Created by gnilk on 09.09.23.
//
#include <testinterface.h>
#include <string>

#include "Core/StrUtil.h"
#include "Core/UnicodeHelper.h"


using namespace gedit;

extern "C" {
DLL_EXPORT int test_strutil(ITesting *t);
DLL_EXPORT int test_strutil_utf32toutf8(ITesting *t);
DLL_EXPORT int test_strutil_utf8toascii(ITesting *t);
DLL_EXPORT int test_strutil_ltrim32(ITesting *t);
DLL_EXPORT int test_strutil_trim32(ITesting *t);
}

DLL_EXPORT int test_strutil(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_strutil_utf32toutf8(ITesting *t) {
    std::u32string str = U"this is utf32";
    std::string utf8str;
    TR_ASSERT(t, UnicodeHelper::ConvertUTF32ToUTF8String(utf8str, str));

    TR_ASSERT(t, utf8str == u8"this is utf32");
    return kTR_Pass;
}
DLL_EXPORT int test_strutil_utf8toascii(ITesting *t) {
    std::string src = u8"åäö-mamma";
    std::string dst;
    TR_ASSERT(t, UnicodeHelper::ConvertUTF8ToASCII(dst, src));
    printf("dst: %s\n", dst.c_str());
    TR_ASSERT(t, dst == "-mamma");
    return kTR_Pass;
}

DLL_EXPORT int test_strutil_ltrim32(ITesting *t) {
    std::u32string str = U"   mamma";
    const std::u32string chars = U"\t\n\v\f\r ";

    strutil::ltrim(str);

    std::string utf8str;
    TR_ASSERT(t, UnicodeHelper::ConvertUTF32ToUTF8String(utf8str, str));
    TR_ASSERT(t, utf8str == u8"mamma");

    return kTR_Pass;
}

DLL_EXPORT int test_strutil_trim32(ITesting *t) {
    std::u32string str = U"   it should only be this   ";
    strutil::trim(str);

    std::string utf8str;
    TR_ASSERT(t, UnicodeHelper::ConvertUTF32ToUTF8String(utf8str, str));

    printf("trimmed: '%s'\n", utf8str.c_str());

    TR_ASSERT(t, utf8str == u8"it should only be this");

    return kTR_Pass;
}
