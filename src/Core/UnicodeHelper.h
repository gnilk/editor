//
// Created by gnilk on 09.09.23.
//

#ifndef SDLAPP_UNICODEHELPER_H
#define SDLAPP_UNICODEHELPER_H

#include <string>

namespace gedit {
    class UnicodeHelper {
    public:
        static bool ConvertUTF8ToUTF32String(std::u32string &out, const std::string &src);
        static bool ConvertUTF32ToUTF8String(std::string &out, const std::u32string &src);
        static bool ConvertUTF8ToASCII(std::string &out, const std::string &src);
    };
}


#endif //SDLAPP_UNICODEHELPER_H
