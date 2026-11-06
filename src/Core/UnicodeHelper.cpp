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

// Convert a single UTF8 char to UFT32 and adds it to the u32string.
// Returns the number of bytes consumed from the UTF8 string
// -1 on error..
int UnicodeHelper::ConvertUTF8ToUTF32Char(std::u32string &out, const uint8_t *utf8Data, size_t maxBytes) {
    if (utf8Data == nullptr) {
        return -1;
    }
    const UTF8 *srcStart = reinterpret_cast<const UTF8 *>(utf8Data);
    const UTF8 *srcEnd = &srcStart[maxBytes];

    if (out.capacity() < (out.length() + 1)) {
        out.resize(out.capacity()+1);
    }

    char32_t dst[2] = {};

    auto result = ::ConvertSingleCodePointUTF8toUTF32(&srcStart, srcEnd, reinterpret_cast<UTF32 *>(&dst[0]), strictConversion);
    if (result != ConversionResult::conversionOK) {
        return -1;
    }
    out += dst[0];
    auto diff = srcStart - utf8Data;
    return static_cast<int>(diff);
}

// FIXME: Can be consolidated with the other
bool UnicodeHelper::ConvertUTF8ToUTF32String(std::u32string &out, const std::u8string &src) {
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

// A file that isn't UTF-8 (Latin-1 comments are common in old C) would stop the decoder at the first bad
// byte and lose the rest of the file. Identifiers are ASCII, so each bad byte just becomes a '?'.
void UnicodeHelper::ReplaceInvalidUtf8(std::string &data) {
    size_t idx = 0;
    while (idx < data.size()) {
        auto ch = (unsigned char)data[idx];
        size_t length = (ch < 0x80) ? 1 : ((ch >= 0xc2) && (ch <= 0xdf)) ? 2 : ((ch & 0xf0) == 0xe0) ? 3 : ((ch >= 0xf0) && (ch <= 0xf4)) ? 4 : 0;
        bool isValid = (length > 0) && ((idx + length) <= data.size());
        for (size_t i = 1; isValid && (i < length); i++) {
            isValid = ((unsigned char)data[idx + i] & 0xc0) == 0x80;
        }
        // Overlong forms, surrogates and past U+10FFFF
        if (isValid && (length > 2)) {
            auto second = (unsigned char)data[idx + 1];
            isValid = !(((ch == 0xe0) && (second < 0xa0)) || ((ch == 0xed) && (second > 0x9f)) ||
                        ((ch == 0xf0) && (second < 0x90)) || ((ch == 0xf4) && (second > 0x8f)));
        }
        if (isValid) {
            idx += length;
        } else {
            data[idx] = '?';
            idx++;
        }
    }
}
