//
// Created by gnilk on 09.09.23.
//

#ifndef SDLAPP_UNICODEHELPER_H
#define SDLAPP_UNICODEHELPER_H

#include <string>

namespace gedit {
    class UnicodeHelper {
    public:
        static int ConvertUTF8ToUTF32Char(std::u32string &out, const uint8_t *ptrData, size_t maxBytes);

        static bool ConvertUTF8ToUTF32String(std::u32string &out, const std::string &src);
        static bool ConvertUTF8ToUTF32String(std::u32string &out, const std::u8string &src);
        static bool ConvertUTF8ToASCII(std::string &out, const std::string &src);
        static bool ConvertUTF32ToUTF8String(std::string &out, const std::u32string &src);
        static bool ConvertUTF32ToUTF8String(std::u8string &out, const std::u32string &src);
        static bool ConvertUTF32ToASCII(std::string &out, const std::u32string &src);


        static std::u32string utf8to32(const std::string &src);
        static std::string utf32to8(const std::u32string &src);
        static std::string utf8toascii(const std::string &src);
        static std::string utf8toascii(const char8_t *src);
        static std::string utf32toascii(const std::u32string &src);
    };
}


#endif //SDLAPP_UNICODEHELPER_H
