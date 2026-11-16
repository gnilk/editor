//
// The encoding of a file's bytes (open-bugs #15): detected on load, written back on save. The buffer holds UTF-32,
// so loading is lossless whatever the encoding; saving can only fail for a character the encoding can't hold.
//

#ifndef GEDIT_TEXTENCODING_H
#define GEDIT_TEXTENCODING_H

#include <optional>
#include <string>
#include <string_view>

namespace gedit {
    class TextEncoding {
    public:
        enum class kEncoding {
            kUTF8,
            kUTF8BOM,       // UTF-8 with the EF BB BF mark first
            kUTF16LE,       // with its BOM
            kUTF16BE,       // with its BOM
            kCP1252,        // Windows-1252: Latin-1 plus 0x80-0x9f (euro, quotes, dashes)
            kLatin1,        // ISO-8859-1: each byte its own code point
        };
    public:
        // A BOM is exact; valid UTF-8 (all of it) is UTF-8; anything else is 'fallback' - single-byte encodings can't
        // be told apart from the bytes, so which one is a setting, not a guess
        static kEncoding Detect(std::string_view bytes, kEncoding fallback);
        // The text, without its BOM. Never fails: what isn't valid for the encoding becomes U+FFFD
        static std::u32string Decode(std::string_view bytes, kEncoding encoding);
        // Appends 'text' encoded, without a BOM. False when a character can't be written in 'encoding';
        // 'outIdxFailed' is its index in 'text' and 'out' is left as it was
        static bool Encode(std::u32string_view text, kEncoding encoding, std::string &out, size_t &outIdxFailed);
        static std::string_view ByteOrderMark(kEncoding encoding);
        // Every Unicode character fits
        static bool IsUnicode(kEncoding encoding);
        static const char *Name(kEncoding encoding);
        // Case-insensitive; the common aliases ("utf8", "windows-1252", "iso-8859-1", ...)
        static std::optional<kEncoding> FromName(std::string_view name);
    };
}

#endif //GEDIT_TEXTENCODING_H
