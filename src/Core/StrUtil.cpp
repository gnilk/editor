//
// Created by gnilk on 14.01.23.
//

#include <vector>
#include <string>
#include "Core/StrUtil.h"

namespace strutil {
    std::string &ltrim(std::string &s, const std::string &chars /* = "\t\n\v\f\r " */) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [chars](unsigned char ch) {
            return !std::strchr(chars.c_str(), ch);
            // Note: Can't use 'isspace' as some routines depends on us being able to provide a custom string...
            //return !std::isspace(ch);
        }));
        return s;
    }


    std::string &rtrim(std::string &s, const std::string &chars /* = "\t\n\v\f\r " */) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [chars](unsigned char ch) {
            return !std::strchr(chars.c_str(), ch);
        }).base(), s.end());
        return s;
    }

    std::string &trim(std::string &str, const std::string &chars /* = "\t\n\v\f\r " */) {
        return ltrim(rtrim(str, chars), chars);
    }

    void split(std::vector<std::string> &strings, const char *strInput, int splitChar) {
        std::string input(strInput);
        size_t iPos = 0;
        while (iPos != std::string::npos) {
            size_t iStart = iPos;
            iPos = input.find(splitChar, iPos);
            if (iPos != std::string::npos) {
                std::string str = input.substr(iStart, iPos - iStart);
                trim(str);
                // 2018-11-29, Gnilk: Push back even if length is 0
                strings.push_back(str);
                iPos++;
            } else {
                std::string str = input.substr(iStart, input.length() - iStart);
                trim(str);
                if (str.length() > 0) {
                    strings.push_back(str);
                }
            }
        }
    }

}
