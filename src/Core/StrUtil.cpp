//
// Created by gnilk on 14.01.23.
//

#include <string>
#include "Core/StrUtil.h"

namespace strutil {
    std::string &rtrim(std::string &s, const std::string &chars /* = "\t\n\v\f\r " */) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [chars](unsigned char ch) {
            return !std::strchr(chars.c_str(), ch);
        }).base(), s.end());
        return s;
    }
}
