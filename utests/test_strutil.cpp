//
// Created by gnilk on 09.09.23.
//
#include <testinterface.h>
#include <string>
#include <locale>

#include "Core/StrUtil.h"
#include "Core/UnicodeHelper.h"


using namespace gedit;

extern "C" {
DLL_EXPORT int test_strutil(ITesting *t);
DLL_EXPORT int test_strutil_utf32toutf8(ITesting *t);
DLL_EXPORT int test_strutil_utf8toascii(ITesting *t);
DLL_EXPORT int test_strutil_ltrim32(ITesting *t);
DLL_EXPORT int test_strutil_trim32(ITesting *t);
DLL_EXPORT int test_strutil_ucode(ITesting *t);
}

DLL_EXPORT int test_strutil(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_strutil_utf32toutf8(ITesting *t) {
    std::u32string str = U"this is utf32";
    auto utf8str = UnicodeHelper::utf32to8(str);
    TR_ASSERT(t, !utf8str.empty());

    return kTR_Pass;
}
DLL_EXPORT int test_strutil_utf8toascii(ITesting *t) {
    auto src = u8"åäö-mamma";
    auto dst = UnicodeHelper::utf8toascii(src);

    printf("dst: %s\n", dst.c_str());
//    auto other = "-mamma";
//    auto res = dst == other;

    TR_ASSERT(t, dst == "-mamma");
    return kTR_Pass;
}

DLL_EXPORT int test_strutil_ltrim32(ITesting *t) {
    std::u32string str = U"   mamma";
    const std::u32string chars = U"\t\n\v\f\r ";

    strutil::ltrim(str);

    std::u8string utf8str;
    TR_ASSERT(t, UnicodeHelper::ConvertUTF32ToUTF8String(utf8str, str));

    TR_ASSERT(t, utf8str == u8"mamma");

    return kTR_Pass;
}

DLL_EXPORT int test_strutil_trim32(ITesting *t) {
    std::u32string str = U"   it should only be this   ";
    strutil::trim(str);

    std::u8string utf8str;
    TR_ASSERT(t, UnicodeHelper::ConvertUTF32ToUTF8String(utf8str, str));

    printf("trimmed: '%s'\n", utf8str.c_str());

    TR_ASSERT(t, utf8str == u8"it should only be this");

    return kTR_Pass;
}

// This just tests a few unicode things..
DLL_EXPORT int test_strutil_ucode(ITesting *t) {


    std::vector<std::u32string> lines = {
            U"line a",
            U"line b",
            U"line c",
    };
    // flatten
    std::u32string flattened;
    for(auto &l : lines) {
        flattened += l;
        flattened += U"\n";
    }
    auto utf8str = UnicodeHelper::utf32to8(flattened);
    printf("result, %zu bytes:\n'%s'\n", utf8str.length(), utf8str.c_str());

    // Checking the length/size stuff UTF32 vs UTF8


    std::u8string u8str = u8"åäö";
    std::u32string u32str = U"åäö";

    // Both returns the number of code-points (?), and u8 requires 2 code points per char
    printf("u32 len=%zu, size=%zu\n", u32str.length(), u32str.size());
    printf("u8  len=%zu, size=%zu\n", u8str.length(), u8str.size());


    return kTR_Pass;
}