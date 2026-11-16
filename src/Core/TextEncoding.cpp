//
// The encoding of a file's bytes (open-bugs #15)
//

#include <algorithm>
#include <array>
#include <cctype>

#include "TextEncoding.h"

using namespace gedit;

namespace {
    using kEncoding = TextEncoding::kEncoding;

    // cp1252's 0x80-0x9f. The five bytes it leaves undefined (0x81, 0x8d, 0x8f, 0x90, 0x9d) stay their C1 code point,
    // as Windows maps them, so every byte decodes - and encodes back to itself
    constexpr std::array<char32_t, 32> cp1252High = {
        U'€', U'\u0081', U'‚', U'ƒ', U'„', U'…', U'†', U'‡',
        U'ˆ', U'‰', U'Š', U'‹', U'Œ', U'\u008d', U'Ž', U'\u008f',
        U'\u0090', U'‘', U'’', U'“', U'”', U'•', U'–', U'—',
        U'˜', U'™', U'š', U'›', U'œ', U'\u009d', U'ž', U'Ÿ',
    };

    constexpr char32_t replacementChar = U'�';

    // The length of the valid UTF-8 sequence at 'idx', 0 when it isn't one (overlong, surrogate, past U+10FFFF, cut)
    size_t Utf8SequenceLength(std::string_view bytes, size_t idx) {
        auto ch = (unsigned char)bytes[idx];
        size_t length = (ch < 0x80) ? 1 : ((ch >= 0xc2) && (ch <= 0xdf)) ? 2 : ((ch & 0xf0) == 0xe0) ? 3 : ((ch >= 0xf0) && (ch <= 0xf4)) ? 4 : 0;
        if ((length == 0) || ((idx + length) > bytes.size())) {
            return 0;
        }
        for (size_t i = 1; i < length; i++) {
            if (((unsigned char)bytes[idx + i] & 0xc0) != 0x80) {
                return 0;
            }
        }
        if (length > 2) {
            auto second = (unsigned char)bytes[idx + 1];
            bool isOverlong = ((ch == 0xe0) && (second < 0xa0)) || ((ch == 0xf0) && (second < 0x90));
            bool isSurrogate = (ch == 0xed) && (second >= 0xa0);
            bool isPastMax = (ch == 0xf4) && (second >= 0x90);
            if (isOverlong || isSurrogate || isPastMax) {
                return 0;
            }
        }
        return length;
    }

    bool IsValidUtf8(std::string_view bytes) {
        size_t idx = 0;
        while (idx < bytes.size()) {
            auto length = Utf8SequenceLength(bytes, idx);
            if (length == 0) {
                return false;
            }
            idx += length;
        }
        return true;
    }

    std::u32string DecodeUtf8(std::string_view bytes) {
        std::u32string text;
        text.reserve(bytes.size());
        size_t idx = 0;
        while (idx < bytes.size()) {
            auto length = Utf8SequenceLength(bytes, idx);
            if (length == 0) {
                text.push_back(replacementChar);
                idx++;
                continue;
            }
            auto ch = (unsigned char)bytes[idx];
            char32_t codePoint = (length == 1) ? ch : (length == 2) ? (ch & 0x1f) : (length == 3) ? (ch & 0x0f) : (ch & 0x07);
            for (size_t i = 1; i < length; i++) {
                codePoint = (codePoint << 6) | ((unsigned char)bytes[idx + i] & 0x3f);
            }
            text.push_back(codePoint);
            idx += length;
        }
        return text;
    }

    std::u32string DecodeUtf16(std::string_view bytes, bool isBigEndian) {
        std::u32string text;
        text.reserve(bytes.size() / 2);
        auto unitAt = [&](size_t idx) -> char32_t {
            auto first = (unsigned char)bytes[idx];
            auto second = (unsigned char)bytes[idx + 1];
            return isBigEndian ? ((first << 8) | second) : ((second << 8) | first);
        };
        size_t idx = 0;
        while ((idx + 1) < bytes.size()) {
            auto unit = unitAt(idx);
            idx += 2;
            if ((unit >= 0xd800) && (unit <= 0xdbff) && ((idx + 1) < bytes.size())) {
                auto low = unitAt(idx);
                if ((low >= 0xdc00) && (low <= 0xdfff)) {
                    text.push_back(0x10000 + ((unit - 0xd800) << 10) + (low - 0xdc00));
                    idx += 2;
                    continue;
                }
            }
            bool isSurrogate = (unit >= 0xd800) && (unit <= 0xdfff);
            text.push_back(isSurrogate ? replacementChar : unit);
        }
        // An odd byte at the end
        if (idx < bytes.size()) {
            text.push_back(replacementChar);
        }
        return text;
    }

    void AppendUtf8(char32_t ch, std::string &out) {
        if (ch < 0x80) {
            out.push_back((char)ch);
        } else if (ch < 0x800) {
            out.push_back((char)(0xc0 | (ch >> 6)));
            out.push_back((char)(0x80 | (ch & 0x3f)));
        } else if (ch < 0x10000) {
            out.push_back((char)(0xe0 | (ch >> 12)));
            out.push_back((char)(0x80 | ((ch >> 6) & 0x3f)));
            out.push_back((char)(0x80 | (ch & 0x3f)));
        } else {
            out.push_back((char)(0xf0 | (ch >> 18)));
            out.push_back((char)(0x80 | ((ch >> 12) & 0x3f)));
            out.push_back((char)(0x80 | ((ch >> 6) & 0x3f)));
            out.push_back((char)(0x80 | (ch & 0x3f)));
        }
    }

    void AppendUtf16Unit(char32_t unit, bool isBigEndian, std::string &out) {
        auto high = (char)((unit >> 8) & 0xff);
        auto low = (char)(unit & 0xff);
        out.push_back(isBigEndian ? high : low);
        out.push_back(isBigEndian ? low : high);
    }

    std::optional<uint8_t> ToCP1252(char32_t ch) {
        if ((ch < 0x80) || ((ch >= 0xa0) && (ch <= 0xff))) {
            return (uint8_t)ch;
        }
        auto it = std::find(cp1252High.begin(), cp1252High.end(), ch);
        if (it == cp1252High.end()) {
            return std::nullopt;
        }
        return (uint8_t)(0x80 + (it - cp1252High.begin()));
    }

    // Unicode code points only: not a surrogate, not past U+10FFFF
    bool IsCodePoint(char32_t ch) {
        return (ch <= 0x10ffff) && !((ch >= 0xd800) && (ch <= 0xdfff));
    }
}

kEncoding TextEncoding::Detect(std::string_view bytes, kEncoding fallback) {
    if (bytes.starts_with("\xef\xbb\xbf")) {
        return kEncoding::kUTF8BOM;
    }
    if (bytes.starts_with("\xff\xfe")) {
        return kEncoding::kUTF16LE;
    }
    if (bytes.starts_with("\xfe\xff")) {
        return kEncoding::kUTF16BE;
    }
    return IsValidUtf8(bytes) ? kEncoding::kUTF8 : fallback;
}

std::u32string TextEncoding::Decode(std::string_view bytes, kEncoding encoding) {
    auto mark = ByteOrderMark(encoding);
    if (!mark.empty() && bytes.starts_with(mark)) {
        bytes.remove_prefix(mark.size());
    }
    switch (encoding) {
        case kEncoding::kUTF8 :
        case kEncoding::kUTF8BOM :
            return DecodeUtf8(bytes);
        case kEncoding::kUTF16LE :
            return DecodeUtf16(bytes, false);
        case kEncoding::kUTF16BE :
            return DecodeUtf16(bytes, true);
        case kEncoding::kCP1252 :
        case kEncoding::kLatin1 : {
            std::u32string text;
            text.reserve(bytes.size());
            for (auto byte : bytes) {
                auto ch = (unsigned char)byte;
                bool isHigh = (encoding == kEncoding::kCP1252) && (ch >= 0x80) && (ch <= 0x9f);
                text.push_back(isHigh ? cp1252High[ch - 0x80] : ch);
            }
            return text;
        }
    }
    return {};
}

bool TextEncoding::Encode(std::u32string_view text, kEncoding encoding, std::string &out, size_t &outIdxFailed) {
    auto sizeBefore = out.size();
    for (size_t idx = 0; idx < text.size(); idx++) {
        auto ch = text[idx];
        bool isEncoded = IsCodePoint(ch);
        if (isEncoded) {
            switch (encoding) {
                case kEncoding::kUTF8 :
                case kEncoding::kUTF8BOM :
                    AppendUtf8(ch, out);
                    break;
                case kEncoding::kUTF16LE :
                case kEncoding::kUTF16BE : {
                    bool isBigEndian = encoding == kEncoding::kUTF16BE;
                    if (ch >= 0x10000) {
                        AppendUtf16Unit(0xd800 + ((ch - 0x10000) >> 10), isBigEndian, out);
                        AppendUtf16Unit(0xdc00 + ((ch - 0x10000) & 0x3ff), isBigEndian, out);
                    } else {
                        AppendUtf16Unit(ch, isBigEndian, out);
                    }
                    break;
                }
                case kEncoding::kCP1252 : {
                    auto byte = ToCP1252(ch);
                    isEncoded = byte.has_value();
                    if (isEncoded) {
                        out.push_back((char)*byte);
                    }
                    break;
                }
                case kEncoding::kLatin1 :
                    isEncoded = ch <= 0xff;
                    if (isEncoded) {
                        out.push_back((char)ch);
                    }
                    break;
            }
        }
        if (!isEncoded) {
            out.resize(sizeBefore);
            outIdxFailed = idx;
            return false;
        }
    }
    return true;
}

std::string_view TextEncoding::ByteOrderMark(kEncoding encoding) {
    switch (encoding) {
        case kEncoding::kUTF8BOM :
            return "\xef\xbb\xbf";
        case kEncoding::kUTF16LE :
            return "\xff\xfe";
        case kEncoding::kUTF16BE :
            return "\xfe\xff";
        default :
            return {};
    }
}

bool TextEncoding::IsUnicode(kEncoding encoding) {
    return (encoding != kEncoding::kCP1252) && (encoding != kEncoding::kLatin1);
}

const char *TextEncoding::Name(kEncoding encoding) {
    switch (encoding) {
        case kEncoding::kUTF8 :
            return "utf-8";
        case kEncoding::kUTF8BOM :
            return "utf-8-bom";
        case kEncoding::kUTF16LE :
            return "utf-16le";
        case kEncoding::kUTF16BE :
            return "utf-16be";
        case kEncoding::kCP1252 :
            return "cp1252";
        case kEncoding::kLatin1 :
            return "latin1";
    }
    return "utf-8";
}

std::optional<kEncoding> TextEncoding::FromName(std::string_view name) {
    std::string key;
    for (auto ch : name) {
        if ((ch != '-') && (ch != '_')) {
            key.push_back((char)std::tolower((unsigned char)ch));
        }
    }
    if (key == "utf8") {
        return kEncoding::kUTF8;
    }
    if ((key == "utf8bom") || (key == "utf8sig")) {
        return kEncoding::kUTF8BOM;
    }
    if (key == "utf16le") {
        return kEncoding::kUTF16LE;
    }
    if (key == "utf16be") {
        return kEncoding::kUTF16BE;
    }
    if ((key == "cp1252") || (key == "windows1252")) {
        return kEncoding::kCP1252;
    }
    if ((key == "latin1") || (key == "iso88591")) {
        return kEncoding::kLatin1;
    }
    return std::nullopt;
}
