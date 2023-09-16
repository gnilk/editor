//
// Created by gnilk on 09.09.23.
//

#include "UnicodeHelper.h"
#include "ConvertUTF.h"

// Check this one out: https://utfcpp.sourceforge.net/
using namespace gedit;

std::u32string UnicodeHelper::utf8to32(const std::string &src) {
    std::u32string dst;
    ConvertUTF8ToUTF32String(dst, src);
    return dst;
}

std::string UnicodeHelper::utf32to8(const std::u32string &src) {
    std::string dst;
    ConvertUTF32ToUTF8String(dst, src);
    return dst;
}

std::string UnicodeHelper::utf32toascii(const std::u32string &src) {
    std::string dst;
    ConvertUTF32ToASCII(dst, src);
    return dst;
}

std::string UnicodeHelper::utf8toascii(const char8_t *src) {
    std::string dst;
    auto srcstr = reinterpret_cast<const char *>(src);
    ConvertUTF8ToASCII(dst, srcstr);
    return dst;
}

std::string UnicodeHelper::utf8toascii(const std::string &src) {
    std::string dst;
    ConvertUTF8ToASCII(dst, src);
    return dst;
}



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
    out.resize(reinterpret_cast<char32_t *>(dstStart) - &out[0]);

    return true;
}
bool UnicodeHelper::ConvertUTF32ToUTF8String(std::u8string &out, const std::u32string &src) {
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
    out.resize(reinterpret_cast<char8_t *>(dstStart) - &out[0]);
    // Zero terminate this...
    out.push_back(0);
    out.pop_back();
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

bool UnicodeHelper::ConvertUTF32ToASCII(std::string &out, const std::u32string &src) {
    if (src.empty()) {
        return true;
    }
    out.reserve(src.length() + 1);

    size_t nConverted = 0;
    for(auto codePoint : src) {
        if (codePoint < 0x80) {
            out.push_back(static_cast<char>(codePoint & 0x7f));
            nConverted++;
        }
    }
    out.resize(nConverted);

    // zero terminate this..
    out.push_back(0);
    out.pop_back();

    return true;

}


bool UnicodeHelper::ConvertUTF8ToASCII(std::string &out, const std::string &src) {
    if (src.empty()) {
        return true;
    }

    std::u32string stru32;
    if (!UnicodeHelper::ConvertUTF8ToUTF32String(stru32, src)) {
        return false;
    }

    return ConvertUTF32ToASCII(out, stru32);
}

