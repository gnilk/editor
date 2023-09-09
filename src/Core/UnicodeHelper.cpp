//
// Created by gnilk on 09.09.23.
//

#include "UnicodeHelper.h"
#include "ConvertUTF.h"

using namespace gedit;

bool UnicodeHelper::ConvertUTF8ToUTF32String(std::u32string &out, const std::string &src) {
    if (src.empty()) {
        return true;
    }


    const UTF8 *srcStart = reinterpret_cast<const UTF8 *>(&src[0]);
    const UTF8 *srcEnd = &srcStart[src.length()];

    // Should be reserve but doesn't matter since we operate on the raw pointers
    out.resize(src.length());

    auto dstStart = reinterpret_cast<UTF32 *>(&out[0]);
    auto dstEnd = &dstStart[out.capacity()];

    auto result = ::ConvertUTF8toUTF32(&srcStart,
                                       srcEnd,
                                       &dstStart,
                                       dstEnd,
                                       strictConversion);

    if (result != ConversionResult::conversionOK) {
        return false;
    }
    return true;
}

bool UnicodeHelper::ConvertUTF32ToUTF8String(std::string &out, const std::u32string &src) {

    if (src.empty()) {
        return true;
    }

    const UTF32 *srcStart = reinterpret_cast<const UTF32 *>(&src[0]);
    const UTF32 *srcEnd = &srcStart[src.length()];

    // Should be reserve but doesn't matter since we operate on the raw pointers
    out.resize(src.length() * UNI_MAX_UTF8_BYTES_PER_CODE_POINT  + 1);

    UTF8 *dstStart = reinterpret_cast<UTF8 *>(&out[0]);
    UTF8 *dstEnd = &dstStart[out.capacity()];

    auto result = ::ConvertUTF32toUTF8(&srcStart,
                                       srcEnd,
                                       &dstStart,
                                       dstEnd,
                                       strictConversion);

    if (result != ConversionResult::conversionOK) {
        out.clear();
        return false;
    }
    out.resize(reinterpret_cast<char *>(dstStart) - &out[0]);
    // Zero terminate this...
    out.push_back(0);
    out.pop_back();
    return true;
}

bool UnicodeHelper::ConvertUTF8ToASCII(std::string &out, const std::string &src) {
    if (src.empty()) {
        return true;
    }

    const UTF8 *srcStart = reinterpret_cast<const UTF8 *>(&src[0]);
    const UTF8 *srcEnd = &srcStart[src.length()];

    out.reserve(src.length() + 1);
    while(srcStart != srcEnd) {
        auto codePoint = *srcStart;
        if (!(codePoint & 0x80)) {
            out.push_back((char)(codePoint & 0x7f));
        }
        srcStart++;
    }
    // zero terminate this..
    out.push_back(0);
    out.pop_back();

    return true;
}

